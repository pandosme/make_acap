/**
* MQTT.c - Asynchronous Implementation
* Fred Juhlin 2025
* Version 2.2
*/

typedef struct MQTTPersistence_beforeWrite MQTTPersistence_beforeWrite;
typedef struct MQTTPersistence_afterRead MQTTPersistence_afterRead;

#define MQTTASYNC_PERSISTENCE_NONE 1
#define NO_PERSISTENCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dlfcn.h>
#include <syslog.h>
#include <glib.h>
#include <unistd.h>
#include "ACAP.h"
#include "MQTT.h"
#include "MQTTAsync.h"
#include "CERTS.h"

#define LOG(fmt, ...) syslog(LOG_INFO, fmt, ##__VA_ARGS__); printf(fmt, ##__VA_ARGS__)
#define LOG_WARN(fmt, ...) syslog(LOG_WARNING, fmt, ##__VA_ARGS__); printf(fmt, ##__VA_ARGS__)
//#define LOG_TRACE(fmt, args...)    { syslog(LOG_INFO, fmt, ## args); printf(fmt, ## args); }
#define LOG_TRACE(fmt, args...)    {}

#define MQTTASYNC_MSG_QOS0 0
#define MQTTASYNC_MSG_QOS1 1
#define MQTTASYNC_MSG_QOS2 2

// Function pointers for dynamically loaded Paho Async MQTT library
typedef int (*MQTTAsync_create_func)(MQTTAsync*, const char*, const char*, int, void*);
typedef int (*MQTTAsync_connect_func)(MQTTAsync, const MQTTAsync_connectOptions*);
typedef int (*MQTTAsync_disconnect_func)(MQTTAsync, const MQTTAsync_disconnectOptions*);
typedef int (*MQTTAsync_isConnected_func)(MQTTAsync);
typedef int (*MQTTAsync_send_func)(MQTTAsync, const char*, MQTTAsync_message*, int, void*, MQTTAsync_responseOptions*);
typedef int (*MQTTAsync_sendMessage_func)(MQTTAsync handle, const char* destinationName, const MQTTAsync_message* msg, MQTTAsync_responseOptions* response);
typedef int (*MQTTAsync_subscribe_func)(MQTTAsync, const char*, int, MQTTAsync_responseOptions*);
typedef int (*MQTTAsync_unsubscribe_func)(MQTTAsync, const char*, MQTTAsync_responseOptions*);
typedef int (*MQTTAsync_setCallbacks_func)(MQTTAsync, void*, MQTTAsync_connectionLost*, MQTTAsync_messageArrived*, MQTTAsync_deliveryComplete*);
typedef void (*MQTTAsync_freeMessage_func)(MQTTAsync_message**);
typedef void (*MQTTAsync_destroy_func)(MQTTAsync*);
typedef void (*MQTTAsync_free_func)(void* ptr);	
typedef void (*MQTTAsync_setConnected_func)(MQTTAsync handle, void* context, MQTTAsync_connected* co);
static struct {
    MQTTAsync_create_func create;
    MQTTAsync_connect_func connect;
    MQTTAsync_disconnect_func disconnect;
    MQTTAsync_isConnected_func isConnected;
    MQTTAsync_send_func send;
    MQTTAsync_sendMessage_func sendMessage;
    MQTTAsync_subscribe_func subscribe;
    MQTTAsync_unsubscribe_func unsubscribe;
    MQTTAsync_setCallbacks_func setCallbacks;
    MQTTAsync_freeMessage_func freeMessage;
	MQTTAsync_free_func free;
	MQTTAsync_setConnected_func setConnected;
    MQTTAsync_destroy_func destroy;
} mqtt;

static void* MQTT_libHandle = NULL;
static cJSON* MQTTSettings = NULL;
static MQTTAsync mqtt_client = NULL;
static MQTT_Callback_Message userSubscriptionCallback = NULL;
static MQTT_Callback_Connection connectionCallback = NULL;
static char LastWillTopic[64];
static char LastWillMessage[512];
static pthread_mutex_t config_mutex = PTHREAD_MUTEX_INITIALIZER;

// Private function prototypes
static int MQTT_SetupClient();
static void connectionLost(void* context, char* cause);
static int messageArrived(void* context, char* topicName, int topicLen, MQTTAsync_message* message);
static void deliveryComplete(void* context, MQTTAsync_token token);
static void onConnectFailure(void* context, MQTTAsync_failureData* response);
static void onConnect(void* context, MQTTAsync_successData* response);
static void onDisconnect(void* context, MQTTAsync_successData* response);
static void onReconnect(void* context, char* cause);
//static gboolean reconnect_task(gpointer user_data);

static int  MQTT_Load_Settings(void);
static int  MQTT_Load_Library(void);
static int  MQTT_Connect(void);
static int  MQTT_SetupClient(void);
static void MQTT_HTTP_callback(const ACAP_HTTP_Response response, const ACAP_HTTP_Request request);


cJSON*
MQTT_Settings() {
	return MQTTSettings;
}

int
MQTT_Init(MQTT_Callback_Connection stateCallback, MQTT_Callback_Message messageCallback) {
	LOG_TRACE("%s:\n",__func__);
    connectionCallback = stateCallback;
    userSubscriptionCallback = messageCallback;

    ACAP_HTTP_Node("mqtt", MQTT_HTTP_callback);
    if (!MQTT_Load_Settings()) return 0;
    if (!MQTT_Load_Library()) return 0;
    if (!MQTT_SetupClient()) return 0;
    
    CERTS_Init();
    connectionCallback(MQTT_CONNECTING);
	
	MQTT_Connect();
    return 1;
}

int
MQTT_Connect() {
    LOG_TRACE("%s:\n", __func__);
	ACAP_STATUS_SetString("mqtt","status","Connecting");
	ACAP_STATUS_SetBool("mqtt","connected",0);
    
    if (!mqtt_client) {
        LOG_WARN("%s: Invalid mqtt-client\n", __func__);
        return 0;
    }
    
    // Static storage to prevent dangling pointers - CRITICAL FIX
    static MQTTAsync_SSLOptions ssl_opts = MQTTAsync_SSLOptions_initializer;
    static MQTTAsync_willOptions will_opts = MQTTAsync_willOptions_initializer;
    
    MQTTAsync_connectOptions conn_opts = MQTTAsync_connectOptions_initializer;
    
    // Essential connection parameters
    conn_opts.keepAliveInterval = 60;
    conn_opts.cleansession = 1;
    conn_opts.automaticReconnect = 1;
    conn_opts.minRetryInterval = 5;
    conn_opts.maxRetryInterval = 60;
    conn_opts.connectTimeout = 30;  // Add connection timeout
    conn_opts.onSuccess = onConnect;
    conn_opts.onFailure = onConnectFailure;
    conn_opts.context = mqtt_client;
    
    // Authentication configuration
    cJSON* user_item = cJSON_GetObjectItem(MQTTSettings, "user");
    cJSON* password_item = cJSON_GetObjectItem(MQTTSettings, "password");
    
    if (user_item && user_item->valuestring && strlen(user_item->valuestring)) {
        conn_opts.username = user_item->valuestring;
    }
    
    // Fixed password validation bug - was checking user_item instead of password_item
    if (password_item && password_item->valuestring && strlen(password_item->valuestring)) {
        conn_opts.password = password_item->valuestring;
    }

    // TLS Configuration -
	if (cJSON_IsTrue(cJSON_GetObjectItem(MQTTSettings, "tls"))) {
		const char* caCert = CERTS_Get_CA();
		const char* cert = CERTS_Get_Cert();
		const char* key = CERTS_Get_Key();
		const char* password = CERTS_Get_Password();
		
		LOG_TRACE("%s: Initializing TLS", __func__);
		ssl_opts = (MQTTAsync_SSLOptions)MQTTAsync_SSLOptions_initializer;
		ssl_opts.sslVersion = 3; // TLS 1.2
		
		// Always set trust store if available, regardless of length
		if (caCert && strlen(caCert) > 0) {
			ssl_opts.trustStore = caCert;
			LOG_TRACE("TLS: Trust store configured");
		} else {
			LOG_WARN("TLS: No CA certificate available");
		}
		
		// Client certificate configuration
		if (cert && strlen(cert) > 0 && key && strlen(key) > 0) {
			ssl_opts.keyStore = cert;
			ssl_opts.privateKey = key;
			if (password && strlen(password) > 0) {
				ssl_opts.privateKeyPassword = password;
			}
			LOG_TRACE("TLS: Client certificate configured");
		}
		
		ssl_opts.enableServerCertAuth = cJSON_IsTrue(cJSON_GetObjectItem(MQTTSettings, "verify"));
		LOG_TRACE("TLS: Server cert auth = %d", ssl_opts.enableServerCertAuth);
		
		conn_opts.ssl = &ssl_opts;
	}
    
    // Last Will Configuration - Now using static storage
    will_opts = (MQTTAsync_willOptions)MQTTAsync_willOptions_initializer;
    
    cJSON* lwt = cJSON_CreateObject();
    if (!lwt) {
        LOG_WARN("%s: Failed to create LWT JSON object\n", __func__);
        return 0;
    }
    
    cJSON_AddFalseToObject(lwt, "connected");
    cJSON_AddStringToObject(lwt, "address", ACAP_DEVICE_Prop("IPv4"));
    
    // Add additional payload properties
    cJSON* additionalProperties = cJSON_GetObjectItem(MQTTSettings, "payload");
    if (additionalProperties) {
        cJSON* name_item = cJSON_GetObjectItem(additionalProperties, "name");
        cJSON* location_item = cJSON_GetObjectItem(additionalProperties, "location");
        
        if (name_item && name_item->valuestring && strlen(name_item->valuestring)) {
            cJSON_AddStringToObject(lwt, "name", name_item->valuestring);
        }
        if (location_item && location_item->valuestring && strlen(location_item->valuestring)) {
            cJSON_AddStringToObject(lwt, "location", location_item->valuestring);
        }
    }
    cJSON_AddStringToObject(lwt, "serial", ACAP_DEVICE_Prop("serial"));
    
    // Construct Last Will Topic with bounds checking
    cJSON* preTopic_item = cJSON_GetObjectItem(MQTTSettings, "preTopic");
    if (preTopic_item && preTopic_item->valuestring && strlen(preTopic_item->valuestring)) {
        int result = snprintf(LastWillTopic, sizeof(LastWillTopic), "%s/connect/%s",
                             preTopic_item->valuestring, ACAP_DEVICE_Prop("serial"));
        if (result >= sizeof(LastWillTopic)) {
            LOG_WARN("%s: Last Will Topic truncated\n", __func__);
        }
    } else {
        int result = snprintf(LastWillTopic, sizeof(LastWillTopic), "connect/%s", 
                             ACAP_DEVICE_Prop("serial"));
        if (result >= sizeof(LastWillTopic)) {
            LOG_WARN("%s: Last Will Topic truncated\n", __func__);
        }
    }
    
    // Create Last Will message with proper error handling
    char* json = cJSON_PrintUnformatted(lwt);
    if (json) {
        int result = snprintf(LastWillMessage, sizeof(LastWillMessage), "%s", json);
        if (result >= sizeof(LastWillMessage)) {
            LOG_WARN("%s: Last Will Message truncated\n", __func__);
        }
        
        will_opts.topicName = LastWillTopic;
        will_opts.message = LastWillMessage;
        will_opts.retained = 1;
        will_opts.qos = 0;
        conn_opts.will = &will_opts;  // Now safe - static storage
        
        free(json);
    } else {
        LOG_WARN("%s: Failed to serialize Last Will message\n", __func__);
    }
    
    cJSON_Delete(lwt);

    // Attempt connection
    int rc = mqtt.connect(mqtt_client, &conn_opts);
    if (rc != MQTTASYNC_SUCCESS) {
        LOG_WARN("%s: Unable to initialize MQTT connection. Code %d\n", __func__, rc);
        
        // Log detailed error information
        switch (rc) {
            case MQTTASYNC_FAILURE:
                LOG_WARN("MQTT Connect: General failure\n");
                break;
            case MQTTASYNC_PERSISTENCE_ERROR:
                LOG_WARN("MQTT Connect: Persistence error\n");
                break;
            case MQTTASYNC_DISCONNECTED:
                LOG_WARN("MQTT Connect: Client disconnected\n");
                break;
            case MQTTASYNC_MAX_MESSAGES_INFLIGHT:
                LOG_WARN("MQTT Connect: Max messages in flight\n");
                break;
            case MQTTASYNC_BAD_UTF8_STRING:
                LOG_WARN("MQTT Connect: Bad UTF8 string\n");
                break;
            case MQTTASYNC_NULL_PARAMETER:
                LOG_WARN("MQTT Connect: Null parameter\n");
                break;
            case MQTTASYNC_TOPICNAME_TRUNCATED:
                LOG_WARN("MQTT Connect: Topic name truncated\n");
                break;
            case MQTTASYNC_BAD_STRUCTURE:
                LOG_WARN("MQTT Connect: Bad structure\n");
                break;
            case MQTTASYNC_BAD_QOS:
                LOG_WARN("MQTT Connect: Bad QoS\n");
                break;
            default:
                LOG_WARN("MQTT Connect: Unknown error code %d\n", rc);
                break;
        }
        
        return 0;
    } else {
        LOG_TRACE("%s: Connection initialized successfully\n", __func__);
    }

    cJSON *connect_item = cJSON_GetObjectItem(MQTTSettings, "connect");
    if (connect_item) {
        connect_item->type = cJSON_True;
		ACAP_FILE_Write("localdata/mqtt.json", MQTTSettings);
	}
   
    return 1;
}

int
MQTT_Disconnect() {
    if (!mqtt_client) return 0;

	ACAP_STATUS_SetString("mqtt","status","Disconnection");
	ACAP_STATUS_SetBool("mqtt","connected",0);
    
    MQTTAsync_disconnectOptions disc_opts = MQTTAsync_disconnectOptions_initializer;
    disc_opts.onSuccess = onDisconnect;
    disc_opts.context = mqtt_client;
    
    return (mqtt.disconnect(mqtt_client, &disc_opts) == MQTTASYNC_SUCCESS);
}

int
MQTT_Publish(const char *topic, const char *payload, int qos, int retained) {
//    LOG_TRACE("%s:\n",__func__);
    if (!mqtt_client || !mqtt.isConnected(mqtt_client)) {
        return 0;
    }
    
    if (!topic || !payload) {
        LOG_WARN("%s: Invalid parameters\n", __func__);
        return 0;
    }

    char *preTopic = cJSON_GetObjectItem(MQTTSettings, "preTopic") ? 
                     cJSON_GetObjectItem(MQTTSettings, "preTopic")->valuestring : NULL;
    char fullTopic[256];
    
    if (preTopic && strlen(preTopic) > 0) {
        int result = snprintf(fullTopic, sizeof(fullTopic), "%s/%s", preTopic, topic);
        if (result >= sizeof(fullTopic)) {
            LOG_WARN("%s: Topic too long (truncated)\n", __func__);
            // Continue anyway - topic will be truncated but still valid
        }
    } else {
        strncpy(fullTopic, topic, sizeof(fullTopic) - 1);
        fullTopic[sizeof(fullTopic) - 1] = '\0';
    }

//    LOG_TRACE("%s: %s %s\n",__func__,fullTopic,payload);

    MQTTAsync_message pubmsg = MQTTAsync_message_initializer;
    pubmsg.payload = (void*)payload;
    pubmsg.payloadlen = strlen(payload);
    pubmsg.qos = qos;
    pubmsg.retained = retained;

    MQTTAsync_responseOptions opts = MQTTAsync_responseOptions_initializer;
    opts.context = mqtt_client;
    
    int rc = mqtt.sendMessage(mqtt_client, fullTopic, &pubmsg, &opts);
    if( rc != MQTTASYNC_SUCCESS )
        LOG_TRACE("%s: Published failed\n",__func__);
    
    return (rc == MQTTASYNC_SUCCESS);
}

int
MQTT_Publish_JSON(const char *topic, cJSON *payload, int qos, int retained) {

    if (!mqtt_client || !mqtt.isConnected(mqtt_client)) {
        return 0;
    }
    
    if (!payload) {
        LOG_WARN("%s: NULL payload\n", __func__);
        return 0;
    }

    cJSON* publish = cJSON_Duplicate(payload, 1);
    if (!publish) {
        LOG_WARN("%s: Failed to duplicate JSON\n", __func__);
        return 0;
    }
    
    cJSON* additional = cJSON_GetObjectItem(MQTTSettings, "payload");
    if(additional) {
        // Fixed: Proper NULL checking
        cJSON* name_item = cJSON_GetObjectItem(additional, "name");
        cJSON* location_item = cJSON_GetObjectItem(additional, "location");
        
        if(name_item && name_item->valuestring && strlen(name_item->valuestring)) {
            cJSON_AddStringToObject(publish, "name", name_item->valuestring);
        }
        if(location_item && location_item->valuestring && strlen(name_item->valuestring)) {
            cJSON_AddStringToObject(publish, "location", location_item->valuestring);
        }
    }
    
    const char* serial = ACAP_DEVICE_Prop("serial");
    if (serial) {
        cJSON_AddStringToObject(publish, "serial", serial);
    }
    
    char* json = cJSON_PrintUnformatted(publish);
    int result = 0;
    
    if (json) {
        result = MQTT_Publish(topic, json, qos, retained);
        free(json);
    } else {
        LOG_WARN("%s: Failed to serialize JSON\n", __func__);
    }
    
    cJSON_Delete(publish);
    return result;
}

int
MQTT_Publish_Binary(const char *topic, int payloadlen, void *payload, int qos, int retained) {
    
    if (!mqtt_client || !mqtt.isConnected(mqtt_client)) {
        return 0;
    }
    
    if (!topic || !payload || payloadlen <= 0) {
        LOG_WARN("%s: Invalid parameters\n", __func__);
        return 0;
    }

    char *preTopic = cJSON_GetObjectItem(MQTTSettings, "preTopic") ? cJSON_GetObjectItem(MQTTSettings, "preTopic")->valuestring : NULL;
    char *fullTopic = NULL;
    
    if (preTopic && strlen(preTopic) > 0) {
        fullTopic = malloc(strlen(preTopic) + strlen(topic) + 2); // +2 for '/' and null terminator
        if (!fullTopic) {
            LOG_WARN("%s: Failed to allocate memory for full topic\n", __func__);
            return 0;
        }
        sprintf(fullTopic, "%s/%s", preTopic, topic);
    } else {
        fullTopic = (char *)topic;
    }



    MQTTAsync_message pubmsg = MQTTAsync_message_initializer;
    pubmsg.payload = (void*)payload;
    pubmsg.payloadlen = payloadlen;
    pubmsg.qos = qos;
    pubmsg.retained = retained;

    MQTTAsync_responseOptions opts = MQTTAsync_responseOptions_initializer;
    opts.context = mqtt_client;
   
    int rc = mqtt.sendMessage(mqtt_client, fullTopic, &pubmsg, &opts);
    
    return (rc == MQTTASYNC_SUCCESS);
}

int
MQTT_Subscribe(const char *topic) {
    if (!mqtt.isConnected(mqtt_client)) return 0;

    MQTTAsync_responseOptions opts = MQTTAsync_responseOptions_initializer;
    return (mqtt.subscribe(mqtt_client, topic, 0, &opts) == MQTTASYNC_SUCCESS);
}

int
MQTT_Unsubscribe(const char *topic) {
    if (!mqtt.isConnected(mqtt_client)) return 0;

    MQTTAsync_responseOptions opts = MQTTAsync_responseOptions_initializer;
    return (mqtt.unsubscribe(mqtt_client, topic, &opts) == MQTTASYNC_SUCCESS);
}

static int
MQTT_Load_Library() {
    const char* lib_name = "libpaho-mqtt3as.so.1";  // Always load SSL-enabled library

    MQTT_libHandle = dlopen(lib_name, RTLD_LAZY);
    if (!MQTT_libHandle) {
        LOG_WARN("Failed to load MQTT library: %s\n", dlerror());
        return 0;
    }
	
    #define LOAD_SYMBOL(sym) \
        if (!(mqtt.sym = dlsym(MQTT_libHandle, "MQTTAsync_" #sym))) { \
            LOG_WARN("Failed to load symbol: MQTTAsync_" #sym "\n"); \
            return 0; \
        }

    LOAD_SYMBOL(create)
    LOAD_SYMBOL(connect)
    LOAD_SYMBOL(disconnect)
    LOAD_SYMBOL(isConnected)
    LOAD_SYMBOL(send)
    LOAD_SYMBOL(sendMessage)
    LOAD_SYMBOL(subscribe)
    LOAD_SYMBOL(unsubscribe)
    LOAD_SYMBOL(setCallbacks)
    LOAD_SYMBOL(freeMessage)
	LOAD_SYMBOL(free)
	LOAD_SYMBOL(setConnected)
    LOAD_SYMBOL(destroy)

    return 1;
}

static int
MQTT_SetupClient() {
	LOG_TRACE("%s:\n",__func__);
    
    /* Validate settings structure */
    cJSON *address_item = cJSON_GetObjectItem(MQTTSettings, "address");
    cJSON *port_item = cJSON_GetObjectItem(MQTTSettings, "port");
    cJSON *tls_item = cJSON_GetObjectItem(MQTTSettings, "tls");
    
    if (!address_item || !port_item || !tls_item) {
        LOG_WARN("%s: Invalid MQTT settings structure\n", __func__);
        return 0;
    }

    const char* address = address_item->valuestring;
    const char* port = port_item->valuestring;
    const char* scheme = cJSON_IsTrue(tls_item) ? "ssl" : "tcp";

    /* Construct server URI with bounds checking */
    char serverURI[256];
    int uri_len = snprintf(serverURI, sizeof(serverURI), "%s://%s:%s", 
                          scheme, address, port);
    if (uri_len >= sizeof(serverURI)) {
        LOG_WARN("%s: Server URI too long (max %zu chars)\n", 
                __func__, sizeof(serverURI)-1);
        return 0;
    }

    /* Generate unique client ID */
    char clientId[128];
    snprintf(clientId, sizeof(clientId), "%s-%s", 
            ACAP_Name(), ACAP_DEVICE_Prop("serial"));

    /* Create client instance */
    int rc = mqtt.create(&mqtt_client, serverURI, clientId, MQTTASYNC_PERSISTENCE_NONE, NULL);
    if (rc != MQTTASYNC_SUCCESS) {
        LOG_WARN("%s: Client creation failed: %d\n", __func__, rc);
        return 0;
    }

    /* Set callbacks with validation */
	mqtt.setConnected(mqtt_client, NULL, onReconnect);	
    rc = mqtt.setCallbacks(mqtt_client, NULL, connectionLost, messageArrived, deliveryComplete);
    if (rc != MQTTASYNC_SUCCESS) {
        LOG_WARN("%s: Failed to set callbacks: %d\n", __func__, rc);
        mqtt.destroy(&mqtt_client);
        return 0;
    }

    return 1;
}

// Callback implementations
static void connectionLost(void* context, char* cause) {
	ACAP_STATUS_SetString("mqtt","status","Connection lost");
	ACAP_STATUS_SetBool("mqtt","connected",0);
	
    LOG_WARN("Connection lost: %s\n", cause ? cause : "unknown reason");
    
    connectionCallback(MQTT_RECONNECTING);
}

static int
messageArrived(void* context, char* topicName, int topicLen, MQTTAsync_message* message) {
/*	
    if (userSubscriptionCallback) {
        // Create null-terminated copy for callback
        char *payload = malloc(message->payloadlen + 1);
        if (payload) {
            memcpy(payload, message->payload, message->payloadlen);
            payload[message->payloadlen] = '\0';
            userSubscriptionCallback(topicName, payload);
            free(payload);  // Free our copy, not the original
        }
    }
    
    mqtt.freeMessage(&message);  // Proper cleanup
    mqtt.free(topicName);
*/	
    return 1;
}

static void
deliveryComplete(void* context, MQTTAsync_token token) {
    LOG("Message delivery confirmed for token %d\n", token);
}

static void
onConnect(void* context, MQTTAsync_successData* response) {
	ACAP_STATUS_SetString("mqtt","status","Connected");
	ACAP_STATUS_SetBool("mqtt","connected",1);

    LOG_TRACE("%s: Connection established to %s\n",__func__, response ? response->alt.connect.serverURI : "unknown");
    connectionCallback(MQTT_CONNECTED);
	LOG_TRACE("%s: Exit\n",__func__);
}

static void
onReconnect(void* context, char* cause) {
    LOG("%s: Reconnected to MQTT broker.  %s\n",__func__, cause?cause:"Unknown");
	ACAP_STATUS_SetString("mqtt","status","Connected");
	ACAP_STATUS_SetBool("mqtt","connected",1);
//    connectionCallback(MQTT_RECONNECTED);
	LOG_TRACE("%s: Exit\n",__func__);
}

static void
onDisconnect(void* context, MQTTAsync_successData* response) {
	LOG_TRACE("%s:\n",__func__);
	ACAP_STATUS_SetString("mqtt","status","Disconnected");
	ACAP_STATUS_SetBool("mqtt","connected",0);
    connectionCallback(MQTT_DISCONNECTED);
}

static void
onConnectFailure(void* context, MQTTAsync_failureData* response) {
    char text[256] = "Connection failed";
    
    if (response) {
        if (response->message) {
            snprintf(text, sizeof(text), "%s (code: %d)", 
                    response->message, response->code);
        } else {
            snprintf(text, sizeof(text), "Connection failed. Code: %d", response->code);
        }
    }
    
    LOG_WARN("%s", text);

	ACAP_STATUS_SetString("mqtt","status",text);
	ACAP_STATUS_SetBool("mqtt","connected",0);
    
    connectionCallback(MQTT_DISCONNECTED);
}


void
MQTT_Cleanup() {
    LOG_TRACE("%s:\n", __func__);
    
    pthread_mutex_lock(&config_mutex);
    
    if (mqtt_client) {
        // Check if already connected before attempting disconnect
        if (mqtt.isConnected && mqtt.isConnected(mqtt_client)) {
            // Synchronous disconnect with timeout
            MQTTAsync_disconnectOptions opts = MQTTAsync_disconnectOptions_initializer;
            opts.timeout = 5000;  // 5 second timeout
            
            pthread_mutex_unlock(&config_mutex);  // Release mutex before async call
            
            int rc = mqtt.disconnect(mqtt_client, &opts);
            if (rc == MQTTASYNC_SUCCESS) {
                // Wait for disconnect to complete (simple polling approach)
                int wait_count = 0;
                while (mqtt.isConnected(mqtt_client) && wait_count < 50) {
                    usleep(100000);  // 100ms
                    wait_count++;
                }
            }
            
            pthread_mutex_lock(&config_mutex);  // Reacquire for destroy
        }
        
        // Now safe to destroy
        mqtt.destroy(&mqtt_client);
        mqtt_client = NULL;
    }
    
    // Clean up library handle
    if (MQTT_libHandle) {
        dlclose(MQTT_libHandle);
        MQTT_libHandle = NULL;
    }
    
    // Clean up settings
    if (MQTTSettings) {
        cJSON_Delete(MQTTSettings);
        MQTTSettings = NULL;
    }
    
    pthread_mutex_unlock(&config_mutex);
    LOG_TRACE("%s: Exit\n", __func__);
}

// Remaining helper functions from original implementation
static int
MQTT_Load_Settings() {
    if (MQTTSettings) return 1;

    MQTTSettings = ACAP_FILE_Read("settings/mqtt.json");
    if (!MQTTSettings) {
		LOG_WARN("%s: Unable to read default configuration\n",__func__);
		return 0;
	}

    cJSON* saved = ACAP_FILE_Read("localdata/mqtt.json");
    if (saved) {
        cJSON* item = saved->child;
        while(item) {
            cJSON_ReplaceItemInObject(MQTTSettings, item->string, cJSON_Duplicate(item, 1));
            item = item->next;
        }
        cJSON_Delete(saved);
    }
    return 1;
}

static void
MQTT_HTTP_callback(const ACAP_HTTP_Response response, const ACAP_HTTP_Request request)
{
    pthread_mutex_lock(&config_mutex); // Lock configuration mutex
    
    // 1. Handle initial state checks
    if (!MQTTSettings) {
        ACAP_HTTP_Respond_Error(response, 500, "MQTT not initialized");
        syslog(LOG_ERR, "MQTT settings not initialized");
        pthread_mutex_unlock(&config_mutex);
        return;
    }

    const char* action = ACAP_HTTP_Request_Param(request, "action");
    const char* json = ACAP_HTTP_Request_Param(request, "json");
    int full_reinit_required = 0;

    if (action && strcmp(action, "disconnect") == 0) {
        if (connectionCallback) connectionCallback(MQTT_DISCONNECTING);
        
        if (MQTT_Disconnect()) {
            cJSON_GetObjectItem(MQTTSettings, "connect")->type = cJSON_False;
            ACAP_FILE_Write("localdata/mqtt.json", MQTTSettings);
            ACAP_HTTP_Respond_Text(response, "Disconnected");
        } else {
            ACAP_HTTP_Respond_Error(response, 500, "Disconnect failed");
        }
        pthread_mutex_unlock(&config_mutex);
        return;
    }

    if (!json) {
        ACAP_HTTP_Respond_JSON(response, MQTTSettings);
        pthread_mutex_unlock(&config_mutex);
        return;
    }

    cJSON *new_settings = cJSON_Parse(json);
    if (!new_settings) {
        ACAP_HTTP_Respond_Error(response, 400, "Invalid JSON");
        pthread_mutex_unlock(&config_mutex);
        return;
    }

    cJSON *payload = cJSON_GetObjectItem(new_settings, "payload");
    if (payload) {
        cJSON *mqtt_payload = cJSON_GetObjectItem(MQTTSettings, "payload");
        if (!mqtt_payload) {
            mqtt_payload = cJSON_CreateObject();
            cJSON_AddItemToObject(MQTTSettings, "payload", mqtt_payload);
        }
        
        cJSON *item = payload->child;
        while (item) {
            cJSON_ReplaceItemInObject(mqtt_payload, item->string, cJSON_Duplicate(item, 1));
            item = item->next;
        }
        
        ACAP_FILE_Write("localdata/mqtt.json", MQTTSettings);
        ACAP_HTTP_Respond_Text(response, "Payload updated");
        cJSON_Delete(new_settings);
        pthread_mutex_unlock(&config_mutex);
        return;
    }

    cJSON *item = new_settings->child;
    while (item) {
        const char *key = item->string;
        cJSON *existing = cJSON_GetObjectItem(MQTTSettings, key);
        
        if (existing) {
            // Check if parameter requires reinitialization
            if (strcmp(key, "address") == 0 || strcmp(key, "port") == 0 || 
                strcmp(key, "user") == 0 || strcmp(key, "password") == 0) {
                full_reinit_required = 1;
            }
            cJSON_ReplaceItemInObject(MQTTSettings, key, cJSON_Duplicate(item, 1));
        }
        item = item->next;
    }

    if (!ACAP_FILE_Write("localdata/mqtt.json", MQTTSettings)) {
        ACAP_HTTP_Respond_Error(response, 500, "Failed to save settings");
        cJSON_Delete(new_settings);
        pthread_mutex_unlock(&config_mutex);
        return;
    }

    if (full_reinit_required) {
        LOG_TRACE("%s Performing full MQTT reinitialization\n",__func__);
        
        if (mqtt_client) {
            MQTT_Disconnect();
            mqtt.destroy(&mqtt_client);
            mqtt_client = NULL;
        }

        if (!MQTT_SetupClient() ) {
            ACAP_HTTP_Respond_Error(response, 500, "Reinitialization failed");
            pthread_mutex_unlock(&config_mutex);
            return;
        }
    }

	if( MQTT_Connect() ) {
		ACAP_HTTP_Respond_Text(response, "Connecting");
	} else {
		ACAP_HTTP_Respond_Error(response, 500, "Error initializing connection");
	}
    
    cJSON_Delete(new_settings);
    pthread_mutex_unlock(&config_mutex); // Release configuration mutex
}
