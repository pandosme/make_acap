/*------------------------------------------------------------------
 *  Fred Juhlin (2023)
 *------------------------------------------------------------------*/
 
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include "HTTP.h"
#include "STATUS.h"
#include "cJSON.h"

#define LOG(fmt, args...)    { syslog(LOG_INFO, fmt, ## args); printf(fmt, ## args);}
#define LOG_WARN(fmt, args...)    { syslog(LOG_WARNING, fmt, ## args); printf(fmt, ## args);}
//#define LOG_TRACE(fmt, args...)    { syslog(LOG_INFO, fmt, ## args); printf(fmt, ## args); }
#define LOG_TRACE(fmt, args...)    {}

cJSON *STATUS_Container = 0;

static void
STATUS_HTTP_callback(const HTTP_Response response,const HTTP_Request request) {
	if( STATUS_Container == 0 ) {
		HTTP_Respond_Error( response, 500, "Status container not initialized");
		return;
	}
	HTTP_Respond_JSON( response, STATUS_Container);	
}

cJSON*
STATUS_Group( const char *group ) {
	if(!STATUS_Container)
		STATUS_Container = cJSON_CreateObject();
	cJSON* g = cJSON_GetObjectItem(STATUS_Container, group );
	return g;
}

int
STATUS_Bool( const char *group, const char *name ) {
	cJSON* g = STATUS_Group( group );
	if(!g) {
		LOG_WARN("%s: %s is undefined\n",__func__,group);
		return 0;
	}
	cJSON *object = cJSON_GetObjectItem(g, name);
	if( !object ) {
		LOG_WARN("%s: %s/%s is undefined\n",__func__,group,name);
		return 0;
	}
	if( !( object->type == cJSON_True || object->type == cJSON_False || object->type == cJSON_NULL)) {
		LOG_WARN("%s: %s/%s is not bool (Type = %d)\n",__func__,group,name,object->type); 
		return 0;
	}
	if( object->type == cJSON_True )
		return 1;
	return 0;
}

double
STATUS_Double( const char *group, const char *name ) {
	cJSON* g = STATUS_Group( group );
	if(!g) {
		LOG_WARN("%s: %s is undefined\n",__func__,group);
		return 0;
	}
	cJSON *object = cJSON_GetObjectItem(g, name);
	if( !object ) {
		LOG_WARN("%s: %s/%s is undefined\n",__func__,group,name);
		return 0;
	}
	if( object->type != cJSON_Number ) {
		LOG_WARN("%s: %s/%s is not a number\n",__func__,group,name); 
		return 0;
	}
	return object->valuedouble;
}

int
STATUS_Int( const char *group, const char *name ) {
	cJSON* g = STATUS_Group( group );
	if(!g) {
		LOG_WARN("%s: %s is undefined\n",__func__,group);
		return 0;
	}
	cJSON *object = cJSON_GetObjectItem(g, name);
	if( !object ) {
		LOG_WARN("%s: %s/%s is undefined\n",__func__,group,name);
		return 0;
	}
	if( object->type != cJSON_Number ) {
		LOG_WARN("%s: %s/%s is not a number\n",__func__,group,name); 
		return 0;
	}
	return object->valueint;
}

char*
STATUS_String( const char *group, const char *name ) {
	cJSON* g = STATUS_Group(group);
	if(!g) {
		LOG_WARN("%s: %s is undefined\n",__func__,group);
		return 0;
	}
	cJSON *object = cJSON_GetObjectItem(g, name);
	if( !object ) {
		LOG_WARN("%s: %s/%s is undefined\n",__func__,group,name);
		return 0;
	}
	if( object->type != cJSON_String ) {
		LOG_WARN("%s: %s/%s is not a string\n",__func__,group,name); 
		return 0;
	}
	return object->valuestring;
}

cJSON*
STATUS_Object( const char *group, const char *name ) {
	cJSON* g = STATUS_Group(group);
	if(!g) {
		LOG_WARN("%s: %s is undefined\n",__func__,group);
		return 0;
	}
	cJSON *object = cJSON_GetObjectItem(g, name);
	if( !object ) {
		LOG_WARN("%s: %s/%s is undefined\n",__func__,group,name);
		return 0;
	}
	return object;
}

void
STATUS_SetBool( const char *group, const char *name, int state ) {
	LOG_TRACE("%s: %s.%s=%s \n",__func__, group, name, state?"true":"false" );
	cJSON* g = STATUS_Group(group);
	if(!g) {
		g = cJSON_CreateObject();
		cJSON_AddItemToObject(STATUS_Container, group, g);
	}
	cJSON *object = cJSON_GetObjectItem( g, name);
	if( !object ) {
		if( state )
			cJSON_AddTrueToObject( g, name );
		else
			cJSON_AddFalseToObject( g, name );
		return;
	}
	if( state )
		cJSON_ReplaceItemInObject(g, name, cJSON_CreateTrue() );
	else
		cJSON_ReplaceItemInObject(g, name, cJSON_CreateFalse() );
}

void
STATUS_SetNumber( const char *group, const char *name, double value ) {
	LOG_TRACE("%s: %s.%s=%f \n",__func__, group, name, value );
	cJSON* g = STATUS_Group(group);
	if(!g) {
		g = cJSON_CreateObject();
		cJSON_AddItemToObject(STATUS_Container, group, g);
	}
	cJSON *object = cJSON_GetObjectItem( g, name);
	if( !object ) {
		cJSON_AddNumberToObject( g, name, value );
		return;
	}
	cJSON_ReplaceItemInObject(g, name, cJSON_CreateNumber( value ));
}

void
STATUS_SetString( const char *group, const char *name, const char *string ) {
	LOG_TRACE("%s: %s.%s=%s \n",__func__, group, name, string );
	cJSON* g = STATUS_Group(group);
	if(!g) {
		g = cJSON_CreateObject();
		cJSON_AddItemToObject(STATUS_Container, group, g);
	}
	cJSON *object = cJSON_GetObjectItem( g, name);
	if( !object ) {
		cJSON_AddStringToObject( g, name, string );
		return;
	}
	cJSON_ReplaceItemInObject(g, name, cJSON_CreateString( string ));
}

void
STATUS_SetObject( const char *group, const char *name, cJSON* data ) {
	LOG_TRACE("%s: %s.%s\n",__func__, group, name );
	
	cJSON* g = STATUS_Group(group);
	if(!g) {
		g = cJSON_CreateObject();
		cJSON_AddItemToObject(STATUS_Container, group, g);
	}

	cJSON *object = cJSON_GetObjectItem( g, name);
	if( !object ) {
		cJSON_AddItemToObject( g, name, cJSON_Duplicate(data,1) );
		return;
	}
	cJSON_ReplaceItemInObject(g, name, cJSON_Duplicate(data,1));
}

void
STATUS_SetNull( const char *group, const char *name ) {
	LOG_TRACE("%s: %s.%s=NULL\n",__func__, group, name );
	
	cJSON* g = STATUS_Group(group);
	if(!g) {
		g = cJSON_CreateObject();
		cJSON_AddItemToObject(STATUS_Container, group, g);
	}

	cJSON *object = cJSON_GetObjectItem( g, name);
	if( !object ) {
		cJSON_AddItemToObject( g, name, cJSON_CreateNull() );
		return;
	}
	cJSON_ReplaceItemInObject(g, name, cJSON_CreateNull());
}

cJSON*
STATUS() {
	if( STATUS_Container != 0 )
		return STATUS_Container;
	LOG_TRACE("%s:\n",__func__);
	STATUS_Container = cJSON_CreateObject();
	HTTP_Node( "status", STATUS_HTTP_callback );
	return STATUS_Container;
}

