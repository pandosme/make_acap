#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <math.h>
#include <assert.h>

#include "TRACKER.h"
#include "cJSON.h"
#include "MOTE.h"
#include "HTTP.h"
#include "FILE.h"
#include "DEVICE.h"

#define LOG(fmt, args...)    { syslog(LOG_INFO, fmt, ## args); printf(fmt, ## args);}
#define LOG_WARN(fmt, args...)    { syslog(LOG_WARNING, fmt, ## args); printf(fmt, ## args);}
//#define LOG_TRACE(fmt, args...)    { syslog(LOG_INFO, fmt, ## args);; printf(fmt, ## args); }
#define LOG_TRACE(fmt, args...)    {}

cJSON *TrackerSettings = 0;
cJSON* trackers = 0;
TRACKER_Callback Tracker_UserCallback = 0;
int updatingTrackers = 0;

cJSON*
TRACKER_Settings() {
	return TrackerSettings;
}

cJSON*
TRACKER_Create( double timestamp, cJSON* detection ) {
	cJSON *tracker = cJSON_Duplicate(detection,1);
	cJSON_AddNumberToObject(tracker,"timestamp",timestamp);

	cJSON_AddNumberToObject(tracker,"bx", cJSON_GetObjectItem( detection,"cx")->valuedouble ); //Birth location
	cJSON_AddNumberToObject(tracker,"by", cJSON_GetObjectItem( detection,"cy")->valuedouble);
	cJSON_AddNumberToObject(tracker,"px", cJSON_GetObjectItem( detection,"cx")->valuedouble ); // Previous location
	cJSON_AddNumberToObject(tracker,"py", cJSON_GetObjectItem( detection,"cy")->valuedouble); //
	cJSON_AddNumberToObject(tracker,"dx", 0);
	cJSON_AddNumberToObject(tracker,"dy", 0);
	cJSON_AddNumberToObject(tracker,"birth",timestamp);
	cJSON_AddNumberToObject(tracker,"age", 0);
	cJSON_AddNumberToObject(tracker,"distance",0);
	cJSON_AddTrueToObject(tracker,"active");
	return tracker;
}

void 
TRACKER_Update( double timestamp, cJSON* tracker, cJSON* detection ) {
	if(!tracker || !detection ) {
		LOG_WARN("%s: Invalid input\n",__func__);
		return;
	}
	cJSON_ReplaceItemInObject(tracker,"timestamp",cJSON_CreateNumber(timestamp));

	double x = cJSON_GetObjectItem(detection,"x")->valuedouble;
	double y = cJSON_GetObjectItem(detection,"y")->valuedouble;
	double w = cJSON_GetObjectItem(detection,"w")->valuedouble;
	double h = cJSON_GetObjectItem(detection,"h")->valuedouble;
	double cx = cJSON_GetObjectItem(detection,"cx")->valuedouble;
	double cy = cJSON_GetObjectItem(detection,"cy")->valuedouble;

	cJSON_ReplaceItemInObject(tracker,"x",cJSON_CreateNumber(x));
	cJSON_ReplaceItemInObject(tracker,"y",cJSON_CreateNumber(y));
	cJSON_ReplaceItemInObject(tracker,"w",cJSON_CreateNumber(w));
	cJSON_ReplaceItemInObject(tracker,"h",cJSON_CreateNumber(h));
	cJSON_ReplaceItemInObject(tracker,"cx",cJSON_CreateNumber(cx));
	cJSON_ReplaceItemInObject(tracker,"cy",cJSON_CreateNumber(cy));

	double minimumDelta = cJSON_GetObjectItem(TrackerSettings,"delta")?cJSON_GetObjectItem(TrackerSettings,"delta")->valuedouble:50.0;
	
	double deltaX = cx - cJSON_GetObjectItem(tracker,"px")->valuedouble;
	double deltaY = cy - cJSON_GetObjectItem(tracker,"py")->valuedouble;
	double deltaDistance = sqrt( (deltaX * deltaX) + (deltaY*deltaY) );
	if( deltaDistance < minimumDelta )
		return;
	cJSON_ReplaceItemInObject(tracker,"px",cJSON_CreateNumber(cx));
	cJSON_ReplaceItemInObject(tracker,"py",cJSON_CreateNumber(cy));

	double bx = cJSON_GetObjectItem(tracker,"bx")->valuedouble;
	double by = cJSON_GetObjectItem(tracker,"by")->valuedouble;
	cJSON_ReplaceItemInObject(tracker,"dx", cJSON_CreateNumber(cx - bx) );
	cJSON_ReplaceItemInObject(tracker,"dy", cJSON_CreateNumber(cy - by) );

	double distance = cJSON_GetObjectItem(tracker,"distance")->valuedouble;
	distance += deltaDistance / 10.0;
	cJSON_ReplaceItemInObject(tracker,"distance",cJSON_CreateNumber(distance));

	double age = timestamp - cJSON_GetObjectItem(tracker,"birth")->valuedouble;
	cJSON_ReplaceItemInObject(tracker,"age", cJSON_CreateNumber(age/1000));

	Tracker_UserCallback( tracker );
}

void
TRACKER_Kill( cJSON* list ) {
	if(!list || cJSON_GetArraySize(list) == 0 )
		return;
	cJSON* tracker = list->child;
	while(tracker) {
		if( cJSON_GetObjectItem(tracker,"age")->valueint > 0 && cJSON_GetObjectItem(tracker,"distance")->valueint > 0 ) {
			cJSON_ReplaceItemInObject(tracker,"active",cJSON_CreateFalse());
			Tracker_UserCallback( tracker );
		}
		tracker = tracker->next;
	}
	cJSON_Delete(list);
}

void
TRACKER_MOTE_Callback( double timestamp, cJSON* list) {
	char trackerID[16];
	
	if(!trackers)
		trackers = cJSON_CreateObject();
		
	if( !list || list->type != cJSON_Array )
		return;

	cJSON* activeTrackers = cJSON_CreateObject();
	cJSON* detection = list->child;
	while( detection ) {
		sprintf(trackerID,"%lu",(unsigned long)cJSON_GetObjectItem( detection,"id")->valuedouble);
		cJSON *active = cJSON_DetachItemFromObject(trackers,trackerID);
		if( active ) {
			cJSON_AddItemToObject(activeTrackers,trackerID,active);
		} else {
			cJSON_AddItemToObject(activeTrackers,trackerID, TRACKER_Create(timestamp, detection) );
		}
		detection = detection->next;
	}
	
	TRACKER_Kill( trackers );
	trackers = activeTrackers;
	
	detection = list->child;
	while( detection ) {
		sprintf(trackerID,"%lu",(unsigned long)cJSON_GetObjectItem( detection,"id")->valuedouble);
		cJSON* tracker = cJSON_GetObjectItem(trackers,trackerID);
		if( tracker ) {
			TRACKER_Update( timestamp, tracker, detection );
		} else {
			LOG_WARN("%s: Invalid tracker ID detected\n",__func__);
		}
		detection = detection->next;
	}
}

static void
TRACKER_HTTP(const HTTP_Response response,const HTTP_Request request) {

	const char* json = HTTP_Request_Param(request, "json");
	if(!json)
		json = HTTP_Request_Param(request, "set");
	if( json ) {
		cJSON *settings = cJSON_Parse(json);
		if( !settings ) {
			HTTP_Respond_Error( response, 400, "Unable to parse input" );
			LOG_WARN("%s: Unable to parse input",__func__);
			return;
		}
		cJSON* prop = settings->child;
		while(prop) {
			if( cJSON_GetObjectItem(TrackerSettings,prop->string ) )
				cJSON_ReplaceItemInObject(TrackerSettings,prop->string,cJSON_Duplicate(prop,1) );
			prop = prop->next;
		}
		cJSON_Delete(settings);
		FILE_Write( "localdata/tracker.json", TrackerSettings);
		HTTP_Respond_Text( response, "OK" );
		return;
	}
	
	HTTP_Respond_JSON( response, TrackerSettings );
}

int 
TRACKER_Init( TRACKER_Callback callback ) {
	LOG_TRACE("%s:\n",__func__);

	if( Tracker_UserCallback )
		return 1;

	Tracker_UserCallback = callback;

	TrackerSettings = FILE_Read( "html/config/tracker.json" );
	if( !TrackerSettings ) {
		LOG_WARN("%s: Invalid default configuration\n",__func__);
		return 0;
	}

	cJSON* savedSettings = FILE_Read( "localdata/tracker.json" );
	if( savedSettings ) {
		cJSON* prop = savedSettings->child;
		while(prop) {
			if( cJSON_GetObjectItem(TrackerSettings,prop->string ) )
				cJSON_ReplaceItemInObject(TrackerSettings,prop->string,cJSON_Duplicate(prop,1) );
			prop = prop->next;
		}
		cJSON_Delete(savedSettings);
	}

	HTTP_Node( "tracker", TRACKER_HTTP );

	if( !MOTE_Init() )
		return 0;
	
	if( !MOTE_Subscribe( TRACKER_MOTE_Callback ) )
		return 0;

	return 1;
} 