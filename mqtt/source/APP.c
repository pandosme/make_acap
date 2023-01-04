/*------------------------------------------------------------------
 *  Fred Juhlin (2022)
 *------------------------------------------------------------------*/

#include <stdlib.h>
#include <syslog.h>

#include "APP.h"
#include "FILE.h"
#include "HTTP.h"


#define LOG(fmt, args...)		{ syslog(LOG_INFO, fmt, ## args); printf(fmt, ## args);}
#define LOG_WARN(fmt, args...)	{ syslog(LOG_WARNING, fmt, ## args); printf(fmt, ## args);}
#define LOG_TRACE(fmt, args...)	{ syslog(LOG_INFO, fmt, ## args); printf(fmt, ## args); }
//#define LOG_TRACE(fmt, args...)	{}

cJSON* app = 0;

cJSON*
APP_Settings() {
	return app;
}

static void
APP_http(const HTTP_Response response,const HTTP_Request request) {
	cJSON* settings = cJSON_CreateObject();
	cJSON* info = cJSON_CreateObject();
	cJSON_AddStringToObject( info,"name", APP_NAME );
	cJSON_AddStringToObject( info,"package", APP_PACKAGE );
	cJSON_AddStringToObject( info,"version", APP_VERSION );
	cJSON_AddNumberToObject( info,"id", APP_ID );
	
	cJSON_AddItemToObject(settings,"info", info );
	cJSON_AddItemToObject(settings,"settings", cJSON_Duplicate(app,1));
//	cJSON_AddItemToObject(settings,"device", cJSON_Duplicate(DEVICE(),1));
//	cJSON_AddItemToObject(settings,"status", cJSON_Duplicate(STATUS(),1));
	HTTP_Respond_JSON( response, settings );
	cJSON_Delete(settings);
}

int
APP_Init() {
	LOG_TRACE("%s:()\n",__func__);
	app = FILE_Read( "html/config/settings.json" );
	
	if( !app ) {
		LOG_WARN("No default settings found\n");
		return 0;
	}

	cJSON* savedSettings = FILE_Read( "localdata/settings.json" );
	if( savedSettings ) {
		cJSON* prop = savedSettings->child;
		while(prop) {
			if( cJSON_GetObjectItem(app,prop->string ) )
				cJSON_ReplaceItemInObject(app,prop->string,cJSON_Duplicate(prop,1) );
			prop = prop->next;
		}
		cJSON_Delete(savedSettings);
	}
	HTTP_Node( "app", APP_http );	
	return 1;
}
