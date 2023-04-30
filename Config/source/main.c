/*------------------------------------------------------------------
 *  Fred Juhlin (2023)
 *
 *  Example of how to manage settings.  The settings are declared as JSON
 *  in html/config/settings.json.
 *  Updated settings are stored on the device localdata/settings.json
 *  
 *------------------------------------------------------------------*/
 
#include <stdio.h>
#include <stdlib.h>
#include <glib.h>
#include <glib/gi18n.h>
#include <string.h>
#include <syslog.h>

#include "APP.h"

#define LOG(fmt, args...)    { syslog(LOG_INFO, fmt, ## args); printf(fmt, ## args);}
#define LOG_WARN(fmt, args...)    { syslog(LOG_WARNING, fmt, ## args); printf(fmt, ## args);}
//#define LOG_TRACE(fmt, args...)    { syslog(LOG_INFO, fmt, ## args); printf(fmt, ## args); }
#define LOG_TRACE(fmt, args...)    {}

#define APP_PACKAGE	"config"

static GMainLoop *main_loop = NULL;

// Settings will be called when user updates settings
void
SettingsCallback( const char *service, cJSON* data) {
	char *json = cJSON_PrintUnformatted( data );
	if( json ) {
		LOG("%s:%s\n", service,json );
		free(json);
	}

	//Alternative way to get settings
	cJSON* settings = APP_Service("settings");
	
	char *someString = cJSON_GetObjectItem(settings,"someString")->valuestring;
	LOG("someString = %s\n", someString );

	int someNumber = cJSON_GetObjectItem(settings,"someNumber")->valueint;
	LOG("someNumber = %d\n", someNumber );

	char *someBool = cJSON_GetObjectItem(settings,"someBool")->type == cJSON_True ? "True":"False";
	LOG("someBool = %s\n", someBool );
	
}


int 
main(void) {
	openlog(APP_PACKAGE, LOG_PID|LOG_CONS, LOG_USER);
	
	//Initialize ACAP resources
	APP( APP_PACKAGE, SettingsCallback );

	main_loop = g_main_loop_new(NULL, FALSE);
	g_main_loop_run(main_loop);

	return 0;
}
