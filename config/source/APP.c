/*------------------------------------------------------------------
 *  Fred Juhlin (2022)
 *------------------------------------------------------------------*/

#include <stdlib.h>
#include <syslog.h>

#include "APP.h"
#include "FILE.h"
#include "HTTP.h"


#define LOG(fmt, args...)    { syslog(LOG_INFO, fmt, ## args); printf(fmt, ## args);}
#define LOG_WARN(fmt, args...)    { syslog(LOG_WARNING, fmt, ## args); printf(fmt, ## args);}
#define LOG_TRACE(fmt, args...)    { syslog(LOG_INFO, fmt, ## args); printf(fmt, ## args); }

cJSON* app = 0;

cJSON*
APP_Settings() {
	return app;
}

static void
APP_http_app(const HTTP_Response response,const HTTP_Request request) {
	cJSON* settings = cJSON_CreateObject();
	cJSON* info = cJSON_CreateObject();
	cJSON_AddStringToObject( info,"name", APP_NAME );
	cJSON_AddStringToObject( info,"package", APP_PACKAGE );
	cJSON_AddStringToObject( info,"version", APP_VERSION );
	cJSON_AddNumberToObject( info,"id", APP_ID );
	
	cJSON_AddItemToObject(settings,"info", info );
	cJSON_AddItemToObject(settings,"settings", cJSON_Duplicate(app,1));
	HTTP_Respond_JSON( response, settings );
	cJSON_Delete(settings);
}

static void
APP_http_settings(const HTTP_Response response,const HTTP_Request request) {
	const char *json = HTTP_Request_Param( request, "json" );
	if(!json) {
		HTTP_Respond_JSON(response, app);
		return;
	}
	cJSON *newSettings = cJSON_Parse(json);
	if(!newSettings) {
		HTTP_Respond_Error( response, 400, "Invalid JSON data" );
		return;
	}

	cJSON* prop = newSettings->child;
	while(prop) {
		if( cJSON_GetObjectItem(app,prop->string ) )
			cJSON_ReplaceItemInObject(app,prop->string,cJSON_Duplicate(prop,1) );
		prop = prop->next;
	}
	cJSON_Delete(newSettings);
	FILE_Write( "localdata/settings.json", app );
	HTTP_Respond_Text( response, "Settings updated" );
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
	HTTP_Node( "app", APP_http_app );
	HTTP_Node( "settings", APP_http_settings );
	return 1;
}
