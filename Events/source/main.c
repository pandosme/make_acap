/*------------------------------------------------------------------
 *  Fred Juhlin (2023)
 *
 *  Example of how to fire three different types of events.
 *  Events are declared in in html/config/events.json.
 *
 *  An HTTP GET initites the event fire  
 *  /local/package/events/fire?event=...
 *  Look in html/index.html how the web page fires events
 *------------------------------------------------------------------*/
 
#include <stdio.h>
#include <stdlib.h>
#include <glib.h>
#include <glib/gi18n.h>
#include <string.h>
#include <syslog.h>

#include "APP.h"
#include "HTTP.h"
#include "EVENTS.h"

#define LOG(fmt, args...)    { syslog(LOG_INFO, fmt, ## args); printf(fmt, ## args);}
#define LOG_WARN(fmt, args...)    { syslog(LOG_WARNING, fmt, ## args); printf(fmt, ## args);}
//#define LOG_TRACE(fmt, args...)    { syslog(LOG_INFO, fmt, ## args); printf(fmt, ## args); }
#define LOG_TRACE(fmt, args...)    {}

static GMainLoop *main_loop = NULL;

#define APP_PACKAGE	"events"

static void
MAIN_http_fire(const HTTP_Response response,const HTTP_Request request) {
	const char* event = HTTP_Request_Param( request, "event");
	if(!event){
		LOG_WARN("Invalid event");
		HTTP_Respond_Error( response, 400, "Invalid event" );
		return;
	}

	LOG_TRACE("%s: %s\n",__func__,event);

	if(strcmp(event,"anEvent") == 0) {
		if( EVENTS_Fire( event ) ) {
			HTTP_Respond_Text( response, "Event Fired" );
		} else {
			HTTP_Respond_Error( response, 500, "Event fire failed" );
		}
		return;
	}

	if(strcmp(event,"aState") == 0 ) {
		const char* state = HTTP_Request_Param( request, "state");
		int value = 1;
		if(!state)
			state = "true";
		if( strcmp(state,"false") == 0 )
			value = 0;
		if( EVENTS_Fire_State( event, value ) ) {
			HTTP_Respond_Text( response, "Event fired" );
		} else {
			HTTP_Respond_Error( response, 500, "Event fire failed" );
		}
		return;
	}

	if(strcmp(event,"someData") == 0 ) {
		cJSON* data = cJSON_CreateObject();

		const char* name = HTTP_Request_Param( request, "name");
		if(name) {
			cJSON_AddStringToObject(data,"name",name);
		} else {
			cJSON_AddStringToObject(data,"undefined",name);
		}
			
		const char* value = HTTP_Request_Param( request, "value");
		if( value ) {
			cJSON_AddNumberToObject( data,"value", atoi( value ) );
		} else {
			cJSON_AddNumberToObject( data,"value", 0 );
		}
			
		if( EVENTS_Fire_JSON( event, data ) ) {
			HTTP_Respond_Text( response, "Event fired" );
		} else {
			HTTP_Respond_Error( response, 500, "Event fire failed" );
		}
		cJSON_Delete(data);
		return;
	}
		
	HTTP_Respond_Error( response, 400, "Invalid event" );
}

int 
main(void) {
	openlog(APP_PACKAGE, LOG_PID|LOG_CONS, LOG_USER);
	
	APP( APP_PACKAGE, NULL );  //This ACAP has no settings
	APP_Register("events",EVENTS());

	HTTP_Node( "fire", MAIN_http_fire );

	main_loop = g_main_loop_new(NULL, FALSE);
	g_main_loop_run(main_loop);

	return 0;
}
