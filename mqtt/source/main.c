#include <stdio.h>
#include <stdlib.h>
#include <glib.h>
#include <glib/gi18n.h>
#include <string.h>
#include <syslog.h>

#include "APP.h"
#include "HTTP.h"
#include "FILE.h"
#include "MQTTD.h"

#define LOG(fmt, args...)    { syslog(LOG_INFO, fmt, ## args); printf(fmt, ## args);}
#define LOG_WARN(fmt, args...)    { syslog(LOG_WARNING, fmt, ## args); printf(fmt, ## args);}
#define LOG_TRACE(fmt, args...)    { syslog(LOG_INFO, fmt, ## args); printf(fmt, ## args); }
//#define LOG_TRACE(fmt, args...)    {}

static GMainLoop *main_loop = NULL;


static void
MAIN_http_publish(const HTTP_Response response,const HTTP_Request request) {

	const char* topic = HTTP_Request_Param( request, "topic");
	const char* payload = HTTP_Request_Param( request, "payload");

	int status = MQTTD_Publish( topic, payload, 0);

	switch( status ) {
		case MQTTD_OK:
			HTTP_Respond_Text( response, "OK" );
			break;
		case MQTTD_NO_SIMQTT:
			HTTP_Respond_Error( response, 500, "SIMQTT is not running" );
			break;
		case MQTTD_BROKER:
			HTTP_Respond_Error( response, 500, "SIMQTT is not connected to broker" );
			break;
		case MQTTD_INVALID:
			HTTP_Respond_Error( response, 400, "Invalid topic or payload" );
			break;
		default:
			HTTP_Respond_Error( response, 500, "Message publish failed" );
			break;
	}
}

int 
main(void) {
	openlog(APP_PACKAGE, LOG_PID|LOG_CONS, LOG_USER);
	
	FILE_Init( APP_PACKAGE );
	HTTP_Init( APP_PACKAGE );
//	DEVICE_Init( APP_PACKAGE );
//	STATUS_Init( APP_PACKAGE );
	APP_Init();

	HTTP_Node( "publish", MAIN_http_publish );	

	main_loop = g_main_loop_new(NULL, FALSE);
	g_main_loop_run(main_loop);

	return 0;
}
