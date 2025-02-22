/*------------------------------------------------------------------
 *  Fred Juhlin 2024
 *------------------------------------------------------------------*/

#include <stdlib.h>
#include <glib.h>
#include <stdio.h> 
#include <string.h>
#include <syslog.h>
#include <unistd.h>
#include <dlfcn.h>
#include <dirent.h> 
#include "ACAP.h"
#include "MQTT.h"
#include "MQTTClient.h"
#include "CERTS.h"

#define LOG(fmt, args...)    { syslog(LOG_INFO, fmt, ## args);  printf(fmt, ## args); }
#define LOG_WARN(fmt, args...)    { syslog(LOG_WARNING, fmt, ## args); printf(fmt, ## args);}
//#define LOG_TRACE(fmt, args...)    { syslog(LOG_INFO, fmt, ## args); printf(fmt, ## args); }
#define LOG_TRACE(fmt, args...)    {}
#define URL_SIZE	256

int MQTT_LoadLib();
int mqttLibLoaded = 0;
static void	(*f_MQTTClient_global_init)(MQTTClient_init_options* inits);
static int	(*f_MQTTClient_create)(MQTTClient* handle, const char* serverURI, const char* clientId, int persistence_type, void* persistence_context) = 0;
static int	(*f_MQTTClient_setCallbacks)(MQTTClient handle,void* context, MQTTClient_connectionLost* cl, MQTTClient_messageArrived* ma, MQTTClient_deliveryComplete* dc) = 0;
static int	(*f_MQTTClient_connect)(MQTTClient handle, MQTTClient_connectOptions* options);
static int	(*f_MQTTClient_disconnect)(MQTTClient handle, int timeout) = 0;
static int	(*f_MQTTClient_isConnected)(MQTTClient handle) = 0;
static int	(*f_MQTTClient_subscribe)(MQTTClient handle, const char* topic, int qos) = 0;
static int	(*f_MQTTClient_unsubscribe)(MQTTClient handle, const char* topic) = 0;
static int	(*f_MQTTClient_publish)(MQTTClient handle, const char* topicName, int payloadlen, void* payload, int qos, int retained,MQTTClient_deliveryToken* dt) = 0;
static int	(*f_MQTTClient_publishMessage)(MQTTClient handle, const char* topicName, MQTTClient_message* msg, MQTTClient_deliveryToken* dt) = 0;
static void	(*f_MQTTClient_yield)(void) = 0;
static void	(*f_MQTTClient_freeMessage)(MQTTClient_message** msg);
static void	(*f_MQTTClient_free)(void* ptr);
static void	(*f_MQTTClient_destroy)(MQTTClient* handle);


MQTTClient client = 0;
MQTTClient_init_options mqttInit = MQTTClient_init_options_initializer;
MQTTClient_connectOptions mqttOptions = MQTTClient_connectOptions_initializer;
MQTTClient_willOptions LastWill = MQTTClient_willOptions_initializer;
MQTTClient_SSLOptions sslOptions = MQTTClient_SSLOptions_initializer;
volatile MQTTClient_deliveryToken MQTT_Deliveredtoken;

cJSON* MQTTSettings = 0;
cJSON* MQTT_Subscriptions = 0;
MQTT_Callback_Connection MQTT_ACAP_Connection_Callback = 0;
MQTT_Callback_Message MQTT_ACAP_Message_Callback = 0;
int MQTT_Busy = 0;
cJSON* MQTT_Buffer = 0;
int	MQTT_Current_Buffer_Size = 0;
#define MQTT_MAX_BUFFER_SIZE	50000
char LastWillTopic[64];
char LastWillMessage[512];


MQTT_Callback_Message MQTT_Subscription_Callbacks[20];
char MQTT_Subscription_Topics[20][30];

//Function declarations
int 	MQTT_Message_Recived(void *context, char *topic, int topicLen, MQTTClient_message *MQTTMessage);
void	MQTT_Disconnected(void *context, char *cause);
void	MQTT_Delivered(void *context, MQTTClient_deliveryToken dt);
static void MQTT_HTTP_callback(const ACAP_HTTP_Response response,const ACAP_HTTP_Request request);
int     MQTT_Set( cJSON* settings );
static  gboolean MQTT_Timer();
int		MQTT_Connected();


// Add at the top of the file
static time_t lastActivityTime = 0;

// Update activity time on any message send/receive
void UpdateActivityTime() {
    lastActivityTime = time(NULL);
}

cJSON*
MQTT_Settings() {
	if( !MQTTSettings )
		MQTTSettings = cJSON_CreateObject();
	return  MQTTSettings;
}

static gboolean MQTT_Yield_Handler() {
    if (MQTT_Connected()) {
        (*f_MQTTClient_yield)();
        UpdateActivityTime();
    }
    return TRUE;  // Continue the timer
}

int
MQTT_Init( const char* acapname, MQTT_Callback_Connection callback ) {
	LOG_TRACE("%s: Entry\n",__func__);
	ACAP_STATUS_SetString("mqtt", "status", "Not initialized" );
	ACAP_STATUS_SetBool("mqtt", "connected", FALSE );
	ACAP_STATUS_SetBool("mqtt", "connecting", FALSE );	
	if(!callback) {
		LOG_WARN("%s: Invalid callback\n",__func__);
		return 0;
	}

	ACAP_HTTP_Node( "mqtt", MQTT_HTTP_callback );
	CERTS_Init();

	MQTT_ACAP_Connection_Callback = callback;	

	MQTT_LoadLib();
	LOG_TRACE("%s: Library loaded\n",__func__);
	if( callback == 0 ) {
		LOG_WARN("MQTT_Init: Invalid input\n");
		return 0;
	}
	mqttInit.do_openssl_init = 1;

	if( f_MQTTClient_global_init ) {
		LOG_TRACE("%s: f_MQTTClient_global_init\n",__func__);
		(*f_MQTTClient_global_init)(&mqttInit);
		LOG_TRACE("%s: f_MQTTClient_global_init: Done\n",__func__);
	}
	
	if( !MQTTSettings ) {
		MQTTSettings = ACAP_FILE_Read( "settings/mqtt.json" );
		if(!MQTTSettings) {
			LOG_WARN("%s: Unable to parse default settings\n",__func__);
			return 0;
		}

		cJSON* savedSettings = ACAP_FILE_Read( "localdata/mqtt.json" );
		LOG_TRACE("%s: File read: Settings: %s\n",__func__,savedSettings?"OK":"Failed");
		if( savedSettings ) {
			LOG_TRACE("%s: Saved settings found\n", __func__);
			cJSON* prop = savedSettings->child;
			while(prop) {
				if( cJSON_GetObjectItem(MQTTSettings,prop->string ) )
					cJSON_ReplaceItemInObject(MQTTSettings,prop->string,cJSON_Duplicate(prop,1) );
				prop = prop->next;
			}
			cJSON_Delete(savedSettings);
		}
		
		char topicString[32];
		sprintf( topicString,"%s-%s",acapname,ACAP_DEVICE_Prop("serial"));
		cJSON_ReplaceItemInObject(MQTTSettings,"clientID",cJSON_CreateString(topicString));
	}

	cJSON* clientID = cJSON_GetObjectItem(MQTTSettings,"clientID");
	if( !clientID) {
		char newClientID[30];
		sprintf(newClientID,"acap-%s", ACAP_DEVICE_Prop("serial"));
		cJSON_AddStringToObject(MQTTSettings,"clientID", newClientID );
	}
	if( strlen(cJSON_GetObjectItem(MQTTSettings,"clientID")->valuestring) < 5 ) {
		char newClientID[30];
		sprintf(newClientID,"acap-%s", ACAP_DEVICE_Prop("serial"));
		cJSON_ReplaceItemInObject(MQTTSettings,"clientID", cJSON_CreateString(newClientID) );
	}
	
	for( int i = 0; i < 20; i++ ) {
		MQTT_Subscription_Callbacks[i] = 0;
		MQTT_Subscription_Topics[i][0] = 0;
	}

	MQTT_Buffer = cJSON_CreateArray();
	cJSON* settings = cJSON_Duplicate(MQTTSettings,1);
	if( MQTT_Set(settings) )
		MQTT_Connect();
	cJSON_Delete(settings);
	g_timeout_add_seconds( 30, MQTT_Timer, NULL );
	g_timeout_add(100, MQTT_Yield_Handler, NULL);
	ACAP_STATUS_SetString("mqtt", "status", "Not connected" );
	LOG_TRACE("%s: Exit\n",__func__);
	return 1;
}

void   MQTT_Cleanup() {
	MQTT_Disconnect();	
}

static void
MQTT_HTTP_callback(const ACAP_HTTP_Response response, const ACAP_HTTP_Request request) {
    if (!MQTTSettings) {
        ACAP_HTTP_Respond_Error(response, 500, "Invalid settings");
        LOG_WARN("%s: Invalid settings not initialized\n", __func__);
        return;
    }
	
	LOG_TRACE("%s:\n",__func__);

	const char* json = ACAP_HTTP_Request_Param(request, "json");
	if(!json)
		json = ACAP_HTTP_Request_Param(request, "set");

    if (!json) {
		const char* action = ACAP_HTTP_Request_Param(request, "action");
		if( action ) {
			cJSON_ReplaceItemInObject(MQTTSettings,"connected",cJSON_CreateFalse());
			ACAP_FILE_Write( "localdata/mqtt.json", MQTTSettings );
			if( MQTT_Disconnect() )
				ACAP_HTTP_Respond_Text( response, "OK" );
			else
				ACAP_HTTP_Respond_Error( response, 400, ACAP_STATUS_String("mqtt","status" ));
			return;
		}
		
        ACAP_HTTP_Respond_JSON(response, MQTTSettings);
        return;
    }

    cJSON* settings = cJSON_Parse(json);
    if (!settings) {
        ACAP_HTTP_Respond_Error(response, 400, "Invalid JSON");
        LOG_WARN("Unable to parse json for MQTT settings\n");
        return;
    }

	cJSON* payload = cJSON_GetObjectItem(settings,"payload");

	if( payload ) {  //Just update the helper pyaload properties
		const char* name = cJSON_GetObjectItem(payload,"name")?cJSON_GetObjectItem(payload,"name")->valuestring:"";
		const char* location = cJSON_GetObjectItem(payload,"location")?cJSON_GetObjectItem(payload,"location")->valuestring:"";
		cJSON* mqttPayload = cJSON_GetObjectItem(MQTTSettings,"payload");
		if(!mqttPayload) {
			mqttPayload = cJSON_CreateObject();
			cJSON_AddStringToObject( payload,"name",name);
			cJSON_AddStringToObject( payload,"location",location);
			cJSON_AddItemToObject(MQTTSettings,"payload",mqttPayload);
		}
		cJSON_ReplaceItemInObject(mqttPayload,"name",cJSON_CreateString(name));	
		cJSON_ReplaceItemInObject(mqttPayload,"location",cJSON_CreateString(location));	
		ACAP_FILE_Write( "localdata/mqtt.json", MQTTSettings );
		ACAP_HTTP_Respond_Text(response,"Payload properties updated");
		return;
	}

    LOG_TRACE("%s: %s\n", __func__, json);
        
    if (!MQTT_Set(settings)) {
        ACAP_HTTP_Respond_Error(response, 400, ACAP_STATUS_String("mqtt", "status"));
        LOG_WARN("Unable to update MQTT settings\n");
        cJSON_Delete(settings);
		return;
	}

    cJSON_Delete(settings);
    ACAP_HTTP_Respond_Text(response, "MQTT Updated");
    MQTT_Connect();
}

int
MQTT_Set( cJSON* settings ) {
	LOG_TRACE("%s: Entry\n",__func__);
	
	if(!settings) {
		LOG_WARN("%s: Invalid settings\n",__func__);
		return 0;
	}

	char *json = cJSON_PrintUnformatted(settings);
	if(json){
		LOG_TRACE("%s: Input: %s",__func__,json);
		free(json);
	}

	if(!mqttLibLoaded) {
		LOG_WARN("%s: MQTT Lib not loaded\n",__func__);
		ACAP_STATUS_SetString("mqtt", "status","Internal error: No MQTT lib" );
		return 0;
	}
	
	if( client ) {
		LOG_TRACE("%s: MQTT_Client: Previous client exists\n",__func__);
		if((*f_MQTTClient_isConnected)(client) ) {
			LOG("Disconnecting current MQTT connection\n");
			(*f_MQTTClient_disconnect)(client, 1000);
		}
		LOG_TRACE("%s: Removing previous client\n",__func__);
		(*f_MQTTClient_destroy)(&client);
		ACAP_STATUS_SetString("mqtt", "status","Disconnected" );
		client = 0;
	}
	
	if( !MQTTSettings ) {
		LOG_WARN("%s: MQTT Settings not initialized\n",__func__);
		return 0;
	}

	ACAP_STATUS_SetBool("mqtt", "connected", FALSE );
	ACAP_STATUS_SetBool("mqtt", "connecting", TRUE );
	ACAP_STATUS_SetString("mqtt", "status","Connecting" );

	cJSON* setting = settings->child;
	while( setting ) {
		if( cJSON_GetObjectItem(MQTTSettings,setting->string ) )
			cJSON_ReplaceItemInObject(MQTTSettings,setting->string,cJSON_Duplicate(setting,1) );
		setting = setting->next;
	}
	json = cJSON_PrintUnformatted(MQTTSettings);
	if(json){
		LOG_TRACE("%s: Connect: %s",__func__,json);
		free(json);
	}
	
	ACAP_FILE_Write( "localdata/mqtt.json", MQTTSettings );
	const char *address = cJSON_GetObjectItem(MQTTSettings,"address")?cJSON_GetObjectItem(MQTTSettings,"address")->valuestring:0;
	const char *port = cJSON_GetObjectItem(MQTTSettings,"port")?cJSON_GetObjectItem(MQTTSettings,"port")->valuestring:"1883";
	
	if( address == 0 ) {
		LOG_WARN("%s: Invalid MQTT address",__func__);
		ACAP_STATUS_SetString("mqtt", "status","Disconnected." );
		return 0;
	}

	char *clientID = cJSON_GetObjectItem(MQTTSettings,"clientID")?cJSON_GetObjectItem(MQTTSettings,"clientID")->valuestring:0;
	if( clientID == 0 || strlen(clientID) < 2) {
		LOG_WARN("%s: Invalid MQTT Client ID",__func__);
		ACAP_STATUS_SetString("mqtt", "status","Disconnected." );
		return 0;
	}
	LOG_TRACE("%s: ClientID = %s\n",__func__,clientID);
	int tls = cJSON_GetObjectItem(MQTTSettings,"tls")?cJSON_GetObjectItem(MQTTSettings,"tls")->type==cJSON_True:0;

	if( strlen(address) < 4 || strlen(clientID) < 2 ) {
		ACAP_STATUS_SetString("mqtt", "status","Disconnected" );
		return 0;
	}

	char url[URL_SIZE];
	size_t required_size;

	// Check input parameters
	if (!address || !port) {
		return 0;
	}

	// Calculate required size: protocol(7 or 6) + address + ':' + port + null terminator
	required_size = (tls ? 7 : 6) + strlen(address) + 1 + strlen(port) + 1;

	if (required_size > URL_SIZE) {
		LOG_WARN("URL too long: need %zu bytes, have %d\n", required_size, URL_SIZE);
		return 0;
	}

	// Now safe to format
	int written = tls ? 
		snprintf(url, URL_SIZE, "ssl://%s:%s", address, port) :
		snprintf(url, URL_SIZE, "tcp://%s:%s", address, port);

	if (written < 0 || written >= URL_SIZE) {
		LOG_WARN("URL formatting failed\n");
		return 0;
	}

	LOG_TRACE("%s: Address = %s\n",__func__,url);
	
	int result = (*f_MQTTClient_create)(&client, url, clientID, MQTTCLIENT_PERSISTENCE_NONE, NULL);
	if( result != MQTTCLIENT_SUCCESS || client == 0 ) {
		LOG_WARN("Could not create client.  Error code %d\n",result);
		ACAP_STATUS_SetString("mqtt","status", "Internal client error." );
		client = 0;
		return 0;
	}
	LOG_TRACE("%s: Client created\n",__func__);

	result = (*f_MQTTClient_setCallbacks)(client, 0, MQTT_Disconnected, MQTT_Message_Recived, MQTT_Delivered);
	if( result != MQTTCLIENT_SUCCESS ) {
		ACAP_STATUS_SetString("mqtt","status", "Internal client callback error." );
		LOG_WARN("%s: Unable to set MQTT callbacks\n",__func__);
		return 0;
	}
	LOG_TRACE("%s: Exit\n",__func__);
	return 1;
}

int
MQTT_Connected() {
	if( client == 0 || !MQTTSettings )
		return 0;
	return (*f_MQTTClient_isConnected)(client);
}

int
MQTT_Connect() {
	LOG_TRACE("%s: Entry\n",__func__);
	
	if( !client || !MQTTSettings ) {
		ACAP_STATUS_SetBool("mqtt","connected",FALSE );
		ACAP_STATUS_SetBool("mqtt","connecting",FALSE );
		ACAP_STATUS_SetString("mqtt","status", "Client not initialized" );
		LOG_WARN("Connection failed.  Client is not configured\n");
		return 0;
	}


	if( (*f_MQTTClient_isConnected)(client) ) {
		return 1;
	}

	ACAP_STATUS_SetBool("mqtt","connected", FALSE );
	ACAP_STATUS_SetBool("mqtt","connecting", TRUE );
	ACAP_STATUS_SetString("mqtt","status", "Connecting" );
	
	const char *user = cJSON_GetObjectItem(MQTTSettings,"user")?cJSON_GetObjectItem(MQTTSettings,"user")->valuestring:0;
	const char *password = cJSON_GetObjectItem(MQTTSettings,"password")?cJSON_GetObjectItem(MQTTSettings,"password")->valuestring:0;
	int tls = cJSON_GetObjectItem(MQTTSettings,"tls")?cJSON_GetObjectItem(MQTTSettings,"tls")->type==cJSON_True:0;
	int validateCertificate = cJSON_GetObjectItem(MQTTSettings,"validateCertificate")?cJSON_GetObjectItem(MQTTSettings,"validateCertificate")->type==cJSON_True:0;

	mqttOptions.keepAliveInterval = 60;
	
	if( user && strlen(user) > 0 && password && strlen(password) > 0 ) {
//		LOG_TRACE("MQTT password authentication\n");
		mqttOptions.username = user;  
		mqttOptions.password = password;  
	}
	
	char *preTopic = cJSON_GetObjectItem(MQTTSettings,"preTopic") ? cJSON_GetObjectItem(MQTTSettings,"preTopic")->valuestring:0;
	if( preTopic && strlen(preTopic) )
		sprintf(LastWillTopic,"%s/connect/%s",preTopic,ACAP_DEVICE_Prop("serial"));
	else
		sprintf(LastWillTopic,"connect/%s",ACAP_DEVICE_Prop("serial"));
	sprintf(LastWillMessage,"{\"device\":\"%s\",\"connected\":false}",ACAP_DEVICE_Prop("serial"));
	
	LastWill.topicName = LastWillTopic;
	LastWill.message = LastWillMessage;
	LastWill.retained = 1;
	mqttOptions.will = &LastWill;


	sslOptions.keyStore = 0;
	sslOptions.privateKey = 0;
	mqttOptions.ssl = 0;

	if( tls ) {
		LOG_TRACE("Connecting MQTT over TLS\n");
		sslOptions.trustStore = CERTS_Get_CA();
		if( validateCertificate ) {
			sslOptions.enableServerCertAuth = 1;
			sslOptions.verify = 1;
		} else {
			sslOptions.enabledCipherSuites = "aNULL";
			sslOptions.verify = 0;
			sslOptions.enableServerCertAuth = 0;
		}
		//Use client certificate if avaialble
		sslOptions.keyStore = CERTS_Get_Cert();
		sslOptions.privateKey = CERTS_Get_Key();
		sslOptions.privateKeyPassword = CERTS_Get_Password();
		mqttOptions.ssl = &sslOptions;
	}

	LOG_TRACE("%s: Connecting...\n", __func__);
	int status = (*f_MQTTClient_connect)(client, &mqttOptions);
	LOG_TRACE("%s: Connection status %d\n", __func__,status);
	char errorString[128];

	switch( status ) {
		case MQTTCLIENT_SUCCESS:
			LOG("MQTT connected to %s as %s\n",cJSON_GetObjectItem(MQTTSettings,"address")->valuestring, cJSON_GetObjectItem(MQTTSettings,"clientID")->valuestring);
			ACAP_STATUS_SetString("mqtt","status", "Connected" );
			ACAP_STATUS_SetBool("mqtt","connected", TRUE );
			ACAP_STATUS_SetBool("mqtt","connecting", FALSE );
			cJSON_ReplaceItemInObject(MQTTSettings,"connect", cJSON_CreateTrue() );
			ACAP_FILE_Write( "localdata/mqtt.json", MQTTSettings );
			MQTT_ACAP_Connection_Callback( MQTT_CONNECT);
/*	
			cJSON* subscription = MQTT_Subscriptions?MQTT_Subscriptions->child:0;
			while( subscription ) {
				LOG("%s: TODO - Add subscription\n",__func__);
				subscription = subscription->next;
			}
*/
			return 1;
		case MQTTCLIENT_FAILURE:
			ACAP_STATUS_SetString("mqtt","status", "Failed to connect" );
			ACAP_STATUS_SetBool("mqtt","connected", FALSE );
			ACAP_STATUS_SetBool("mqtt","connecting", FALSE );
			LOG_WARN("MQTT Client failed to connect\n");
			break;
		case MQTTCLIENT_DISCONNECTED:
			LOG_WARN("MQTT Client disconnected\n");
			ACAP_STATUS_SetString("mqtt","status", "Disconnected" );
			ACAP_STATUS_SetBool("mqtt","connected", FALSE );
			ACAP_STATUS_SetBool("mqtt","connecting", FALSE );
			MQTT_ACAP_Connection_Callback( MQTT_DISCONNECT ) ;			
			break;
		case 1:
			ACAP_STATUS_SetString("mqtt","status", "Invalid protocol version" );
			ACAP_STATUS_SetBool("mqtt","connected", FALSE );
			ACAP_STATUS_SetBool("mqtt","connecting", FALSE );
			LOG_WARN("MQTT connection refused: Unacceptable protocol version\n");
			status = 6;
			break;
		case 2:
			ACAP_STATUS_SetString("mqtt","status", "Connection refused." );
			ACAP_STATUS_SetBool("mqtt","connected", FALSE );
			ACAP_STATUS_SetBool("mqtt","connecting", FALSE );
			LOG_WARN("MQTT connection refused: Identifier rejected\n");
			break;
		case 3:
			ACAP_STATUS_SetString("mqtt","status", "Broker is not responding" );
			ACAP_STATUS_SetBool("mqtt","connected", FALSE );
			ACAP_STATUS_SetBool("mqtt","connecting", FALSE );
			LOG_WARN("MQTT connection refused: Broker not responding (invalid address?)\n");
			break;
		case 4:
			ACAP_STATUS_SetString("mqtt","status", "Connection refused.  Bad user or password" );
			ACAP_STATUS_SetBool("mqtt","connected", FALSE );
			ACAP_STATUS_SetBool("mqtt","connecting", FALSE );
			break;
		case 5:
			ACAP_STATUS_SetString("mqtt","status", "Authorization failed" );
			ACAP_STATUS_SetBool("mqtt","connected", FALSE );
			ACAP_STATUS_SetBool("mqtt","connecting", FALSE );
			break;
		default:
			printf(errorString,"Failed to connect.  Error code %d");
			ACAP_STATUS_SetString("mqtt","status", errorString );
			ACAP_STATUS_SetBool("mqtt","connected", FALSE );
			ACAP_STATUS_SetBool("mqtt","connecting", FALSE );
			LOG_WARN("MQTT connection refused: Undefined error code %d\n", status);
			break;
	}
	LOG_TRACE("%s: Exit\n",__func__);
	return 0;
}

int
MQTT_Disconnect() {
	LOG_TRACE("%s: Entry\n",__func__);
	if(!client || !MQTTSettings )
		return 1;
	if( MQTT_Connected() == 0 )
		return 1;
	if((*f_MQTTClient_isConnected)(client) ) {
		LOG_WARN("MQTT Disconnecting\n");
		(*f_MQTTClient_disconnect)(client, 1000);
	}
	(*f_MQTTClient_destroy)(&client);
	ACAP_STATUS_SetString("mqtt", "status","Disconnected" );
	ACAP_STATUS_SetBool("mqtt", "connected", FALSE );
	ACAP_STATUS_SetBool("mqtt", "connecting", FALSE );
	client = 0;
	LOG_TRACE("%s: Exit\n",__func__);
	return 1;
}

const char *
MQTT_ClientID() {
	LOG_TRACE("%s: Entry\n",__func__);
	if( !MQTTSettings ) {
		LOG_WARN("MQTT_Client: Not initialized\n");
		return 0;
	}
	LOG_TRACE("%s: Exit\n",__func__);
	return cJSON_GetObjectItem( MQTTSettings,"clientID") ? cJSON_GetObjectItem( MQTTSettings,"clientID")->valuestring : "undefined";
}

int
MQTT_Publish( const char *topic, const char *payload, int qos, int retained ) {
	if( client == 0 ) {
//		LOG_TRACE("%s: Invalid client\n",__func__);
		return 0;
	}
	if( !(*f_MQTTClient_isConnected)(client) ) {
//		LOG_TRACE("%s: Client not connected\n",__func__);
		return 0;
	}
	
	if( !topic ) {
		LOG_WARN("%s: Invalid topic\n",__func__);
		return 0;
	}

	char fullTopic[256];
	char* preTopic = cJSON_GetObjectItem(MQTTSettings,"preTopic")?cJSON_GetObjectItem(MQTTSettings,"preTopic")->valuestring:"";
	if( strlen(preTopic) )
		sprintf(fullTopic,"%s/%s", preTopic, topic);
	else
		sprintf(fullTopic,"%s", topic);

	LOG_TRACE("%s: %s %s\n",__func__,fullTopic, payload);

	MQTTClient_message pubmsg = MQTTClient_message_initializer;
	MQTTClient_deliveryToken mqtt_token;

	pubmsg.payload = payload?(char *)payload:"";
	pubmsg.payloadlen = strlen(payload);
	pubmsg.qos = qos;
	pubmsg.retained = retained;

	int status = (*f_MQTTClient_publishMessage)(client, fullTopic, &pubmsg, &mqtt_token);
	if( status != MQTTCLIENT_SUCCESS ) {
		LOG_WARN("%s: Error code %d\n", __func__,status);
		return 0;
	} else {
		UpdateActivityTime();
	}
	return 1;
}

int
MQTT_Publish_JSON( const char *topic, cJSON* payload, int qos, int retained ) {
	LOG_TRACE("%s:\n",__func__);

	if( !topic || strlen(topic) == 0 ) {
		LOG_TRACE("%s: Invallid topic\n",__func__);
		return 0;
	}

	if( !payload ) {
		LOG_TRACE("%s: Invallid payload\n",__func__);
		return 0;
	}

	if( client == 0 ) {
//		LOG_TRACE("%s: Invalid client\n",__func__);
		return 0;
	}
	if( !(*f_MQTTClient_isConnected)(client) ) {
//		LOG_TRACE("%s: Client not connected\n",__func__);
		return 0;
	}
	
	cJSON* publish = cJSON_Duplicate(payload, 1);
	cJSON* helperProperties = cJSON_GetObjectItem(MQTTSettings,"payload");

	if( helperProperties ) {
		const char* name = cJSON_GetObjectItem(helperProperties,"name")?cJSON_GetObjectItem(helperProperties,"name")->valuestring:0;
		const char* location = cJSON_GetObjectItem(helperProperties,"location")?cJSON_GetObjectItem(helperProperties,"location")->valuestring:0;
		if( name && strlen(name) > 0 )
			cJSON_AddStringToObject(publish,"name",name);
		if( location && strlen(location) > 0 )
			cJSON_AddStringToObject(publish,"location",location);
		cJSON_AddStringToObject(publish,"serial",ACAP_DEVICE_Prop("serial"));
	}


	char *json = cJSON_PrintUnformatted(publish);
	if(!json) {
		LOG("%s: Unable to format JSON\n",__func__);
		LOG_TRACE("%s: Exit error\n",__func__);
		return 0;
	}
	cJSON_Delete(publish);
	
	int result = MQTT_Publish(topic, json, qos, retained );
	free(json);
	LOG_TRACE("%s: Exit\n",__func__);
	return result;
}

int
MQTT_Publish_Binary( const char *topic, int payloadlen, void *payload, int qos, int retained ) {
	char fullTopic[128];
	if( client == 0 || !(*f_MQTTClient_isConnected)(client) )
		return 0;

	if( !topic || !payload ) {
		LOG_WARN("MQTT_Publish: Invalid topic or payload is null\n");
		return 0;
	}

	MQTTClient_deliveryToken mqtt_token;
	char* preTopic = cJSON_GetObjectItem(MQTTSettings,"preTopic")?cJSON_GetObjectItem(MQTTSettings,"preTopic")->valuestring:"";
	if( strlen(preTopic) )
		sprintf(fullTopic,"%s/%s", preTopic, topic);
	else
		sprintf(fullTopic,"%s", topic);

	int status = (*f_MQTTClient_publish)(client, fullTopic, payloadlen, payload, qos, retained, &mqtt_token );
	if( status != MQTTCLIENT_SUCCESS ) {
		LOG_WARN("Cannot publish MQTT message (binary data).  Error code %d\n", status);
		return 0;
	}
	return status == MQTTCLIENT_SUCCESS;
}

void
MQTT_Delivered(void *context, MQTTClient_deliveryToken dt) {
  MQTT_Deliveredtoken = dt;
}

int
MQTT_Message_Recived(void *context, char *topic, int topicLen, MQTTClient_message *MQTTMessage) {
	LOG_TRACE("%s: Entry: Topic = %s\n",__func__,topic);
	(*f_MQTTClient_freeMessage)(&MQTTMessage);
	(*f_MQTTClient_free)(topic);
	LOG_TRACE("%s: Exit:\n",__func__);
	return 1;
}

int
MQTT_Subscribe( const char *topic, MQTT_Callback_Message callback ) {
	int status;
	LOG_TRACE("%s: Entry: Topic = %s\n",__func__,topic);
	if( !client || !f_MQTTClient_isConnected || !(*f_MQTTClient_isConnected)(client) ) {
		LOG_TRACE("Cannot subscribe to %s.  MQTT client not connected\n", topic);
		return 0;
	}
	
	if( !topic || strlen(topic) < 2 ) {
		LOG_WARN("MQTT_Subscribe: %s is invalid topic\n", topic?topic:"null" );
		return 0;
	}
	if( MQTTSettings == 0 ) {
		LOG_TRACE("MQTT_Subscribe: Client not initialized\n");
		return 0;
	}
	status = (*f_MQTTClient_subscribe)(client, topic, 2);
	if( status != MQTTCLIENT_SUCCESS) {
		LOG_WARN("MQTT Subscription failed\n");
		return 0;
	}
	LOG_TRACE("%s: Exit\n",__func__);
	return 0;
}

int
MQTT_Unsubscribe( const char *topic ) {
	if( !client || !f_MQTTClient_isConnected || !(*f_MQTTClient_isConnected)(client) ) {
		LOG_WARN("%s: Cannot subscribe to topic.  MQTT client not initialized\n",__func__);
		return 0;
	}
  
	if( !topic || strlen(topic) < 2 ) {
		LOG_WARN("%s: Unable to unsubscribe.  Invalid topic\n",__func__);
		return 0;
	}
	if( (*f_MQTTClient_unsubscribe)(client, topic) != MQTTCLIENT_SUCCESS ) {
		LOG_WARN("%s: Cannot unsubscribe\n",__func__);
		return 0;
	}
	return 1;
}


void
MQTT_Disconnected(void *context, char *cause) {
	LOG_WARN("MQTT client disconnected from broker: %s", cause?cause:"Unknown");
	ACAP_STATUS_SetString("mqtt","status", "Reconnecting" );
	ACAP_STATUS_SetBool("mqtt","connected", FALSE );
}


static gboolean MQTT_Timer() {
    if (!MQTT_Connected() && cJSON_GetObjectItem(MQTTSettings,"connect")->type == cJSON_True) {
        MQTT_Connect();
    }
    return TRUE;
}

int MQTT_LoadLib() {
	DIR *dir;
	struct dirent *ent;
	char *libFile = 0;
	void *handle;
	char *error;

//	LOG_TRACE("%s: Entry\n",__func__);
	ACAP_STATUS_SetString("mqtt","status", "Loading library" );

	if ((dir = opendir ("/usr/lib")) == NULL) {
		perror ("");
		LOG_WARN("%s: Could not open /usr/lib",__func__);
		return 0;
	}
//	LOG_TRACE("%s: Library opened",__func__);

	while ((ent = readdir (dir)) != NULL) {
		if(strstr(ent->d_name,"libpaho-mqtt3cs") != NULL) {
			libFile = ent->d_name;
			break;
		}
	}
	closedir (dir);
//	LOG_TRACE("%s: Directory read OK",__func__);

	handle = dlopen (libFile, RTLD_LAZY);
	if(!handle) {
		LOG_WARN("Unable to open lib file: %s",dlerror())
		return 0;
	}
	dlerror();    /* Clear any existing error */
	if ((error = dlerror()) != NULL)  {
		LOG_WARN("MQTT Lib setup error: %s",dlerror())
		return 0;
	}

	f_MQTTClient_global_init = dlsym(handle, "MQTTClient_global_init");
	if(!f_MQTTClient_global_init || (error = dlerror()) != NULL)  {
		LOG_WARN("Error f_MQTTClient_global_init %s",dlerror())
		return 0;
	}
	
	f_MQTTClient_create = dlsym(handle, "MQTTClient_create");
	if(!f_MQTTClient_create || (error = dlerror()) != NULL)  {
		LOG_WARN("Error f_MQTTClient_create %s",dlerror())
		return 0;
	}

	f_MQTTClient_setCallbacks = dlsym(handle, "MQTTClient_setCallbacks");
	if ((error = dlerror()) != NULL)  {
		LOG_WARN("Error f_MQTTClient_setCallbacks %s",dlerror())
		return 0;
	}
	
	f_MQTTClient_connect = dlsym(handle, "MQTTClient_connect");
	if ((error = dlerror()) != NULL)  {
		LOG_WARN("Error f_MQTTClient_connect %s",dlerror())
		return 0;
	}

	f_MQTTClient_disconnect = dlsym(handle, "MQTTClient_disconnect");
	if ((error = dlerror()) != NULL)  {
		LOG_WARN("Error f_MQTTClient_setCallbacks %s",dlerror())
		return 0;
	}

	f_MQTTClient_isConnected = dlsym(handle, "MQTTClient_isConnected");
	if ((error = dlerror()) != NULL)  {
		LOG_WARN("Error f_MQTTClient_isConnected %s",dlerror())
		return 0;
	}

	f_MQTTClient_subscribe = dlsym(handle, "MQTTClient_subscribe");
	if ((error = dlerror()) != NULL)  {
		LOG_WARN("Error f_MQTTClient_subscribe %s",dlerror())
		return 0;
	}

	f_MQTTClient_unsubscribe = dlsym(handle, "MQTTClient_unsubscribe");
	if ((error = dlerror()) != NULL)  {
		LOG_WARN("Error f_MQTTClient_unsubscribe %s",dlerror())
		return 0;
	}

	f_MQTTClient_publish = dlsym(handle, "MQTTClient_publish");
	if ((error = dlerror()) != NULL)  {
		LOG_WARN("Error f_MQTTClient_publish %s",dlerror())
		return 0;
	}

	f_MQTTClient_publishMessage = dlsym(handle, "MQTTClient_publishMessage");
	if ((error = dlerror()) != NULL)  {
		LOG_WARN("Error f_MQTTClient_publishMessage %s",dlerror())
		return 0;
	}

	f_MQTTClient_yield = dlsym(handle, "MQTTClient_yield");
	if ((error = dlerror()) != NULL)  {
		LOG_WARN("Error f_MQTTClient_yield %s",dlerror())
		return 0;
	}

	f_MQTTClient_freeMessage = dlsym(handle, "MQTTClient_freeMessage");
	if ((error = dlerror()) != NULL)  {
		LOG_WARN("Error f_MQTTClient_freeMessage %s",dlerror())
		return 0;
	}

	f_MQTTClient_free = dlsym(handle, "MQTTClient_free");
	if ((error = dlerror()) != NULL)  {
		LOG_WARN("Error f_MQTTClient_free %s",dlerror())
		return 0;
	}

	f_MQTTClient_destroy = dlsym(handle, "MQTTClient_destroy");
	if(!f_MQTTClient_destroy || (error = dlerror()) != NULL)  {
		LOG_WARN("Error f_MQTTClient_destroy %s",dlerror())
		return 0;
	}
	mqttLibLoaded = 1;
//	LOG_TRACE("MQTT Lib loaded\n");
	return 1;
}

