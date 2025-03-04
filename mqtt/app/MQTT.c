/**
 * MQTT.c
 * Fred Juhlin 2025
 * This file implements an MQTT client using the Paho MQTT library.
 * It handles connection, disconnection, reconnection, and message publishing.
 * Version 2.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>
#include <dlfcn.h>
#include <pthread.h>
#include <time.h>
#include <errno.h>
#include <syslog.h>
#include "ACAP.h"
#include "MQTT.h"
#include "MQTTClient.h"
#include "CERTS.h"

#define LOG(fmt, args...)    { syslog(LOG_INFO, fmt, ## args);  printf(fmt, ## args); }
#define LOG_WARN(fmt, args...)    { syslog(LOG_WARNING, fmt, ## args); printf(fmt, ## args);}
#define LOG_TRACE(fmt, args...)    { syslog(LOG_INFO, fmt, ## args); printf(fmt, ## args); }
//#define LOG_TRACE(fmt, args...)    {}

#define MQTT_KEEP_ALIVE_INTERVAL 60
#define MQTT_RECONNECT_DELAY_MS 5000
#define MQTT_DEFAULT_QOS 0
#define MQTT_TIMEOUT_MS 10000
#define MQTT_MAX_PAYLOAD_SIZE 1024
#define MQTT_MAX_TOPIC_SIZE 256

// Structure to hold MQTT client configuration
typedef struct {
    bool enabled;
    char address[256];
    char port[16];
    char user[128];
    char password[128];
    char clientID[128];
    char preTopic[128];
    bool tls;
    bool verify;
    struct {
        char name[128];
        char location[128];
    } payload;
} MQTTConfig;

// Structure to hold MQTT client state
typedef struct {
    void *libHandle;
    MQTTClient client;
    bool connected;
	bool disconnecting;	
    bool initialized;
    pthread_mutex_t mutex;
    pthread_t reconnect_thread;
    bool reconnect_thread_running;
	bool manual_disconnect;	
    MQTTConfig config;
    MQTT_Callback_Connection connection_callback;
    MQTT_Callback_Message message_callback;
    char acapname[128];
} MQTTClientState;

// Function pointers for dynamically loaded Paho MQTT library
typedef int (*MQTTClient_create_func)(MQTTClient*, const char*, const char*, int, void*);
typedef int (*MQTTClient_connect_func)(MQTTClient, MQTTClient_connectOptions*);
typedef int (*MQTTClient_disconnect_func)(MQTTClient, int);
typedef int (*MQTTClient_isConnected_func)(MQTTClient);
typedef int (*MQTTClient_publishMessage_func)(MQTTClient, const char*, MQTTClient_message*, MQTTClient_deliveryToken*);
typedef int (*MQTTClient_destroy_func)(MQTTClient*);
typedef int (*MQTTClient_subscribe_func)(MQTTClient, const char*, int);
typedef int (*MQTTClient_unsubscribe_func)(MQTTClient, const char*);
typedef int (*MQTTClient_setCallbacks_func)(MQTTClient, void*, MQTTClient_connectionLost*, 
                                           MQTTClient_messageArrived*, MQTTClient_deliveryComplete*);
typedef void (*MQTTClient_freeMessage_func)(MQTTClient_message**);
typedef void (*MQTTClient_free_func)(void*);

// Function pointers
static MQTTClient_create_func mqtt_client_create = NULL;
static MQTTClient_connect_func mqtt_client_connect = NULL;
static MQTTClient_disconnect_func mqtt_client_disconnect = NULL;
static MQTTClient_isConnected_func mqtt_client_isConnected = NULL;
static MQTTClient_publishMessage_func mqtt_client_publishMessage = NULL;
static MQTTClient_destroy_func mqtt_client_destroy = NULL;
static MQTTClient_subscribe_func mqtt_client_subscribe = NULL;
static MQTTClient_unsubscribe_func mqtt_client_unsubscribe = NULL;
static MQTTClient_setCallbacks_func mqtt_client_setCallbacks = NULL;
static MQTTClient_freeMessage_func mqtt_client_freeMessage = NULL;
static MQTTClient_free_func mqtt_client_free = NULL;

// Global client state
static MQTTClientState g_mqtt_state = {0};
// Global MQTT settings JSON structure
static cJSON* MQTTSettings = NULL;
static char LastWillTopic[64];
static char LastWillMessage[512];


// Forward declarations for new functions
static int MQTT_initialize_settings(const char* acapname);
static void MQTT_save_settings(void);
static void* MQTT_reconnect_thread(void *arg);
static int MQTT_load_library(void);
static void MQTT_unload_library(void);
static int MQTT_parse_config(const cJSON *config_json);
static void MQTT_notify_connection_state(int state);
static void MQTT_connection_lost(void *context, char *cause);
static void MQTT_delivery_complete(void *context, MQTTClient_deliveryToken token);
static int MQTT_message_arrived(void *context, char *topicName, int topicLen, MQTTClient_message *message);

// HTTP endpoint callback for MQTT settings
static void
MQTT_HTTP_callback(const ACAP_HTTP_Response response, const ACAP_HTTP_Request request) {
    if (!MQTTSettings) {
        ACAP_HTTP_Respond_Error(response, 500, "Invalid settings");
        LOG_WARN("%s: Invalid settings not initialized\n", __func__);
        return;
    }
	

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

    LOG_TRACE("%s: %s\n", __func__, json);

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

	//Force connection
	cJSON_GetObjectItem(settings,"connect")->type = cJSON_True;

        
    if (!MQTT_Set(settings)) {
        ACAP_HTTP_Respond_Error(response, 400, ACAP_STATUS_String("mqtt", "status"));
        LOG_WARN("%s: Unable to update MQTT settings\n",__func__);
        cJSON_Delete(settings);
		return;
	}

    LOG_TRACE("%s: Connecting...\n", __func__);

    cJSON_Delete(settings);
    ACAP_HTTP_Respond_Text(response, "MQTT Updated");
    MQTT_Connect();
}


// Set MQTT configuration
int MQTT_Set(cJSON* settings) {
    LOG_TRACE("%s: Entry\n", __func__);
	
    if (!settings) {
        ACAP_STATUS_SetString("mqtt", "status", "Cannot set: NULL settings");
		LOG_WARN("%s: Settings are NULL\n",__func__);
        return 0;
    }

    if (MQTT_isConnected()) {
        MQTT_Disconnect();
    }

    pthread_mutex_lock(&g_mqtt_state.mutex);
    if (g_mqtt_state.initialized) {
        mqtt_client_destroy(&g_mqtt_state.client);
        g_mqtt_state.initialized = false;
    }
    pthread_mutex_unlock(&g_mqtt_state.mutex);

	
    // Update global settings first
    if (MQTTSettings) {
        cJSON* prop = settings->child;
        while (prop) {
            if (cJSON_GetObjectItem(MQTTSettings, prop->string)) {
                cJSON_ReplaceItemInObject(MQTTSettings, prop->string, cJSON_Duplicate(prop, 1));
            }
            prop = prop->next;
        }
        
        // Save updated settings
        MQTT_save_settings();
    }
    
    // Parse the configuration into internal structure
    int result = MQTT_parse_config(MQTTSettings);
    if (!result) {
        // Status is set in MQTT_parse_config()
        LOG_WARN("%s: Parse settings failed\n",__func__);
        return 0;
    }
    
    // If not connected but should connect
    result = MQTT_Connect();

    LOG_TRACE("%s: Exit %d %s\n", __func__, result, g_mqtt_state.config.address);
    return result;
}

// Connect to the MQTT broker
int MQTT_Connect() {
    LOG_TRACE("%s: Entry\n", __func__);

    pthread_mutex_lock(&g_mqtt_state.mutex);
    g_mqtt_state.config.enabled = true;

    // Check if already connected
    if (g_mqtt_state.connected) {
        LOG_TRACE("%s: Already connected\n", __func__);
        pthread_mutex_unlock(&g_mqtt_state.mutex);
        return 1;
    }
    
    // Check if MQTT is enabled and address is set
    if (g_mqtt_state.config.address[0] == '\0') {
        pthread_mutex_unlock(&g_mqtt_state.mutex);
        ACAP_STATUS_SetString("mqtt", "status", "MQTT disabled or address not set");
        return 0;
    }

    pthread_mutex_unlock(&g_mqtt_state.mutex);

    // Update status
    ACAP_STATUS_SetString("mqtt", "status", "Connecting...");

    // Construct server URI
    char server_uri[512];
    if (g_mqtt_state.config.tls) {
        snprintf(server_uri, sizeof(server_uri), "ssl://%s:%s",
                 g_mqtt_state.config.address, g_mqtt_state.config.port);
    } else {
        snprintf(server_uri, sizeof(server_uri), "tcp://%s:%s",
                 g_mqtt_state.config.address, g_mqtt_state.config.port);
    }

    // Destroy existing client if initialized
    pthread_mutex_lock(&g_mqtt_state.mutex);
    if (g_mqtt_state.initialized) {
        mqtt_client_destroy(&g_mqtt_state.client);
        g_mqtt_state.initialized = false;
    }
    pthread_mutex_unlock(&g_mqtt_state.mutex);

    // Create a new MQTT client
    char clientId[256];
    if (g_mqtt_state.config.clientID[0]) {
        strncpy(clientId, g_mqtt_state.config.clientID, sizeof(clientId) - 1);
    } else {
        snprintf(clientId, sizeof(clientId), "%s_%ld", g_mqtt_state.acapname, (long)time(NULL));
    }

	LOG_TRACE("%s: Create client %s %s TLS=%d\n",__func__,clientId,server_uri,g_mqtt_state.config.tls);

    int rc = mqtt_client_create(&g_mqtt_state.client, server_uri, clientId, MQTTCLIENT_PERSISTENCE_NONE, NULL);
    
    if (rc != MQTTCLIENT_SUCCESS) {
        char errMsg[256];
        snprintf(errMsg, sizeof(errMsg), "Failed to create MQTT client: %d", rc);
        LOG_WARN("%s: %s\n", __func__, errMsg);
        ACAP_STATUS_SetString("mqtt", "status", errMsg);
        return 0;
    }

    // Set callbacks
    rc = mqtt_client_setCallbacks(g_mqtt_state.client, NULL,
                                   MQTT_connection_lost,
                                   MQTT_message_arrived,
                                   MQTT_delivery_complete);
    
    if (rc != MQTTCLIENT_SUCCESS) {
        char errMsg[256];
        snprintf(errMsg, sizeof(errMsg), "Failed to set MQTT callbacks: %d", rc);
        LOG_WARN("%s: %s\n", __func__, errMsg);
        ACAP_STATUS_SetString("mqtt", "status", errMsg);
        mqtt_client_destroy(&g_mqtt_state.client);
        return 0;
    }

    g_mqtt_state.initialized = true;

    // Set up connection options
    MQTTClient_connectOptions conn_opts = MQTTClient_connectOptions_initializer;
    conn_opts.keepAliveInterval = MQTT_KEEP_ALIVE_INTERVAL;
    conn_opts.cleansession = 1;
    conn_opts.connectTimeout = MQTT_TIMEOUT_MS / 1000; // Convert to seconds

    // Set username and password if provided
    if (g_mqtt_state.config.user[0] != '\0') {
        conn_opts.username = g_mqtt_state.config.user;
        if (g_mqtt_state.config.password[0] != '\0') {
            conn_opts.password = g_mqtt_state.config.password;
        }
    }

	//ADD Last Will Testament
	MQTTClient_willOptions LastWill = MQTTClient_willOptions_initializer;
	
	char *preTopic = cJSON_GetObjectItem(MQTTSettings,"preTopic") ? cJSON_GetObjectItem(MQTTSettings,"preTopic")->valuestring:0;
	if( preTopic && strlen(preTopic) )
		sprintf(LastWillTopic,"%s/connect/%s",preTopic,ACAP_DEVICE_Prop("serial"));
	else
		sprintf(LastWillTopic,"connect/%s",ACAP_DEVICE_Prop("serial"));

	cJSON* lwt = cJSON_CreateObject();
	cJSON_AddFalseToObject(lwt,"connected");
	cJSON_AddStringToObject(lwt,"address",ACAP_DEVICE_Prop("IPv4"));
	cJSON* helperProperties = cJSON_GetObjectItem(MQTTSettings,"payload");
	if( helperProperties ) {
		const char* name = cJSON_GetObjectItem(helperProperties,"name")?cJSON_GetObjectItem(helperProperties,"name")->valuestring:0;
		const char* location = cJSON_GetObjectItem(helperProperties,"location")?cJSON_GetObjectItem(helperProperties,"location")->valuestring:0;
		if( name && strlen(name) )
			cJSON_AddStringToObject(lwt,"name",name);
		if( location && strlen(location) )
			cJSON_AddStringToObject(lwt,"location",location);
	}
	cJSON_AddStringToObject(lwt,"serial",ACAP_DEVICE_Prop("serial"));
	char* json = cJSON_PrintUnformatted(lwt);
	if( json ) {
		sprintf(LastWillMessage,"%s",json);
		LastWill.topicName = LastWillTopic;
		LastWill.message = LastWillMessage;
		LastWill.retained = 1;
		conn_opts.will = &LastWill;
	}
	cJSON_Delete(lwt);

   // Set up SSL options if TLS is enabled
   MQTTClient_SSLOptions ssl_opts = MQTTClient_SSLOptions_initializer;
   if (g_mqtt_state.config.tls) {
       ssl_opts.verify = g_mqtt_state.config.verify;
       ssl_opts.enableServerCertAuth = g_mqtt_state.config.verify;
	   ssl_opts.sslVersion = MQTT_SSL_VERSION_TLS_1_2;
	
       // Add certificate files if available
	   ssl_opts.trustStore = CERTS_Get_CA();
       ssl_opts.keyStore = CERTS_Get_Cert();
       ssl_opts.privateKey = CERTS_Get_Key();
       ssl_opts.privateKeyPassword = CERTS_Get_Password();
//LOG("%s %s %s %s\n", ssl_opts.trustStore, ssl_opts.keyStore,ssl_opts.privateKey,ssl_opts.privateKeyPassword);
       conn_opts.ssl = &ssl_opts;
   }

   // Connect to the broker
   rc = mqtt_client_connect(g_mqtt_state.client, &conn_opts);
   if (rc != MQTTCLIENT_SUCCESS) {
       char errMsg[256];
       snprintf(errMsg, sizeof(errMsg), "Failed to connect to MQTT broker: %d", rc);
       LOG_WARN("%s: %s\n", __func__, errMsg);
       ACAP_STATUS_SetString("mqtt", "status", "Reconnecting...");
       return 0;
   }

   // Update connection state
   pthread_mutex_lock(&g_mqtt_state.mutex);
   g_mqtt_state.connected = true;
   pthread_mutex_unlock(&g_mqtt_state.mutex);

   // Notify application about successful connection
   MQTT_notify_connection_state(MQTT_CONNECT);
   cJSON_GetObjectItem(MQTTSettings,"connect")->type = cJSON_True;
   g_mqtt_state.config.enabled = true;	
   MQTT_save_settings();
   
   LOG_TRACE("%s: Exit\n", __func__);
   
   return 1;
}

// Disconnect from the MQTT broker
int MQTT_Disconnect() {
    LOG_TRACE("%s: Entry\n", __func__);

	if( strlen( LastWillMessage) ) {
		char topic[64];
		sprintf(topic, "connect/%s",ACAP_DEVICE_Prop("serial"));
		MQTT_Publish( topic,LastWillMessage,0,1);
	}
    
    pthread_mutex_lock(&g_mqtt_state.mutex);
    g_mqtt_state.manual_disconnect = true;
    g_mqtt_state.config.enabled = false;
    bool should_disconnect = g_mqtt_state.connected && g_mqtt_state.initialized;
    pthread_mutex_unlock(&g_mqtt_state.mutex);
    
    if (should_disconnect) {
        int rc = mqtt_client_disconnect(g_mqtt_state.client, MQTT_TIMEOUT_MS);
        if (rc != MQTTCLIENT_SUCCESS) {
            LOG_WARN("%s: Failed to disconnect: %d\n", __func__, rc);
        }
        
        pthread_mutex_lock(&g_mqtt_state.mutex);
        g_mqtt_state.connected = false;
        pthread_mutex_unlock(&g_mqtt_state.mutex);
        
        ACAP_STATUS_SetBool("mqtt", "connected", 0);
        ACAP_STATUS_SetString("mqtt", "status", "Disconnected");
    }
    
    // Wait for reconnect thread to stop
    if (g_mqtt_state.reconnect_thread_running) {
        pthread_join(g_mqtt_state.reconnect_thread, NULL);
    }
    // Notify application about disconnection
    MQTT_notify_connection_state(MQTT_DISCONNECT);
    LOG_TRACE("%s: Exit\n", __func__);
    
    return 1;
}

// Initialize MQTT settings from files
static int MQTT_initialize_settings(const char* acapname) {
    LOG_TRACE("%s: Entry\n", __func__);
	
    if (MQTTSettings) {
		LOG_WARN("%s: Already initialized\n",__func__);
        return 0; // Already initialized
    }
    // Read default settings
    MQTTSettings = ACAP_FILE_Read("settings/mqtt.json");
    if (!MQTTSettings) {
        LOG_WARN("%s: Unable to parse default settings\n", __func__);
        ACAP_STATUS_SetString("mqtt", "status", "Missing default settings");
        return 0;
    }
    
    // Read saved settings and merge them
    cJSON* savedSettings = ACAP_FILE_Read("localdata/mqtt.json");
    LOG_TRACE("%s: File read: Settings: %s\n", __func__, savedSettings ? "OK" : "Failed");
    if (savedSettings) {
        LOG_TRACE("%s: Saved settings found\n", __func__);
        cJSON* prop = savedSettings->child;
        while (prop) {
            if (cJSON_GetObjectItem(MQTTSettings, prop->string)) {
                cJSON_ReplaceItemInObject(MQTTSettings, prop->string, cJSON_Duplicate(prop, 1));
            }
            prop = prop->next;
        }
        cJSON_Delete(savedSettings);
    }
    
    // Set clientID based on acapname and device serial
	char topicString[32];
	sprintf( topicString,"%s-%s",acapname,ACAP_DEVICE_Prop("serial"));
	cJSON_ReplaceItemInObject(MQTTSettings, "clientID", cJSON_CreateString(topicString));
    LOG_TRACE("%s: Exit\n", __func__);
  
    return 1;
}

// Save current MQTT settings to file
static void MQTT_save_settings(void) {
    LOG_TRACE("%s: Entry\n", __func__);

    if (MQTTSettings) {
        ACAP_FILE_Write("localdata/mqtt.json", MQTTSettings);
    }
    LOG_TRACE("%s: Exit\n", __func__);
	
}

// Initialize the MQTT client
int MQTT_Init(const char* acapname, MQTT_Callback_Connection callback) {
    // Set initial status
    LOG_TRACE("%s: Entry\n", __func__);
	
    ACAP_STATUS_SetString("mqtt", "status", "Initializing");
    ACAP_STATUS_SetBool("mqtt", "connected", 0);
    
    // Initialize mutex
    if (pthread_mutex_init(&g_mqtt_state.mutex, NULL) != 0) {
        LOG_WARN("%s: Failed to initialize MQTT mutex\n", __func__);
        ACAP_STATUS_SetString("mqtt", "status", "Failed to initialize mutex");
        return 0;
    }
    
    // Store ACAP name and callback
    if (acapname) {
        strncpy(g_mqtt_state.acapname, acapname, sizeof(g_mqtt_state.acapname) - 1);
    } else {
        strncpy(g_mqtt_state.acapname, "ACAP_MQTT", sizeof(g_mqtt_state.acapname) - 1);
    }
    g_mqtt_state.connection_callback = callback;

    
    // Initialize settings
    if (!MQTT_initialize_settings(g_mqtt_state.acapname)) {
		LOG_WARN("%s: Unable to initialize settings\n",__func__);
        pthread_mutex_destroy(&g_mqtt_state.mutex);
        ACAP_STATUS_SetString("mqtt", "status", "Failed to initialize settings");
        return 0;
    }
    
    // Load the MQTT library
    if (!MQTT_load_library()) {
        pthread_mutex_destroy(&g_mqtt_state.mutex);
        // Status is set in MQTT_load_library()
        return 0;
    }

    
    // Parse initial configuration
    if (!MQTT_parse_config(MQTTSettings)) {
        pthread_mutex_destroy(&g_mqtt_state.mutex);
        // Status is set in MQTT_parse_config()
        return 0;
    }
    
    // Initialize HTTP endpoint
    ACAP_HTTP_Node("mqtt", MQTT_HTTP_callback);
    
    // Initialize certificates
    CERTS_Init();
    
    ACAP_STATUS_SetString("mqtt", "status", "Initialized");
    
    // Auto-connect if enabled
    if (g_mqtt_state.config.address[0] != '\0') {
        MQTT_Connect();
    }
	
    LOG_TRACE("%s: Exit\n", __func__);
    return 1;
}

// Clean up MQTT client resources
void MQTT_Cleanup() {
    // Disconnect if connected
    MQTT_Disconnect();
    
    // Destroy client if initialized
    pthread_mutex_lock(&g_mqtt_state.mutex);
    if (g_mqtt_state.initialized) {
        mqtt_client_destroy(&g_mqtt_state.client);
        g_mqtt_state.initialized = false;
    }
    pthread_mutex_unlock(&g_mqtt_state.mutex);
    
    // Free global settings
    if (MQTTSettings) {
        cJSON_Delete(MQTTSettings);
        MQTTSettings = NULL;
    }
    
    // Unload the library
    MQTT_unload_library();
    
    // Destroy mutex
    pthread_mutex_destroy(&g_mqtt_state.mutex);
    
    // Reset state
    memset(&g_mqtt_state, 0, sizeof(MQTTClientState));
    
    // Update status
    ACAP_STATUS_SetString("mqtt", "status", "Not initialized");
    ACAP_STATUS_SetBool("mqtt", "connected", 0);
}

// Check if connected to the MQTT broker
int MQTT_isConnected() {
    pthread_mutex_lock(&g_mqtt_state.mutex);
    bool connected = g_mqtt_state.connected && g_mqtt_state.initialized && 
                    mqtt_client_isConnected(g_mqtt_state.client);
    pthread_mutex_unlock(&g_mqtt_state.mutex);
    
    return connected ? 1 : 0;
}

// Publish a message to a topic
int MQTT_Publish(const char *topic, const char *payload, int qos, int retained) {
    LOG_TRACE("%s: Entry\n", __func__);

	if(!MQTT_isConnected())
		return 0;
	
    int result = 0;
    
    // Check if connected
    pthread_mutex_lock(&g_mqtt_state.mutex);
    if (!g_mqtt_state.connected || !g_mqtt_state.initialized) {
        pthread_mutex_unlock(&g_mqtt_state.mutex);
        return 0;
    }
    pthread_mutex_unlock(&g_mqtt_state.mutex);
    
    // Create full topic with prefix if needed
    char full_topic[MQTT_MAX_TOPIC_SIZE];
    if (g_mqtt_state.config.preTopic[0] != '\0') {
        snprintf(full_topic, sizeof(full_topic), "%s/%s", g_mqtt_state.config.preTopic, topic);
    } else {
        strncpy(full_topic, topic, sizeof(full_topic) - 1);
    }
    
    // Create message
    MQTTClient_message message = MQTTClient_message_initializer;
    message.payload = (void*)payload;
    message.payloadlen = strlen(payload);
    message.qos = MQTT_DEFAULT_QOS; // Always use QoS 0 as specified
    message.retained = retained;
    
    // Publish message
    MQTTClient_deliveryToken token;
    result = mqtt_client_publishMessage(g_mqtt_state.client, full_topic, &message, &token);
    
    if (result != MQTTCLIENT_SUCCESS) {
        char errMsg[256];
        snprintf(errMsg, sizeof(errMsg), "Failed to publish message: %d", result);
        LOG_WARN("%s: %s\n", __func__, errMsg);
    }
    LOG_TRACE("%s: Exit\n", __func__);
    return (result == MQTTCLIENT_SUCCESS) ? 1 : 0;
}

// Publish a JSON message to a topic
int MQTT_Publish_JSON(const char *topic, cJSON *payload, int qos, int retained) {
    if (!payload) {
        return 0;
    }

	if(!MQTT_isConnected())
		return 0;
    
	cJSON* publish = cJSON_Duplicate(payload, 1);
	cJSON* helperProperties = cJSON_GetObjectItem(MQTTSettings,"payload");

	if( helperProperties ) {
		const char* name = cJSON_GetObjectItem(helperProperties,"name")?cJSON_GetObjectItem(helperProperties,"name")->valuestring:0;
		const char* location = cJSON_GetObjectItem(helperProperties,"location")?cJSON_GetObjectItem(helperProperties,"location")->valuestring:0;
		if( name && strlen(name) > 0 )
			cJSON_AddStringToObject(publish,"name",name);
		if( location && strlen(location) > 0 )
			cJSON_AddStringToObject(publish,"location",location);
	}
	cJSON_AddStringToObject(publish,"serial",ACAP_DEVICE_Prop("serial"));


	char *json = cJSON_PrintUnformatted(publish);
	if(!json) {
		LOG_TRACE("%s: Exit error\n",__func__);
		return 0;
	}
	cJSON_Delete(publish);
	
	int result = MQTT_Publish(topic, json, qos, retained );
	free(json);
    
    return result;
}

// Publish binary data to a topic
int MQTT_Publish_Binary(const char *topic, int payloadlen, void *payload, int qos, int retained) {
    int result = 0;

	if(!MQTT_isConnected())
		return 0;
    
    // Check if connected
    pthread_mutex_lock(&g_mqtt_state.mutex);
    if (!g_mqtt_state.connected || !g_mqtt_state.initialized) {
        pthread_mutex_unlock(&g_mqtt_state.mutex);
        ACAP_STATUS_SetString("mqtt", "status", "Cannot publish binary: Not connected");
        return 0;
    }
    pthread_mutex_unlock(&g_mqtt_state.mutex);
    
    // Create full topic with prefix if needed
    char full_topic[MQTT_MAX_TOPIC_SIZE];
    if (g_mqtt_state.config.preTopic[0] != '\0') {
        snprintf(full_topic, sizeof(full_topic), "%s/%s", g_mqtt_state.config.preTopic, topic);
    } else {
        strncpy(full_topic, topic, sizeof(full_topic) - 1);
    }
    
    // Create message
    MQTTClient_message message = MQTTClient_message_initializer;
    message.payload = payload;
    message.payloadlen = payloadlen;
    message.qos = MQTT_DEFAULT_QOS; // Always use QoS 0 as specified
    message.retained = retained;
    
    // Publish message
    MQTTClient_deliveryToken token;
    result = mqtt_client_publishMessage(g_mqtt_state.client, full_topic, &message, &token);
    
    if (result != MQTTCLIENT_SUCCESS) {
        char errMsg[256];
        snprintf(errMsg, sizeof(errMsg), "Failed to publish binary message: %d", result);
        LOG_WARN("%s: %s\n", __func__, errMsg);
        ACAP_STATUS_SetString("mqtt", "status", errMsg);
    }
    
    return (result == MQTTCLIENT_SUCCESS) ? 1 : 0;
}

// Subscribe to a topic
int MQTT_Subscribe(const char *topic, MQTT_Callback_Message callback) {
    int result = 1;

	LOG_TRACE("%s: %s\n",__func__, topic );
    
    // Check if connected
    pthread_mutex_lock(&g_mqtt_state.mutex);
    if (!g_mqtt_state.connected || !g_mqtt_state.initialized) {
        pthread_mutex_unlock(&g_mqtt_state.mutex);
		LOG_WARN("%s: Mutex lock failed\n",__func__);
        return 0;
    }
    
    // Store the message callback
    g_mqtt_state.message_callback = callback;
    pthread_mutex_unlock(&g_mqtt_state.mutex);
    
    // Create full topic with prefix if needed
    char full_topic[MQTT_MAX_TOPIC_SIZE];
    if (g_mqtt_state.config.preTopic[0] != '\0') {
        snprintf(full_topic, sizeof(full_topic), "%s/%s", g_mqtt_state.config.preTopic, topic);
    } else {
        strncpy(full_topic, topic, sizeof(full_topic) - 1);
    }
    
    // Subscribe to the topic
    result = mqtt_client_subscribe(g_mqtt_state.client, full_topic, MQTT_DEFAULT_QOS);
    
    if (result != MQTTCLIENT_SUCCESS) {
        char errMsg[256];
        snprintf(errMsg, sizeof(errMsg), "Failed to subscribe to topic: %d", result);
        LOG_WARN("%s: %s\n", __func__, errMsg);
        ACAP_STATUS_SetString("mqtt", "status", errMsg);
    }
    
    return (result == MQTTCLIENT_SUCCESS) ? 1 : 0;
}

// Unsubscribe from a topic
int MQTT_Unsubscribe(const char *topic) {
    int result = 0;
    
    // Check if connected
    pthread_mutex_lock(&g_mqtt_state.mutex);
    if (!g_mqtt_state.connected || !g_mqtt_state.initialized) {
        pthread_mutex_unlock(&g_mqtt_state.mutex);
        ACAP_STATUS_SetString("mqtt", "status", "Cannot unsubscribe: Not connected");
        return 0;
    }
    pthread_mutex_unlock(&g_mqtt_state.mutex);
    
    // Create full topic with prefix if needed
    char full_topic[MQTT_MAX_TOPIC_SIZE];
    if (g_mqtt_state.config.preTopic[0] != '\0') {
        snprintf(full_topic, sizeof(full_topic), "%s/%s", g_mqtt_state.config.preTopic, topic);
    } else {
        strncpy(full_topic, topic, sizeof(full_topic) - 1);
    }
    
    // Unsubscribe from the topic
    result = mqtt_client_unsubscribe(g_mqtt_state.client, full_topic);
    
    if (result != MQTTCLIENT_SUCCESS) {
        char errMsg[256];
        snprintf(errMsg, sizeof(errMsg), "Failed to unsubscribe from topic: %d", result);
        LOG_WARN("%s\n", errMsg);
        ACAP_STATUS_SetString("mqtt", "status", errMsg);
    }
    
    return (result == MQTTCLIENT_SUCCESS) ? 1 : 0;
}


// Get current MQTT settings as JSON
cJSON* MQTT_Settings() {
    // Return the global settings object
    return MQTTSettings;
}

// Callback for connection lost
static void MQTT_connection_lost(void *context, char *cause) {
    LOG_TRACE("%s: Entry\n", __func__);
    
    (void)context; // Unused parameter

    pthread_mutex_lock(&g_mqtt_state.mutex);
    g_mqtt_state.connected = false;
    pthread_mutex_unlock(&g_mqtt_state.mutex);

    // Update status
    ACAP_STATUS_SetBool("mqtt", "connected", 0);
    
    if (cause) {
        LOG_WARN("%s: MQTT connection lost. Cause: %s\n", __func__, cause);
        char statusMsg[256];
        snprintf(statusMsg, sizeof(statusMsg), "Connection lost: %s", cause);
        ACAP_STATUS_SetString("mqtt", "status", statusMsg);
    } else {
        LOG_WARN("%s: MQTT connection lost. Unknown cause.\n", __func__);
        ACAP_STATUS_SetString("mqtt", "status", "Connection lost: Unknown cause");
    }

    // Notify application about disconnection
    MQTT_notify_connection_state(MQTT_DISCONNECT);

    // Start reconnection thread if not already running and MQTT is enabled
    pthread_mutex_lock(&g_mqtt_state.mutex);
    if (!g_mqtt_state.reconnect_thread_running && g_mqtt_state.config.enabled) {
        g_mqtt_state.reconnect_thread_running = true;
        if (pthread_create(&g_mqtt_state.reconnect_thread, NULL, MQTT_reconnect_thread, NULL) != 0) {
            LOG_WARN("%s: Failed to create MQTT reconnect thread\n", __func__);
            g_mqtt_state.reconnect_thread_running = false;
            ACAP_STATUS_SetString("mqtt", "status", "Failed to create reconnect thread");
        }
    }
    pthread_mutex_unlock(&g_mqtt_state.mutex);

    LOG_TRACE("%s: Exit\n", __func__);
}

// Message arrived callback
static int MQTT_message_arrived(void *context, char *topicName, int topicLen, MQTTClient_message *message) {
    LOG_TRACE("%s: Entry\n", __func__);

    (void)context; // Unused parameter
    (void)topicLen; // Unused parameter

    // Make a null-terminated copy of the payload
    char *payload = malloc(message->payloadlen + 1);
    if (payload) {
        memcpy(payload, message->payload, message->payloadlen);
        payload[message->payloadlen] = '\0';

        // Call the user's message callback if registered
        if (g_mqtt_state.message_callback) {
            g_mqtt_state.message_callback(topicName, payload);
        }

        free(payload);
    }
    
    // Free the message as required by Paho MQTT
    mqtt_client_freeMessage(&message);
    LOG_TRACE("%s: Exit\n", __func__);
    
    return 1;
}

// Delivery complete callback (required by Paho but not used in this implementation)
static void MQTT_delivery_complete(void *context, MQTTClient_deliveryToken token) {
    (void)context; // Unused parameter
    (void)token;   // Unused parameter
    // Not used in this implementation
}

// Notify the application about connection state changes
static void MQTT_notify_connection_state(int state) {
    LOG_TRACE("%s: Entry\n", __func__);
	
    if (g_mqtt_state.connection_callback) {
        g_mqtt_state.connection_callback(state);
    }
    
    // Update status based on the state
    switch (state) {
        case MQTT_CONNECT:
            ACAP_STATUS_SetString("mqtt", "status", "Connected");
            ACAP_STATUS_SetBool("mqtt", "connected", 1);
            break;
            
        case MQTT_DISCONNECT:
            ACAP_STATUS_SetString("mqtt", "status", "Disconnected");
            ACAP_STATUS_SetBool("mqtt", "connected", 0);
            break;
            
        case MQTT_RECONNECT:
            ACAP_STATUS_SetString("mqtt", "status", "Reconnecting");
            ACAP_STATUS_SetBool("mqtt", "connected", 0);
            break;
            
        default:
            // No status change for other states
            break;
    }
    LOG_TRACE("%s: Exit\n", __func__);
}


// Reconnect thread function
static void* MQTT_reconnect_thread(void *arg) {
    LOG_TRACE("%s: Entry\n", __func__);
    
    while (g_mqtt_state.config.enabled) {
        pthread_mutex_lock(&g_mqtt_state.mutex);
        bool should_reconnect = !g_mqtt_state.connected && 
                                g_mqtt_state.config.address[0] != '\0';
        pthread_mutex_unlock(&g_mqtt_state.mutex);
        
        if (should_reconnect) {
            MQTT_notify_connection_state(MQTT_RECONNECT);
			ACAP_STATUS_SetString("mqtt","status","Reconnecting...");
            LOG_TRACE("Attempting to reconnect to MQTT broker...\n");
            if (MQTT_Connect()) {
                LOG_TRACE("Successfully reconnected to MQTT broker\n");
            } else {
                usleep(MQTT_RECONNECT_DELAY_MS * 1000);
            }
        } else {
            usleep(MQTT_RECONNECT_DELAY_MS * 1000);
        }
    }
    
    pthread_mutex_lock(&g_mqtt_state.mutex);
    g_mqtt_state.reconnect_thread_running = false;
    pthread_mutex_unlock(&g_mqtt_state.mutex);
    LOG_TRACE("%s: Exit\n", __func__);
    
    return NULL;
}

// Load the Paho MQTT library
static int MQTT_load_library(void) {
    // Check if already loaded
    LOG_TRACE("%s: Entry\n", __func__);
	
    if (g_mqtt_state.libHandle != NULL) {
        return 0;
    }
    
    DIR *dir;
    struct dirent *ent;
    char *libFile = NULL;
    char libPath[256] = {0};
    
    ACAP_STATUS_SetString("mqtt", "status", "Loading library");
    
    // Search for the library in /usr/lib
    if ((dir = opendir("/usr/lib")) == NULL) {
        char errMsg[256];
        snprintf(errMsg, sizeof(errMsg), "Could not open /usr/lib: %s", strerror(errno));
        LOG_WARN("%s: %s\n",__func__, errMsg);
        ACAP_STATUS_SetString("mqtt", "status", errMsg);
        return 0;
    }
    
    while ((ent = readdir(dir)) != NULL) {
        if (strstr(ent->d_name, "libpaho-mqtt3cs") != NULL) {
            libFile = strdup(ent->d_name);  // Make a copy of the filename
            break;
        }
    }
    closedir(dir);
    
    if (!libFile) {
        char errMsg[256];
        snprintf(errMsg, sizeof(errMsg), "MQTT library not found in /usr/lib");
        LOG_WARN("%s: %s\n", errMsg, __func__);
        ACAP_STATUS_SetString("mqtt", "status", errMsg);
        return 0;
    }
    
    // Construct full path to the library
    snprintf(libPath, sizeof(libPath), "/usr/lib/%s", libFile);
    LOG_TRACE("Found MQTT library: %s\n", libPath);
    
    // Try to load the library with the full path
    g_mqtt_state.libHandle = dlopen(libPath, RTLD_LAZY);
    if (g_mqtt_state.libHandle == NULL) {
        char errMsg[256];
        snprintf(errMsg, sizeof(errMsg), "Failed to load MQTT library: %s", dlerror());
        LOG_WARN("%s: %s\n", __func__, errMsg);
        ACAP_STATUS_SetString("mqtt", "status", errMsg);
        free(libFile);
        return 0;
    }
    
    // Clear any existing error
    dlerror();
    
    // Load function pointers
    mqtt_client_create = (MQTTClient_create_func)dlsym(g_mqtt_state.libHandle, "MQTTClient_create");
    if (!mqtt_client_create) {
        LOG_WARN("Error loading MQTTClient_create: %s\n", dlerror());
        goto cleanup_error;
    }
    
    mqtt_client_connect = (MQTTClient_connect_func)dlsym(g_mqtt_state.libHandle, "MQTTClient_connect");
    if (!mqtt_client_connect) {
        LOG_WARN("Error loading MQTTClient_connect: %s\n", dlerror());
        goto cleanup_error;
    }
    
    mqtt_client_disconnect = (MQTTClient_disconnect_func)dlsym(g_mqtt_state.libHandle, "MQTTClient_disconnect");
    if (!mqtt_client_disconnect) {
        LOG_WARN("Error loading MQTTClient_disconnect: %s\n", dlerror());
        goto cleanup_error;
    }
    
    mqtt_client_isConnected = (MQTTClient_isConnected_func)dlsym(g_mqtt_state.libHandle, "MQTTClient_isConnected");
    if (!mqtt_client_isConnected) {
        LOG_WARN("Error loading MQTTClient_isConnected: %s\n", dlerror());
        goto cleanup_error;
    }
    
    mqtt_client_publishMessage = (MQTTClient_publishMessage_func)dlsym(g_mqtt_state.libHandle, "MQTTClient_publishMessage");
    if (!mqtt_client_publishMessage) {
        LOG_WARN("Error loading MQTTClient_publishMessage: %s\n", dlerror());
        goto cleanup_error;
    }
    
    mqtt_client_destroy = (MQTTClient_destroy_func)dlsym(g_mqtt_state.libHandle, "MQTTClient_destroy");
    if (!mqtt_client_destroy) {
        LOG_WARN("Error loading MQTTClient_destroy: %s\n", dlerror());
        goto cleanup_error;
    }
    
    mqtt_client_subscribe = (MQTTClient_subscribe_func)dlsym(g_mqtt_state.libHandle, "MQTTClient_subscribe");
    if (!mqtt_client_subscribe) {
        LOG_WARN("Error loading MQTTClient_subscribe: %s\n", dlerror());
        goto cleanup_error;
    }
    
    mqtt_client_unsubscribe = (MQTTClient_unsubscribe_func)dlsym(g_mqtt_state.libHandle, "MQTTClient_unsubscribe");
    if (!mqtt_client_unsubscribe) {
        LOG_WARN("Error loading MQTTClient_unsubscribe: %s\n", dlerror());
        goto cleanup_error;
    }
    
    mqtt_client_setCallbacks = (MQTTClient_setCallbacks_func)dlsym(g_mqtt_state.libHandle, "MQTTClient_setCallbacks");
    if (!mqtt_client_setCallbacks) {
        LOG_WARN("Error loading MQTTClient_setCallbacks: %s\n", dlerror());
        goto cleanup_error;
    }
    
    mqtt_client_freeMessage = (MQTTClient_freeMessage_func)dlsym(g_mqtt_state.libHandle, "MQTTClient_freeMessage");
    if (!mqtt_client_freeMessage) {
        LOG_WARN("Error loading MQTTClient_freeMessage: %s\n", dlerror());
        goto cleanup_error;
    }
    
    mqtt_client_free = (MQTTClient_free_func)dlsym(g_mqtt_state.libHandle, "MQTTClient_free");
    if (!mqtt_client_free) {
        LOG_WARN("Error loading MQTTClient_free: %s\n", dlerror());
        goto cleanup_error;
    }
    
    // Success - free the filename copy and return
    LOG_TRACE("MQTT library successfully loaded\n");
    ACAP_STATUS_SetString("mqtt", "status", "Library loaded");
    free(libFile);
    LOG_TRACE("%s: Exit\n", __func__);
	
    return 1;
    
    char errMsg[256];
cleanup_error:
    // Handle error: close the library and free resources
    snprintf(errMsg, sizeof(errMsg), "Failed to load MQTT functions: %s", dlerror());
    LOG_WARN("%s: %s\n", __func__, errMsg);
    ACAP_STATUS_SetString("mqtt", "status", errMsg);
    
    dlclose(g_mqtt_state.libHandle);
    g_mqtt_state.libHandle = NULL;
    free(libFile);
    LOG_TRACE("%s: Exit error\n", __func__);
    return 0;
}

// Unload the Paho MQTT library
static void MQTT_unload_library(void) {
    if (g_mqtt_state.libHandle != NULL) {
        dlclose(g_mqtt_state.libHandle);
        g_mqtt_state.libHandle = NULL;
        
        // Reset function pointers
        mqtt_client_create = NULL;
        mqtt_client_connect = NULL;
        mqtt_client_disconnect = NULL;
        mqtt_client_isConnected = NULL;
        mqtt_client_publishMessage = NULL;
        mqtt_client_destroy = NULL;
        mqtt_client_subscribe = NULL;
        mqtt_client_unsubscribe = NULL;
        mqtt_client_setCallbacks = NULL;
        mqtt_client_freeMessage = NULL;
        mqtt_client_free = NULL;
    }
}

// Parse the MQTT configuration from JSON
static int MQTT_parse_config(const cJSON *root) {
    LOG_TRACE("%s: Entry\n", __func__);
	
    if (root == NULL) {
        LOG_WARN("%s: NULL config JSON provided\n", __func__);
        ACAP_STATUS_SetString("mqtt", "status", "NULL config JSON provided");
        return 0;
    }
    char* json = cJSON_PrintUnformatted(root);
	if(json) {
		LOG("%s\n",json);
		free(json);
	}
    // Clear the existing configuration
    memset(&g_mqtt_state.config, 0, sizeof(MQTTConfig));
    
    // Parse connect flag
    cJSON *connect = cJSON_GetObjectItem(root, "connect");
    if (connect && cJSON_IsBool(connect)) {
        g_mqtt_state.config.enabled = cJSON_IsTrue(connect);
    } else {
        g_mqtt_state.config.enabled = true; // Default to enabled
    }
    
    // Parse address
    cJSON *address = cJSON_GetObjectItem(root, "address");
    if (address && cJSON_IsString(address) && address->valuestring) {
        strncpy(g_mqtt_state.config.address, address->valuestring, sizeof(g_mqtt_state.config.address) - 1);
        // If address is empty, disable MQTT
        if (g_mqtt_state.config.address[0] == '\0') {
            g_mqtt_state.config.enabled = false;
        }
    }
	if (g_mqtt_state.config.address[0] != '\0')
		g_mqtt_state.config.enabled = true;
    // Parse port
    cJSON *port = cJSON_GetObjectItem(root, "port");
    if (port && cJSON_IsString(port) && port->valuestring) {
        strncpy(g_mqtt_state.config.port, port->valuestring, sizeof(g_mqtt_state.config.port) - 1);
    } else {
        strncpy(g_mqtt_state.config.port, "1883", sizeof(g_mqtt_state.config.port) - 1); // Default port
    }
    
    // Parse user
    cJSON *user = cJSON_GetObjectItem(root, "user");
    if (user && cJSON_IsString(user) && user->valuestring) {
        strncpy(g_mqtt_state.config.user, user->valuestring, sizeof(g_mqtt_state.config.user) - 1);
    }
    
    // Parse password
    cJSON *password = cJSON_GetObjectItem(root, "password");
    if (password && cJSON_IsString(password) && password->valuestring) {
        strncpy(g_mqtt_state.config.password, password->valuestring, sizeof(g_mqtt_state.config.password) - 1);
    }
    
    // Parse clientID
    cJSON *clientID = cJSON_GetObjectItem(root, "clientID");
    if (clientID && cJSON_IsString(clientID) && clientID->valuestring) {
        strncpy(g_mqtt_state.config.clientID, clientID->valuestring, sizeof(g_mqtt_state.config.clientID) - 1);
    }
    
    // Parse preTopic
    cJSON *preTopic = cJSON_GetObjectItem(root, "preTopic");
    if (preTopic && cJSON_IsString(preTopic) && preTopic->valuestring) {
        strncpy(g_mqtt_state.config.preTopic, preTopic->valuestring, sizeof(g_mqtt_state.config.preTopic) - 1);
    } else {
        strncpy(g_mqtt_state.config.preTopic, "dataq", sizeof(g_mqtt_state.config.preTopic) - 1); // Default preTopic
    }
    
    // Parse TLS flag
    cJSON *tls = cJSON_GetObjectItem(root, "tls");
    if (tls && cJSON_IsBool(tls)) {
        g_mqtt_state.config.tls = cJSON_IsTrue(tls);
    }
    
    // Parse verify flag
    cJSON *verify = cJSON_GetObjectItem(root, "verify");
    if (verify && cJSON_IsBool(verify)) {
        g_mqtt_state.config.verify = cJSON_IsTrue(verify);
    }
    
    // Parse payload
    cJSON *payload = cJSON_GetObjectItem(root, "payload");
    if (payload && cJSON_IsObject(payload)) {
        cJSON *name = cJSON_GetObjectItem(payload, "name");
        if (name && cJSON_IsString(name) && name->valuestring) {
            strncpy(g_mqtt_state.config.payload.name, name->valuestring, sizeof(g_mqtt_state.config.payload.name) - 1);
        }
        
        cJSON *location = cJSON_GetObjectItem(payload, "location");
        if (location && cJSON_IsString(location) && location->valuestring) {
            strncpy(g_mqtt_state.config.payload.location, location->valuestring, sizeof(g_mqtt_state.config.payload.location) - 1);
        }
    }
    LOG_TRACE("%s: Exit\n", __func__);
   
    return 1;
}
