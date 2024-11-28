#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <glib.h>
#include <glib-unix.h>
#include <signal.h>

#include "cJSON.h"
#include "ACAP.h"
#include "MQTT.h"

#define APP_PACKAGE	"basemqtt"

#define LOG(fmt, args...)    { syslog(LOG_INFO, fmt, ## args); printf(fmt, ## args);}
#define LOG_WARN(fmt, args...)    { syslog(LOG_WARNING, fmt, ## args); printf(fmt, ## args);}
//#define LOG_TRACE(fmt, args...)    { syslog(LOG_INFO, fmt, ## args); printf(fmt, ## args); }
#define LOG_TRACE(fmt, args...)    {}

void
Settings_Updated_Callback( const char* service, cJSON* data) {
	char* json = cJSON_PrintUnformatted(data);
	LOG_TRACE("%s: Service=%s Data=%s\n",__func__, service, json);
	free(json);
}

void
HTTP_ENDPOINT_Publish(ACAP_HTTP_Response response,const ACAP_HTTP_Request request) {
    const char* method = ACAP_HTTP_Get_Method(request);
    if (!method) {
        ACAP_HTTP_Respond_Error(response, 400, "Invalid Request Method");
        return;
    }

    if (strcmp(method, "POST") != 0) {
        ACAP_HTTP_Respond_Error(response, 400, "Invalid Request Method");
		return;
	}

	const char* contentType = ACAP_HTTP_Get_Content_Type(request);
	if (!contentType || strcmp(contentType, "application/json") != 0) {
		ACAP_HTTP_Respond_Error(response, 415, "Unsupported Media Type - Use application/json");
		return;
	}

	if (!request->postData || request->postDataLength == 0) {
		ACAP_HTTP_Respond_Error(response, 400, "Missing POST data");
		return;
	}

	cJSON* body = cJSON_Parse(request->postData);
	if (!body) {
		ACAP_HTTP_Respond_Error(response, 400, "Invalid JSON");
		LOG_WARN("Unable to parse json for MQTT settings\n");
		return;
	}
	const char* topic = cJSON_GetObjectItem(body,"topic")?cJSON_GetObjectItem(body,"topic")->valuestring:0;
	if( !topic || strlen(topic) == 0) {
		ACAP_HTTP_Respond_Error(response, 400, "Topic must be set");
		return;
	}
	const char* payload = cJSON_GetObjectItem(body,"payload")?cJSON_GetObjectItem(body,"payload")->valuestring:0;
	if( !payload ) {
		ACAP_HTTP_Respond_Error(response, 400, "Payload must be set");
		return;
	}
	if( MQTT_Publish( topic, payload, 0, 0 ) )
		ACAP_HTTP_Respond_Text(response, "Message sent");
	else
		ACAP_HTTP_Respond_Error(response, 500, "Message publish failed");
}

void 
Connection_Status (int state) {
	switch( state ) {
		case MQTT_CONNECT:
			LOG_TRACE("%s: Connect\n",__func__);
			break;
		case MQTT_RECONNECT:
			LOG("%s: Reconnect\n",__func__);
			break;
		case MQTT_DISCONNECT:
			LOG("%s: Disconnect\n",__func__);
			break;
	}
}

void
Subscription(const char *topic, const char *payload) {
	LOG("Subscription: %s %s\n",topic,payload);
}


static GMainLoop *main_loop = NULL;

static gboolean
signal_handler(gpointer user_data) {
    LOG("Received SIGTERM, initiating shutdown\n");
    if (main_loop && g_main_loop_is_running(main_loop)) {
        g_main_loop_quit(main_loop);
    }
    return G_SOURCE_REMOVE;
}

int
main(void) {
	openlog(APP_PACKAGE, LOG_PID|LOG_CONS, LOG_USER);
	LOG("------ Starting ACAP Service ------\n");
	
	ACAP( APP_PACKAGE, Settings_Updated_Callback );
	ACAP_HTTP_Node("publish", HTTP_ENDPOINT_Publish );

	MQTT_Init( APP_PACKAGE, Connection_Status );
	MQTT_Subscribe( "base_mqtt", Subscription );
	
	g_idle_add(ACAP_Process, NULL);
	main_loop = g_main_loop_new(NULL, FALSE);
    GSource *signal_source = g_unix_signal_source_new(SIGTERM);
    if (signal_source) {
		g_source_set_callback(signal_source, signal_handler, NULL, NULL);
		g_source_attach(signal_source, NULL);
	} else {
		LOG_WARN("Signal detection failed");
	}

	g_main_loop_run(main_loop);
	LOG("Terminating and cleaning up %s\n",APP_PACKAGE);
	ACAP_Cleanup();
	MQTT_Cleanup();
	
    return 0;
}
