#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <glib.h>
#include <signal.h>

#include "vdo-stream.h"
#include "vdo-frame.h"
#include "vdo-types.h"
#include "ACAP.h"
#include "cJSON.h"

#define APP_PACKAGE	"base"

#define LOG(fmt, args...)    { syslog(LOG_INFO, fmt, ## args); printf(fmt, ## args);}
#define LOG_WARN(fmt, args...)    { syslog(LOG_WARNING, fmt, ## args); printf(fmt, ## args);}
//#define LOG_TRACE(fmt, args...)    { syslog(LOG_INFO, fmt, ## args); printf(fmt, ## args); }
#define LOG_TRACE(fmt, args...)    {}

static GMainLoop *main_loop = NULL;
static volatile sig_atomic_t shutdown_flag = 0;

void term_handler(int signum) {
    if (signum == SIGTERM) {
        shutdown_flag = 1;
        if (main_loop) {
            g_main_loop_quit(main_loop);
        }
    }
}

void cleanup_resources(void) {
    LOG("Performing cleanup before shutdown\n");
    ACAP_Cleanup();
    closelog();
}


void
Settings_Updated_Callback( const char* service, cJSON* data) {
	char* json = cJSON_PrintUnformatted(data);
	LOG_TRACE("%s: Service=%s Data=%s\n",__func__, service, json);
	free(json);
}


void
HTTP_Endpoint_fire(const ACAP_HTTP_Response response, const ACAP_HTTP_Request request) {
	//Note the the events are declared in ./app/html/config/events.json
	
    const char* method = ACAP_HTTP_Get_Method(request);
    if (!method) {
        ACAP_HTTP_Respond_Error(response, 400, "Invalid Request Method");
        return;
    }

    if (strcmp(method, "GET") != 0) {
        ACAP_HTTP_Respond_Error(response, 400, "Invalid Request Method");
		return;
	}

	
    const char* id = ACAP_HTTP_Request_Param(request, "id");
    const char* value_str = ACAP_HTTP_Request_Param(request, "value");
    int state = value_str ? atoi(value_str) : 0;

	LOG("Event fired %s %d\n",id,state);

	if(id) {
		LOG_TRACE("%s: Event id: %s\n",__func__,id);
	} else {
		LOG_WARN("%s: Missing event id\n",__func__);
		ACAP_HTTP_Respond_Error( response, 500, "Invalid event ID" );
		return;
	}

	if(value_str) {
		LOG_TRACE("%s: Event value: %s\n",__func__, value_str);
	}

	if( strcmp( id, "state" ) == 0 ) {
		ACAP_EVENTS_Fire_State( id, state );
		ACAP_HTTP_Respond_Text( response, "State event fired" );
		return;
	}
	if( strcmp( id, "trigger" ) == 0 ) {
		ACAP_EVENTS_Fire( id );
		ACAP_HTTP_Respond_Text( response, "Trigger event fired" );
		return;
	}
	ACAP_HTTP_Respond_Error( response, 500, "Invalid event ID" );
}

void
HTTP_Endpoint_capture(const ACAP_HTTP_Response response, const ACAP_HTTP_Request request) {
    const char* method = ACAP_HTTP_Get_Method(request);
    if (!method) {
        ACAP_HTTP_Respond_Error(response, 400, "Invalid Request Method");
        return;
    }

    if (strcmp(method, "GET") != 0) {
        ACAP_HTTP_Respond_Error(response, 400, "Invalid Request Method");
		return;
	}

    const char* width_str = ACAP_HTTP_Request_Param(request, "width");
    const char* height_str = ACAP_HTTP_Request_Param(request, "height");
	
    int width = width_str ? atoi(width_str) : 1920;
    int height = height_str ? atoi(height_str) : 1080;
	LOG("Image capture %dx%d\n",width,height);

    // Create VDO settings for snapshot
    VdoMap* vdoSettings = vdo_map_new();

    vdo_map_set_uint32(vdoSettings, "format", VDO_FORMAT_JPEG);
    vdo_map_set_uint32(vdoSettings, "width", width);
    vdo_map_set_uint32(vdoSettings, "height", height);

    // Take snapshot
    GError* error = NULL;
    VdoBuffer* buffer = vdo_stream_snapshot(vdoSettings, &error);
	g_clear_object(&vdoSettings);
	
    if (error != NULL) {
        // Handle error
        ACAP_HTTP_Respond_Error(response, 503, "Snapshot capture failed");
        g_error_free(error);
        return;
    }

    // Get snapshot data
    unsigned char* data = vdo_buffer_get_data(buffer);
    unsigned int size = vdo_frame_get_size(buffer);

    // Build HTTP response
	ACAP_HTTP_Respond_String( response, "status: 200 OK\r\n");
	ACAP_HTTP_Respond_String( response, "Content-Description: File Transfer\r\n");
	ACAP_HTTP_Respond_String( response, "Content-Type: image/jpeg\r\n");
	ACAP_HTTP_Respond_String( response, "Content-Disposition: attachment; filename=snapshot.jpeg\r\n");
	ACAP_HTTP_Respond_String( response, "Content-Transfer-Encoding: binary\r\n");
	ACAP_HTTP_Respond_String( response, "Content-Length: %u\r\n", size );
	ACAP_HTTP_Respond_String( response, "\r\n");
	ACAP_HTTP_Respond_Data( response, size, data );

    // Clean up
    g_object_unref(buffer);
}


int main(void) {
    struct sigaction action;
    
    // Initialize signal handling
    memset(&action, 0, sizeof(struct sigaction));
    action.sa_handler = term_handler;
    sigaction(SIGTERM, &action, NULL);
	
	openlog(APP_PACKAGE, LOG_PID|LOG_CONS, LOG_USER);
	LOG("------ Starting ACAP Service ------\n");
	ACAP_STATUS_SetString("app", "status", "The application is starting");	
	
	ACAP( APP_PACKAGE, Settings_Updated_Callback );
	ACAP_HTTP_Node( "capture", HTTP_Endpoint_capture );
	ACAP_HTTP_Node( "fire", HTTP_Endpoint_fire );

	g_idle_add(ACAP_HTTP_Process, NULL);
	main_loop = g_main_loop_new(NULL, FALSE);

    atexit(cleanup_resources);
	
	g_main_loop_run(main_loop);

    if (shutdown_flag) {
        LOG("Received SIGTERM signal, shutting down gracefully\n");
    }
    
    if (main_loop) {
        g_main_loop_unref(main_loop);
    }
	
    return 0;
}
