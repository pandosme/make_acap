/*------------------------------------------------------------------
 *  Fred Juhlin (2023)
 *------------------------------------------------------------------*/

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <syslog.h>
#include <string.h>
#include <assert.h>
#include "DEVICE.h"
#include "STATUS.h"
#include "HTTP.h"
#include "FILE.h"
#include "CLASSIFICATION.h"
#include "cJSON.h"
#include "video_object_detection_subscriber.h"
#include "video_object_detection.pb-c.h"


#define LOG(fmt, args...)    	{ syslog(LOG_INFO, fmt, ## args);  printf(fmt, ## args); }
#define LOG_WARN(fmt, args...)  { syslog(LOG_WARNING, fmt, ## args); printf(fmt, ## args);}
#define LOG_TRACE(fmt, args...)    { syslog(LOG_INFO, fmt, ## args); printf(fmt, ## args);}
//#define LOG_TRACE(fmt, args...) {}

/*
typedef struct {
    float left;
    float top;
    float right;
    float bottom;
    int32_t id;
    int32_t det_class;
    uint32_t score;
} video_object_detection_subscriber_hit_t;

typedef struct {
    uint64_t  timestamp;
    uint32_t  nbr_of_detections;
    video_object_detection_subscriber_hit_t
    detection_list[];
} video_object_detection_subscriber_detections_t;

#define DEFAULT_ID          -1
#define DEFAULT_DET_CLASS   -1
#define DEFAULT_SCORE       0
*/


static video_object_detection_subscriber_t *CLASSIFICATION_Handler = NULL;
int CLASSIFICATION_Rotation = 0;
CLASSIFICATION_ObjectCallback CLASSIFICATION_UserCallback = NULL;
cJSON *ClassSettings = NULL;


int CLASSIFICATION_AREA_X1 = 0,CLASSIFICATION_AREA_Y1 = 0, CLASSIFICATION_AREA_X2 = 1000, CLASSIFICATION_AREA_Y2 = 1000;
int CLASSIFICATION_IGNORE_X1 = 0, CLASSIFICATION_IGNORE_Y1 = 0, CLASSIFICATION_IGNORE_X2= 0, CLASSIFICATION_IGNORE_Y2 = 0;
int CLASSIFICATION_WIDTH = 10,CLASSIFICATION_HEIGHT = 10;
int CLASSIFICATION_MAX_WIDTH = 900,CLASSIFICATION_MAX_HEIGHT = 900;
int CLASSIFICATION_CONFIDENCE = 0;


cJSON*
CLASSIFICATION_Settings() {
	return ClassSettings;
}


static void
CLASSIFICATION_Scene_Callback(const uint8_t *detection_data, size_t data_size, void *user_data) {
    unsigned int i;

	double x1,y1,x2,y2;
	if( !data_size || !detection_data ) {
		LOG_WARN("CLASSIFICATION_Scene_Callback: Invalid input");
		return;
	}

	if( CLASSIFICATION_UserCallback == 0 ) {
		return;
	}

	double timestamp = DEVICE_Timestamp();
	
	VOD__Scene *recv_scene;
	recv_scene = vod__scene__unpack(NULL, data_size, detection_data);
	if (recv_scene == NULL) {
		LOG_WARN("CLASSIFICATION_Scene_Callback: Error in unpacking detections");
		return;
	}

	cJSON* list = cJSON_CreateArray();
	
    for (i = 0; i < recv_scene->n_detections; i++) {
        VOD__Detection *recv_det = recv_scene->detections[i];
		
		x1 = (recv_det->left * 4096) + 4096;
		y1 = 4096 - (recv_det->top * 4096);
		x2 = (recv_det->right * 4096) + 4096;
		y2 = 4096 - (recv_det->bottom * 4096);
		double x = x1;
		double y = y1;
		double w = x2-x1;
		double h = y2-y1;

			
		if( CLASSIFICATION_Rotation == 180 ) {
			x = 8192 - x - w;
			y = 8192 - y - h;
		}
		
		x = (x * 1000)/8192;
		y = (y * 1000)/8192;
		w = (w * 1000)/8192;
		h = (h * 1000)/8192;
			
		if( CLASSIFICATION_Rotation == 90 ) {
			int t = h;
			h = w;
			w = t;
			t = x;
			x = 1000 - y - w;
			y = t;
		}

		if( CLASSIFICATION_Rotation == 270 ) {
			int t = h;
			h = w;
			w = t;
			t = y;
			y = 1000 - x - h;
			x = t;
		}
			
		x1 = x;
		y1 = y;
		x2 = x + w;
		y2 = y + h;
		double cx = x + (w / 2);
		double cy = y + (h / 2);

		int classID = (int)recv_det->det_class;
		cJSON* classes = cJSON_GetObjectItem( ClassSettings,"classes");
		cJSON* class = 0;
		if( classID < cJSON_GetArraySize( classes ) )
			class = cJSON_GetArrayItem( classes, classID );
		else 
			class = cJSON_GetArrayItem( classes, cJSON_GetArraySize( classes ) - 1 );

		if(!class) LOG_WARN("%s: No class\n",__func__);

		int confidence = (unsigned)(recv_det->score);
		
		int ignore = 0;

		if( cJSON_GetObjectItem( class, "active" )->type != cJSON_True )
			ignore = 1;




		if( !ignore && confidence < CLASSIFICATION_CONFIDENCE ) ignore = 1;
		if( !ignore && (w > CLASSIFICATION_MAX_WIDTH || h > CLASSIFICATION_MAX_HEIGHT ) ) ignore = 1;
		if( !ignore && (w < CLASSIFICATION_WIDTH || h < CLASSIFICATION_HEIGHT ) ) ignore = 1;
		if( !ignore && cx < CLASSIFICATION_AREA_X1 || cx > CLASSIFICATION_AREA_X2 || cy < CLASSIFICATION_AREA_Y1 || cy > CLASSIFICATION_AREA_Y2 ) ignore = 1;
		if( !ignore && (cx > CLASSIFICATION_IGNORE_X1 && cx <= CLASSIFICATION_IGNORE_X2 && cy >= CLASSIFICATION_IGNORE_Y1 && cy <= CLASSIFICATION_IGNORE_Y2) ) ignore = 1;
			
		if( !ignore ) {
			const char* type = cJSON_GetObjectItem(class,"type")?cJSON_GetObjectItem(class,"type")->valuestring:"Undefined";
			const char* name = cJSON_GetObjectItem(class,"name")?cJSON_GetObjectItem(class,"name")->valuestring:"Undefined";

			cJSON* object = cJSON_CreateObject();
			cJSON_AddNumberToObject(object,"id",(int)recv_det->id);
			cJSON_AddNumberToObject(object,"x",(int)x);
			cJSON_AddNumberToObject(object,"y",(int)y);
			cJSON_AddNumberToObject(object,"w",(int)w);
			cJSON_AddNumberToObject(object,"h",(int)h);
			cJSON_AddNumberToObject(object,"cx",(int)cx);	
			cJSON_AddNumberToObject(object,"cy",(int)cy);
			cJSON_AddNumberToObject(object,"confidence",confidence);
			cJSON_AddNumberToObject(object,"classID", classID);
			cJSON_AddStringToObject(object,"class", name );
			cJSON_AddStringToObject(object,"type", type );
			cJSON_AddItemToArray(list, object);
		}
    }
    vod__scene__free_unpacked(recv_scene, NULL);
	if( CLASSIFICATION_UserCallback )
		CLASSIFICATION_UserCallback( timestamp, list );
	cJSON_Delete(list);
    return;
}

void
CLASSIFICATION_RefreshSetting() {
	LOG_TRACE("%s: Entry\n",__func__);

	if(!ClassSettings)
		return;

	cJSON* area = cJSON_GetObjectItem(ClassSettings,"area");
	if( area ) {
		CLASSIFICATION_AREA_X1 = cJSON_GetObjectItem(area,"x1")?cJSON_GetObjectItem(area,"x1")->valueint:0;
		CLASSIFICATION_AREA_Y1 = cJSON_GetObjectItem(area,"y1")?cJSON_GetObjectItem(area,"y1")->valueint:0;
		CLASSIFICATION_AREA_X2 = cJSON_GetObjectItem(area,"x2")?cJSON_GetObjectItem(area,"x2")->valueint:1000;
		CLASSIFICATION_AREA_Y2 = cJSON_GetObjectItem(area,"y2")?cJSON_GetObjectItem(area,"y2")->valueint:1000;
	} else {
		LOG_WARN("%s: No area of interested detected\n",__func__);
		CLASSIFICATION_AREA_X1 = 0;
		CLASSIFICATION_AREA_Y1 = 0;
		CLASSIFICATION_AREA_X2 = 1000;
		CLASSIFICATION_AREA_Y2 = 1000;
	}

	cJSON* ignore = cJSON_GetObjectItem(ClassSettings,"ignore");
	if( ignore ) {
		CLASSIFICATION_IGNORE_X1 = cJSON_GetObjectItem(ignore,"x1")?cJSON_GetObjectItem(ignore,"x1")->valueint:0;
		CLASSIFICATION_IGNORE_Y1 = cJSON_GetObjectItem(ignore,"y1")?cJSON_GetObjectItem(ignore,"y1")->valueint:0;
		CLASSIFICATION_IGNORE_X2 = cJSON_GetObjectItem(ignore,"x2")?cJSON_GetObjectItem(ignore,"x2")->valueint:0;
		CLASSIFICATION_IGNORE_Y2 = cJSON_GetObjectItem(ignore,"y2")?cJSON_GetObjectItem(ignore,"y2")->valueint:0;
	} else {
		LOG_WARN("%s: No ignore area detected\n",__func__);
		CLASSIFICATION_IGNORE_X1 = 0;
		CLASSIFICATION_IGNORE_Y1 = 0;
		CLASSIFICATION_IGNORE_X2 = 0;
		CLASSIFICATION_IGNORE_Y2 = 0;
	}
	
	CLASSIFICATION_WIDTH = cJSON_GetObjectItem(ClassSettings,"width")?cJSON_GetObjectItem(ClassSettings,"width")->valueint:0;
	CLASSIFICATION_HEIGHT = cJSON_GetObjectItem(ClassSettings,"height")?cJSON_GetObjectItem(ClassSettings,"height")->valueint:0;
	CLASSIFICATION_MAX_WIDTH = cJSON_GetObjectItem(ClassSettings,"maxWidth")?cJSON_GetObjectItem(ClassSettings,"maxWidth")->valueint:900;
	CLASSIFICATION_MAX_HEIGHT = cJSON_GetObjectItem(ClassSettings,"maxHeight")?cJSON_GetObjectItem(ClassSettings,"maxHeight")->valueint:900;
	
	CLASSIFICATION_CONFIDENCE = cJSON_GetObjectItem(ClassSettings,"confidence")?cJSON_GetObjectItem(ClassSettings,"confidence")->valueint:0;
	CLASSIFICATION_Rotation = cJSON_GetObjectItem(ClassSettings,"rotation")?cJSON_GetObjectItem(ClassSettings,"rotation")->valueint:0;	

	char* json = cJSON_PrintUnformatted(ClassSettings);
	if( json )  {
		LOG_TRACE("%s: %s\n",__func__,json);
		free(json);
	}

	LOG_TRACE("%s: Entry\n",__func__);
	
}

static void
CLASSIFICATION_HTTP_callback(const HTTP_Response response,const HTTP_Request request) {
	LOG_TRACE("%s: Entry\n",__func__);

	cJSON* settings = 0;
	if( !ClassSettings ) {
		HTTP_Respond_Error( response, 500, "Invalid settings" );
		LOG_WARN("%s: Invalid settings not initialized\n",__func__);
		return;
	}
	
	const char* json = HTTP_Request_Param(request, "json");
	if(!json)
		json = HTTP_Request_Param(request, "set");
	if( json ) {
		cJSON* settings = cJSON_Parse(json);
		if(!settings) {
			HTTP_Respond_Error( response, 400, "Invalid JSON" );
			LOG_WARN("Unable to parse json for CLASSIFICATION settings\n");
			return;
		}

		cJSON* setting = settings->child;
		while(setting) {
			if( cJSON_GetObjectItem(ClassSettings,setting->string ) )
				cJSON_ReplaceItemInObject(ClassSettings,setting->string,cJSON_Duplicate(setting,1) );
			setting = setting->next;
		}
		FILE_Write( "localdata/classification.json", ClassSettings );
		cJSON_Delete(settings);
		HTTP_Respond_Text( response, "OK" );
		CLASSIFICATION_RefreshSetting();		
		return;
	}
	HTTP_Respond_JSON( response, ClassSettings );
	LOG_TRACE("%s: Entry\n",__func__);
}


void *CLASSIFICATION_some_user_data;

int
CLASSIFICATION_Init( CLASSIFICATION_ObjectCallback callback ) {
	char *json;
	LOG_TRACE("%s:\n",__func__);

	if( callback == 0 ) {
		CLASSIFICATION_UserCallback = 0;
		return 1;
	}

	if( CLASSIFICATION_UserCallback || CLASSIFICATION_Handler ) {
		LOG_TRACE("%s: CLASSIFICATION Already initialized\n",__func__);
		return 1;
	}

	CLASSIFICATION_UserCallback = callback;

	ClassSettings = FILE_Read( "html/config/classification.json" );
	if(!ClassSettings) {
		LOG_WARN("%s: Missing default settings\n",__func__);
		return 0;
	}

	cJSON* savedSettings = FILE_Read( "localdata/classification.json" );
	if( savedSettings ) {
		cJSON* setting = savedSettings->child;
		while(setting) {
			if( cJSON_GetObjectItem(ClassSettings,setting->string ) )
				cJSON_ReplaceItemInObject(ClassSettings,setting->string,cJSON_Duplicate(setting,1) );
			setting = setting->next;
		}
		cJSON_Delete(savedSettings);
	}

	STATUS_SetString( "classification","status", "Not available" );

	if( video_object_detection_subscriber_create(&CLASSIFICATION_Handler, &CLASSIFICATION_some_user_data, 0) != 0 ) {
		LOG_WARN("%s: Cannot open channel to CLASSIFICATION\n",__func__);
		CLASSIFICATION_Handler = 0;
		STATUS_SetString( "classification","status", "Classificaton is not compatible with this device/firmware" );
		return 0;
	}

	CLASSIFICATION_RefreshSetting();	
	
	
    if (video_object_detection_subscriber_set_get_detection_callback(CLASSIFICATION_Handler, CLASSIFICATION_Scene_Callback) != 0) {
		LOG_WARN("%s: Could not set callback\n",__func__);
		return 0;
	}

    if (video_object_detection_subscriber_set_receive_empty_hits(CLASSIFICATION_Handler, TRUE) != 0) {
        LOG_WARN("%s: Failed to set receive empty hits\n",__func__);
    }

	int status = video_object_detection_subscriber_subscribe(CLASSIFICATION_Handler);
    if ( status != 0) {
        LOG_WARN("%s: Subscription failed.  Error code=%d\n",__func__,status);
		STATUS_SetString("classification","status","Failed to initialize AI model");
		return 0;
    }

	STATUS_SetString( "classification","status", "OK" );
	
	HTTP_Node( "classification", CLASSIFICATION_HTTP_callback );
	
	return 1;
} 
