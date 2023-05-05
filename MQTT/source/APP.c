/*------------------------------------------------------------------
 *  Fred Juhlin (2023)
 *------------------------------------------------------------------*/

#include <stdlib.h>
#include <syslog.h>

#include "cJSON.h"
#include "APP.h"
#include "FILE.h"
#include "HTTP.h"
#include "DEVICE.h"
#include "STATUS.h"

#define LOG(fmt, args...)    { syslog(LOG_INFO, fmt, ## args); printf(fmt, ## args);}
#define LOG_WARN(fmt, args...)    { syslog(LOG_WARNING, fmt, ## args); printf(fmt, ## args);}
//#define LOG_TRACE(fmt, args...)    { syslog(LOG_INFO, fmt, ## args); printf(fmt, ## args); }
#define LOG_TRACE(fmt, args...)    {}

cJSON* app = 0;

APP_Callback	APP_UpdateCallback = 0;

const char* APP_Package() {
	cJSON* manifest = cJSON_GetObjectItem(app,"manifest");
	if(!manifest) {
		LOG_WARN("%s: invalid manifest\n",__func__);
		return "Undefined";
	}
	cJSON* acapPackageConf = cJSON_GetObjectItem(manifest,"acapPackageConf");
	if(!acapPackageConf) {
		LOG_WARN("%s: invalid acapPackageConf\n",__func__);
		return "Undefined";
	}
	cJSON* setup = cJSON_GetObjectItem(acapPackageConf,"setup");
	if(!setup) {
		LOG_WARN("%s: invalid setup\n",__func__);
		return "Undefined";
	}
	return cJSON_GetObjectItem(setup,"appName")->valuestring;
}
	
const char* APP_Name() {
	cJSON* manifest = cJSON_GetObjectItem(app,"manifest");
	if(!manifest) {
		LOG_WARN("%s: invalid manifest\n",__func__);
		return "Undefined";
	}
	cJSON* acapPackageConf = cJSON_GetObjectItem(manifest,"acapPackageConf");
	if(!acapPackageConf) {
		LOG_WARN("%s: invalid acapPackageConf\n",__func__);
		return "Undefined";
	}
	cJSON* setup = cJSON_GetObjectItem(acapPackageConf,"setup");
	if(!setup) {
		LOG_WARN("%s: invalid setup\n",__func__);
		return "Undefined";
	}
	return cJSON_GetObjectItem(setup,"friendlyName")->valuestring;
}

cJSON*
APP_Service(const char* service) {
	cJSON* reqestedService = cJSON_GetObjectItem(app, service );
	if( !reqestedService ) {
		LOG_WARN("%s: %s is undefined\n",__func__,service);
		return 0;
	}
	return reqestedService;
}

int
APP_Register(const char* service, cJSON* serviceSettings ) {
	LOG_TRACE("%s: %s\n",__func__,service);
	if( cJSON_GetObjectItem(app,service) ) {
		LOG_TRACE("%s: %s already registered\n",__func__, service);
		return 1;
	}
	cJSON_AddItemToObject( app, service, serviceSettings );
	return 1;
}

static void
APP_http_app(const HTTP_Response response,const HTTP_Request request) {
	HTTP_Respond_JSON( response, app );
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
	LOG_TRACE("%s: %s\n",__func__,json);
	cJSON* settings = cJSON_GetObjectItem(app,"settings");
	cJSON* param = params->child;
	while(param) {
		if( cJSON_GetObjectItem(settings,param->string ) )
			cJSON_ReplaceItemInObject(settings,param->string,cJSON_Duplicate(param,1) );
		param = param->next;
	}
	cJSON_Delete(params);
	FILE_Write( "localdata/settings.json", settings);	
	HTTP_Respond_Text( response, "OK" );
	if( APP_UpdateCallback )
		APP_UpdateCallback("settings",settings);
}

cJSON*
APP( const char *package, APP_Callback callback ) {
	LOG_TRACE("%s:\n",__func__);

	FILE_Init( package );
	HTTP( package );
	cJSON* device = DEVICE( package );

	APP_UpdateCallback = callback;
	
	app = cJSON_CreateObject();

	cJSON_AddItemToObject(app, "manifest", FILE_Read( "manifest.json" ));


	cJSON* settings = FILE_Read( "html/config/settings.json" );
	if(!settings)
		settings = cJSON_CreateObject();

	cJSON* savedSettings = FILE_Read( "localdata/settings.json" );
	if( savedSettings ) {
		cJSON* prop = savedSettings->child;
		while(prop) {
			if( cJSON_GetObjectItem(settings,prop->string ) )
				cJSON_ReplaceItemInObject(settings,prop->string,cJSON_Duplicate(prop,1) );
			prop = prop->next;
		}
		cJSON_Delete(savedSettings);
	}

	if( !cJSON_GetObjectItem( settings,"deviceName") )
		cJSON_AddStringToObject( settings,"deviceName", DEVICE_Prop("serial") );
	if( cJSON_GetObjectItem( settings,"deviceName")->type != cJSON_String )
		cJSON_ReplaceItemInObject(settings,"deviceName", cJSON_CreateString( DEVICE_Prop("serial") ) );
	
	if( !cJSON_GetObjectItem( settings,"deviceLocation") )
		cJSON_AddStringToObject( settings,"deviceLocation", DEVICE_Prop("IPv4") );
	if( cJSON_GetObjectItem( settings,"deviceLocation")->type != cJSON_String )
		cJSON_ReplaceItemInObject(settings,"deviceLocation", cJSON_CreateString( DEVICE_Prop("IPv4") ) );

	
	cJSON_AddItemToObject(app, "settings",settings);


	APP_Register("status",STATUS());
	APP_Register("device", device);

	
	HTTP_Node( "app", APP_http_app );	
	HTTP_Node( "settings", APP_http_settings );

	if( APP_UpdateCallback )
		APP_UpdateCallback("settings",settings);

	LOG_TRACE("%s:Exit\n",__func__);
	
	return settings;
}
