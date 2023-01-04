#include <stdio.h>
#include <stdlib.h>
#include <glib.h>
#include <glib/gi18n.h>
#include <string.h>
#include <syslog.h>

#include "APP.h"
#include "FILE.h"
#include "HTTP.h"

#define LOG(fmt, args...)    { syslog(LOG_INFO, fmt, ## args); printf(fmt, ## args);}
#define LOG_WARN(fmt, args...)    { syslog(LOG_WARNING, fmt, ## args); printf(fmt, ## args);}
#define LOG_TRACE(fmt, args...)    { syslog(LOG_INFO, fmt, ## args); printf(fmt, ## args); }
//#define LOG_TRACE(fmt, args...)    {}

static GMainLoop *main_loop = NULL;


static void
MAIN_http_test(const HTTP_Response response,const HTTP_Request request) {
	HTTP_Respond_Text( response, "OK" );
}

int 
main(void) {
	openlog(APP_PACKAGE, LOG_PID|LOG_CONS, LOG_USER);
	
	FILE_Init( APP_PACKAGE );
	HTTP_Init( APP_PACKAGE );
	APP_Init();

	//Get parameters and print in syslog
	cJSON* app_parameters = APP_Settings();
	char* json = cJSON_PrintUnformatted(app_parameters);
	if( json ) {
		LOG("%s\n",json);
		free(json);
	}

    //Setup an HTTP node to test HTTP request
	HTTP_Node( "test", MAIN_http_test );	

	main_loop = g_main_loop_new(NULL, FALSE);
	g_main_loop_run(main_loop);

	return 0;
}
