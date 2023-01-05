#include <stdio.h>
#include <stdlib.h>
#include <glib.h>
#include <glib/gi18n.h>
#include <string.h>
#include <syslog.h>

#include "APP.h"
#include "HTTP.h"
#include "FILE.h"
#include "DEVICE.h"
#include "STATUS.h"
#include "MOTE.h"

#define LOG(fmt, args...)    { syslog(LOG_INFO, fmt, ## args); printf(fmt, ## args);}
#define LOG_WARN(fmt, args...)    { syslog(LOG_WARNING, fmt, ## args); printf(fmt, ## args);}
#define LOG_TRACE(fmt, args...)    { syslog(LOG_INFO, fmt, ## args); printf(fmt, ## args); }
//#define LOG_TRACE(fmt, args...)    {}

static GMainLoop *main_loop = NULL;

cJSON* last5 = 0;



void
MoteData( double timestamp, cJSON* list) {

	// list includes bounding boxes for detections at this timestamp.

	char* json = cJSON_PrintUnformatted( list );
	if( json ) {
		LOG_TRACE("%s\n",json);
		free(json);
	}

	//Store the last 5 detection lists with timestamp
	if( !last5 )
		last5 = cJSON_CreateArray();
	
	cJSON* detection = cJSON_CreateObject();
	cJSON_AddNumberToObject( detection,"timestamp", timestamp);
	cJSON_AddItemToObject( detection, "list", cJSON_Duplicate(list,1));

	cJSON_AddItemToArray( last5, detection );
	//Keep the list to max 5 items
	while( cJSON_GetArraySize( last5 ) > 5 )
		cJSON_DeleteItemFromArray( last5, 0 );
}

static void
MAIN_http_last5(const HTTP_Response response,const HTTP_Request request) {
	HTTP_Respond_JSON( response ,last5 );
}

int 
main(void) {
	openlog(APP_PACKAGE, LOG_PID|LOG_CONS, LOG_USER);
	
	FILE_Init( APP_PACKAGE );
	HTTP_Init( APP_PACKAGE );
	DEVICE_Init( APP_PACKAGE );
	STATUS_Init( APP_PACKAGE );
	APP_Init();

	if( MOTE_Init() )
		MOTE_Subscribe( MoteData );

	HTTP_Node( "last5", MAIN_http_last5 );

	main_loop = g_main_loop_new(NULL, FALSE);
	g_main_loop_run(main_loop);

	return 0;
}
