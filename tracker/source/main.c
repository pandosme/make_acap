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
#include "TRACKER.h"
#include "MQTTD.h"

#define LOG(fmt, args...)    { syslog(LOG_INFO, fmt, ## args); printf(fmt, ## args);}
#define LOG_WARN(fmt, args...)    { syslog(LOG_WARNING, fmt, ## args); printf(fmt, ## args);}
#define LOG_TRACE(fmt, args...)    { syslog(LOG_INFO, fmt, ## args); printf(fmt, ## args); }
//#define LOG_TRACE(fmt, args...)    {}

static GMainLoop *main_loop = NULL;

cJSON* mainTrackers = 0;

void
TrackerUpdate( cJSON* tracker) {
	/* Tracker
	{
		"id": Unique object ID,
		"active": bool,	True = still tracking, False = Object tracking lost
		"timestamp": number, Last position timestamp (EPOCH milli seconds)
		"birth": number, Birth timestamp (EPOCH milli seconds)
		"x": 0-1000,
		"y": 0-1000,
		"w": 0-1000,
		"h": 0-1000,
		"cx": 0-1000,  Center of gravity.  Placement depends on configuration
		"cy": 0-1000,   Either in the center of the box or bottom-middle (default)
		"bx": 0-1000,  Birth X (cx)
		"by": 0-1000,  Birth Y (cy)
		"px": 0-1000,  Previous cx
		"py": 0-1000,  Previous cy
		"dx": 0-1000,  Total delta x movement from birth ( positive => right, negative => left
		"dy": 0-1000,  Total delta y movement from birth ( positive => down, negative => up
		"distance": number  Percent of camer view 
		"age": number seconds
	}
	*/

	cJSON* appSettings = APP_Settings();

	double minAgeMS = cJSON_GetObjectItem(appSettings,"minAgeSeconds")->valuedouble*1000;
	if( cJSON_GetObjectItem( tracker,"age" )->valuedouble < minAgeMS )
		return;

	double minDistance = cJSON_GetObjectItem(appSettings,"minDistancePercent")->valuedouble;
	if( cJSON_GetObjectItem( tracker,"distance" )->valuedouble < minDistance )
		return;

	cJSON_AddStringToObject(tracker,"device", DEVICE_Prop("serial"));

	//Publish on MQTT via SIMQTT Proxy
	const char* topic = cJSON_GetObjectItem(appSettings,"topic")->valuestring;
	int mqtt = MQTTD_Publish_JSON( topic, tracker, 0);
	switch( mqtt ) {
		case MQTTD_OK: 			STATUS_SetString( "MQTT", "status","Connected");break;
		case MQTTD_NO_SIMQTT:	STATUS_SetString( "MQTT", "status","SIMQTT is not running");break;
		case MQTTD_BROKER:		STATUS_SetString( "MQTT", "status","SIMQTT is not connected to broker" ); 	break;
		default:				STATUS_SetString( "MQTT", "status","Message publish failed" );break;
	}
	
	//Store mainTrackers for HTTP GET /mainTrackers
	if(!mainTrackers)
		mainTrackers = cJSON_CreateObject();

	char id[16];
	sprintf(id,"%u", cJSON_GetObjectItem(tracker,"id")->valueint );
	int active = cJSON_GetObjectItem(tracker,"active")->type == cJSON_True;
	if( !active ) {
		cJSON_DeleteItemFromObject( mainTrackers,id );
		return;
	}
	cJSON* trackerID = cJSON_DetachItemFromObject(mainTrackers,id);
	if( trackerID )
		cJSON_Delete( trackerID ); 
	cJSON_AddItemToObject(mainTrackers,id, cJSON_Duplicate(tracker,1));
}

static void
MAIN_http_mainTrackers(const HTTP_Response response,const HTTP_Request request) {

	if(!mainTrackers)
		mainTrackers = cJSON_CreateObject();
		
	HTTP_Respond_JSON( response , mainTrackers );
}

int 
main(void) {
	openlog(APP_PACKAGE, LOG_PID|LOG_CONS, LOG_USER);
	
	FILE_Init( APP_PACKAGE );
	HTTP_Init( APP_PACKAGE );
	DEVICE_Init( APP_PACKAGE );
	STATUS_Init( APP_PACKAGE );
	APP_Init();

	TRACKER_Init( TrackerUpdate );

	HTTP_Node( "trackers", MAIN_http_mainTrackers );

	main_loop = g_main_loop_new(NULL, FALSE);
	g_main_loop_run(main_loop);

	return 0;
}
