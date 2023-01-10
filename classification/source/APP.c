/*------------------------------------------------------------------
 *  Fred Juhlin (2022)
 *------------------------------------------------------------------*/

#include <stdlib.h>
#include <syslog.h>

#include "APP.h"
#include "FILE.h"
#include "HTTP.h"
#include "STATUS.h"
#include "DEVICE.h"
#include "CLASSIFICATION.h"


#define LOG(fmt, args...)		{ syslog(LOG_INFO, fmt, ## args); printf(fmt, ## args);}
#define LOG_WARN(fmt, args...)	{ syslog(LOG_WARNING, fmt, ## args); printf(fmt, ## args);}
//#define LOG_TRACE(fmt, args...)	{ syslog(LOG_INFO, fmt, ## args); printf(fmt, ## args); }
#define LOG_TRACE(fmt, args...)	{}

cJSON* app = 0;

cJSON*
APP_Settings() {
	return app;
}

static void
APP_http_app(const HTTP_Response response,const HTTP_Request request) {
	cJSON* settings = cJSON_CreateObject();
	cJSON_AddItemToObject(settings,"settings", cJSON_Duplicate(app,1));
	cJSON_AddItemToObject(settings,"classification", cJSON_Duplicate(CLASSIFICATION_Settings(),1));
	cJSON_AddItemToObject(settings,"device", cJSON_Duplicate(DEVICE(),1));
	cJSON_AddItemToObject(settings,"status", cJSON_Duplicate(STATUS(),1));
	HTTP_Respond_JSON( response, settings );
	cJSON_Delete(settings);
}

static void
APP_http_settings(const HTTP_Response response,const HTTP_Request request) {
	
	const char* json = HTTP_Request_Param( request, "json");
	if( !json )
		json = HTTP_Request_Param( request, "set");
	if( !json ) {
		HTTP_Respond_JSON( response, app );
		return;
	}

	cJSON *params = cJSON_Parse(json);
	if(!params) {
		HTTP_Respond_Error( response, 400, "Invalid JSON data");
		return;
	}

	cJSON* param = params->child;
	while(param) {
		if( cJSON_GetObjectItem(app,param->string ) )
			cJSON_ReplaceItemInObject(app,param->string,cJSON_Duplicate(param,1) );
		param = param->next;
	}
	cJSON_Delete(params);
	FILE_Write( "localdata/settings.json", app);	
	HTTP_Respond_Text( response, "OK" );
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
