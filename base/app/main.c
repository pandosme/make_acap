#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <glib.h>
#include <glib-unix.h>
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

	LOG("Event fired %s %d\n", id ? id : "(null)", state);

	if(!id) {
		LOG_WARN("%s: Missing event id\n",__func__);
		free((void*)value_str);
		ACAP_HTTP_Respond_Error( response, 400, "Missing event ID" );
		return;
	}

	LOG_TRACE("%s: Event id: %s\n",__func__,id);
	if(value_str) {
		LOG_TRACE("%s: Event value: %s\n",__func__, value_str);
	}

	int handled = 0;
	if( strcmp( id, "state" ) == 0 ) {
		ACAP_EVENTS_Fire_State( id, state );
		ACAP_HTTP_Respond_Text( response, "State event fired" );
		handled = 1;
	} else if( strcmp( id, "trigger" ) == 0 ) {
		ACAP_EVENTS_Fire( id );
		ACAP_HTTP_Respond_Text( response, "Trigger event fired" );
		handled = 1;
	}

	free((void*)id);
	free((void*)value_str);

	if(!handled)
		ACAP_HTTP_Respond_Error( response, 400, "Invalid event ID" );
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
    free((void*)width_str);
    free((void*)height_str);
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
	ACAP_HTTP_Respond_String( response, "Status: 200 OK\r\n");
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


static GMainLoop *main_loop = NULL;

static gboolean
signal_handler(gpointer user_data) {
    LOG("Received SIGTERM, initiating shutdown\n");
    if (main_loop && g_main_loop_is_running(main_loop)) {
        g_main_loop_quit(main_loop);
    }
    return G_SOURCE_REMOVE;
}


int main(void) {
    openlog(APP_PACKAGE, LOG_PID|LOG_CONS, LOG_USER);
    LOG("------ Starting ACAP Service ------\n");

    ACAP(APP_PACKAGE, Settings_Updated_Callback);
    ACAP_STATUS_SetString("app", "status", "The application is starting");
    ACAP_HTTP_Node("capture", HTTP_Endpoint_capture);
    ACAP_HTTP_Node("fire", HTTP_Endpoint_fire);
    
    LOG("Entering main loop\n");
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
    closelog();
    return 0;
}
