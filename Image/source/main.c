#include <stdio.h>
#include <stdlib.h>
#include <glib.h>
#include <glib/gi18n.h>
#include <string.h>
#include <syslog.h>
#include "vdo-stream.h"
#include "APP.h"
#include "HTTP.h"

#define LOG(fmt, args...)    { syslog(LOG_INFO, fmt, ## args); printf(fmt, ## args);}
#define LOG_WARN(fmt, args...)    { syslog(LOG_WARNING, fmt, ## args); printf(fmt, ## args);}
#define LOG_TRACE(fmt, args...)    { syslog(LOG_INFO, fmt, ## args); printf(fmt, ## args); }
//#define LOG_TRACE(fmt, args...)    {}

#define APP_PACKAGE	"image"

static void
MAIN_http_capture(const HTTP_Response response,const HTTP_Request request) {
	LOG_TRACE("%s: \n",__func__);
	GError *error = NULL;
	const char* param;
	int width = 640;
	int height = 360;

	param = HTTP_Request_Param(request, "width");
	if( param )
		width = atoi(param);
	
	param = HTTP_Request_Param(request, "height");
	if( param )
		height = atoi(param);

    VdoMap *vdoSettings = vdo_map_new();
    vdo_map_set_uint32(vdoSettings, "format", VDO_FORMAT_JPEG);
    vdo_map_set_uint32(vdoSettings, "width", width);
    vdo_map_set_uint32(vdoSettings, "height", height);

    //take a snapshot
    VdoBuffer* buffer = vdo_stream_snapshot(vdoSettings, &error);
	g_clear_object(&vdoSettings);
	
    if (error != NULL){
        LOG_WARN("%s: %s",__func__,error->message);
		g_error_free( error );
		HTTP_Respond_Error( response, 503, "Image capture error" );
		return;
    }
	
    VdoFrame*  frame  = vdo_buffer_get_frame(buffer);
    if (!frame){
        LOG_WARN("%s: Image frame is empty",__func__);
		HTTP_Respond_Error( response, 503, "Image frame is empty" );
		return;
    }

	void *data = (void *)vdo_buffer_get_data(buffer);
    unsigned int size = vdo_frame_get_size(frame);

	HTTP_Respond_String( response, "status: 200 OK\r\n");
	HTTP_Respond_String( response, "Content-Description: File Transfer\r\n");
	HTTP_Respond_String( response, "Content-Type: image/jpeg\r\n");
	HTTP_Respond_String( response, "Content-Disposition: attachment; filename=image.jpeg\r\n");
	HTTP_Respond_String( response, "Content-Transfer-Encoding: binary\r\n");
	HTTP_Respond_String( response, "Content-Length: %u\r\n", size );
	HTTP_Respond_String( response, "\r\n");
	HTTP_Respond_Data( response, size, data );
	
    g_object_unref(buffer);

}


static GMainLoop *main_loop = NULL;

int 
main(void) {
	openlog(APP_PACKAGE, LOG_PID|LOG_CONS, LOG_USER);
	
	//Initialize ACAP resources
	APP( APP_PACKAGE, NULL ); //No settings

	HTTP_Node( "capture", MAIN_http_capture );	

	main_loop = g_main_loop_new(NULL, FALSE);
	g_main_loop_run(main_loop);

	return 0;
}
