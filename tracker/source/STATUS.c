/*------------------------------------------------------------------
 *  Copyright Fred Juhlin (2021)
 *  License:
 *  All rights reserved.  This code may not be used in commercial
 *  products without permission from Fred Juhlin.
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

int
STATUS_Init() {
	if( STATUS_Container != 0 )
		return 1;
	STATUS_Container = cJSON_CreateObject();
	HTTP_Node( "status", STATUS_HTTP_callback );
	return 1;
}

cJSON*
STATUS() {
	return STATUS_Container;
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
	if(!g)
		return 0;
	cJSON *object = cJSON_GetObjectItem(g, name);
	if( !object )
		return 0;
	if( object->type == cJSON_True )
		return 1;
	return 0;
}

double
STATUS_Value( const char *group, const char *name ) {
	cJSON* g = STATUS_Group( group );
	if(!g)
		return 0;
	cJSON *object = cJSON_GetObjectItem(g, name);
	if( !object )
		return 0;
	if( object->type != cJSON_Number )
		return 0;
	return object->valuedouble;
}

int
STATUS_Int( const char *group, const char *name ) {
	cJSON* g = STATUS_Group( group );
	if(!g)
		return 0;
	cJSON *object = cJSON_GetObjectItem(g, name);
	if( !object )
		return 0;
	if( object->type != cJSON_Number )
		return 0;
	return object->valueint;
}

char*
STATUS_String( const char *group, const char *name ) {
	cJSON* g = STATUS_Group(group);
	if(!g)
		return 0;
	cJSON *object = cJSON_GetObjectItem(g, name);
	if( !object )
		return 0;
	if( object->type != cJSON_String )
		return 0;
	return object->valuestring;
}

void
STATUS_SetBool( const char *group, const char *name, int state ) {
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
	if(!string) {
		LOG_WARN("%s: Trying to set %s:%s to NULL\n",__func__,group,name);
		return;
	}
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
