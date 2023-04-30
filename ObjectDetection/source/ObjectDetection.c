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
#include "ObjectDetection.h"
#include "cJSON.h"
#include "video_object_detection_subscriber.h"
#include "video_object_detection.pb-c.h"


#define LOG(fmt, args...)    	{ syslog(LOG_INFO, fmt, ## args);  printf(fmt, ## args); }
#define LOG_WARN(fmt, args...)  { syslog(LOG_WARNING, fmt, ## args); printf(fmt, ## args);}
//#define LOG_TRACE(fmt, args...)    { syslog(LOG_INFO, fmt, ## args); printf(fmt, ## args);}
#define LOG_TRACE(fmt, args...) {}

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


static video_object_detection_subscriber_t *ObjectDetection_Handler = NULL;
int ObjectDetection_Rotation = 0;
ObjectDetection_Callback ObjectDetection_UserCallback = NULL;

cJSON *ObjectDetectionSettings = NULL;

int ObjectDetection_AREA_X1 = 0,ObjectDetection_AREA_Y1 = 0, ObjectDetection_AREA_X2 = 1000, ObjectDetection_AREA_Y2 = 1000;
int ObjectDetection_IGNORE_X1 = 0, ObjectDetection_IGNORE_Y1 = 0, ObjectDetection_IGNORE_X2= 0, ObjectDetection_IGNORE_Y2 = 0;
int ObjectDetection_WIDTH = 10,ObjectDetection_HEIGHT = 10;
int ObjectDetection_MAX_WIDTH = 900,ObjectDetection_MAX_HEIGHT = 900;
int ObjectDetection_CONFIDENCE = 0;
int ObjectDetection_centerOfGravity = 0;
int ObjectDetection_LAST_EMPTY = 0;

cJSON* ObjectDetectionsDetections = 0;

cJSON*
ObjectDetection_Settings() {
	return ObjectDetectionSettings;
}


static void
ObjectDetection_Scene_Callback(const uint8_t *detection_data, size_t data_size, void *user_data) {
    unsigned int i;

	double x1,y1,x2,y2;
	if( !data_size || !detection_data ) {
		LOG_WARN("%s: Invalid input",__func__);
		return;
	}

	if( ObjectDetection_UserCallback == 0 ) {
		return;
	}

	double timestamp = DEVICE_Timestamp();
	
	VOD__Scene *recv_scene;
	recv_scene = vod__scene__unpack(NULL, data_size, detection_data);
	if (recv_scene == NULL) {
		LOG_WARN("%s: Error in unpacking detections",__func__);
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

			
		if( ObjectDetection_Rotation == 180 ) {
			x = 8192 - x - w;
			y = 8192 - y - h;
		}
		
		x = (x * 1000)/8192;
		y = (y * 1000)/8192;
		w = (w * 1000)/8192;
		h = (h * 1000)/8192;
			
		if( ObjectDetection_Rotation == 90 ) {
			int t = h;
			h = w;
			w = t;
			t = x;
			x = 1000 - y - w;
			y = t;
		}

		if( ObjectDetection_Rotation == 270 ) {
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
		double cy = y + h;
		if( ObjectDetection_centerOfGravity == 1 )
			cy = y + ( h / 2 );

		int classID = (int)recv_det->det_class;
		cJSON* classes = cJSON_GetObjectItem( ObjectDetectionSettings,"classes");
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

		if( !ignore && confidence < ObjectDetection_CONFIDENCE ) ignore = 1;
		if( !ignore && (w > ObjectDetection_MAX_WIDTH || h > ObjectDetection_MAX_HEIGHT ) ) ignore = 1;
		if( !ignore && (w < ObjectDetection_WIDTH || h < ObjectDetection_HEIGHT ) ) ignore = 1;
		if( !ignore && cx < ObjectDetection_AREA_X1 || cx > ObjectDetection_AREA_X2 || cy < ObjectDetection_AREA_Y1 || cy > ObjectDetection_AREA_Y2 ) ignore = 1;
		if( !ignore && (cx > ObjectDetection_IGNORE_X1 && cx <= ObjectDetection_IGNORE_X2 && cy >= ObjectDetection_IGNORE_Y1 && cy <= ObjectDetection_IGNORE_Y2) ) ignore = 1;
			
		if( !ignore ) {
			const char* type = cJSON_GetObjectItem(class,"type")?cJSON_GetObjectItem(class,"type")->valuestring:"Other";
			const char* name = cJSON_GetObjectItem(class,"name")?cJSON_GetObjectItem(class,"name")->valuestring:"Object";

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
	
	if( ObjectDetection_LAST_EMPTY && cJSON_GetArraySize(list) == 0 ) {
		cJSON_Delete( list );
		return;
	}
	ObjectDetection_LAST_EMPTY = (cJSON_GetArraySize(list) == 0);

	if( ObjectDetectionsDetections )
		cJSON_Delete( ObjectDetectionsDetections );
	ObjectDetectionsDetections = cJSON_Duplicate( list, 1 );
	
	if( ObjectDetection_UserCallback )
		ObjectDetection_UserCallback( timestamp, list );
	cJSON_Delete(list);
    return;
}

void
ObjectDetection_RefreshSetting() {
	LOG_TRACE("%s:\n",__func__);

	if(!ObjectDetectionSettings)
		return;

	cJSON* area = cJSON_GetObjectItem(ObjectDetectionSettings,"area");
	if( area ) {
		ObjectDetection_AREA_X1 = cJSON_GetObjectItem(area,"x1")?cJSON_GetObjectItem(area,"x1")->valueint:0;
		ObjectDetection_AREA_Y1 = cJSON_GetObjectItem(area,"y1")?cJSON_GetObjectItem(area,"y1")->valueint:0;
		ObjectDetection_AREA_X2 = cJSON_GetObjectItem(area,"x2")?cJSON_GetObjectItem(area,"x2")->valueint:1000;
		ObjectDetection_AREA_Y2 = cJSON_GetObjectItem(area,"y2")?cJSON_GetObjectItem(area,"y2")->valueint:1000;
	} else {
		LOG_WARN("%s: No area of interested detected\n",__func__);
		ObjectDetection_AREA_X1 = 0;
		ObjectDetection_AREA_Y1 = 0;
		ObjectDetection_AREA_X2 = 1000;
		ObjectDetection_AREA_Y2 = 1000;
	}

	cJSON* ignore = cJSON_GetObjectItem(ObjectDetectionSettings,"ignore");
	if( ignore ) {
		ObjectDetection_IGNORE_X1 = cJSON_GetObjectItem(ignore,"x1")?cJSON_GetObjectItem(ignore,"x1")->valueint:0;
		ObjectDetection_IGNORE_Y1 = cJSON_GetObjectItem(ignore,"y1")?cJSON_GetObjectItem(ignore,"y1")->valueint:0;
		ObjectDetection_IGNORE_X2 = cJSON_GetObjectItem(ignore,"x2")?cJSON_GetObjectItem(ignore,"x2")->valueint:0;
		ObjectDetection_IGNORE_Y2 = cJSON_GetObjectItem(ignore,"y2")?cJSON_GetObjectItem(ignore,"y2")->valueint:0;
	} else {
		LOG_WARN("%s: No ignore area detected\n",__func__);
		ObjectDetection_IGNORE_X1 = 0;
		ObjectDetection_IGNORE_Y1 = 0;
		ObjectDetection_IGNORE_X2 = 0;
		ObjectDetection_IGNORE_Y2 = 0;
	}
	
	ObjectDetection_WIDTH = cJSON_GetObjectItem(ObjectDetectionSettings,"width")?cJSON_GetObjectItem(ObjectDetectionSettings,"width")->valueint:0;
	ObjectDetection_HEIGHT = cJSON_GetObjectItem(ObjectDetectionSettings,"height")?cJSON_GetObjectItem(ObjectDetectionSettings,"height")->valueint:0;
	ObjectDetection_MAX_WIDTH = cJSON_GetObjectItem(ObjectDetectionSettings,"maxWidth")?cJSON_GetObjectItem(ObjectDetectionSettings,"maxWidth")->valueint:900;
	ObjectDetection_MAX_HEIGHT = cJSON_GetObjectItem(ObjectDetectionSettings,"maxHeight")?cJSON_GetObjectItem(ObjectDetectionSettings,"maxHeight")->valueint:900;
	
	ObjectDetection_CONFIDENCE = cJSON_GetObjectItem(ObjectDetectionSettings,"confidence")?cJSON_GetObjectItem(ObjectDetectionSettings,"confidence")->valueint:0;
	ObjectDetection_Rotation = cJSON_GetObjectItem(ObjectDetectionSettings,"rotation")?cJSON_GetObjectItem(ObjectDetectionSettings,"rotation")->valueint:0;	
	ObjectDetection_centerOfGravity = cJSON_GetObjectItem(ObjectDetectionSettings,"centerOfGravity")?cJSON_GetObjectItem(ObjectDetectionSettings,"centerOfGravity")->valueint:0;	

	char* json = cJSON_PrintUnformatted(ObjectDetectionSettings);
	if( json )  {
		LOG_TRACE("%s: %s\n",__func__,json);
		free(json);
	}

	LOG_TRACE("%s: Exit\n",__func__);
	
}

static void
ObjectDetection_HTTP_callback(const HTTP_Response response,const HTTP_Request request) {
	LOG_TRACE("%s:\n",__func__);

	cJSON* settings = 0;
	if( !ObjectDetectionSettings ) {
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
			LOG_WARN("Unable to parse json for ObjectDetection settings\n");
			return;
		}

		cJSON* setting = settings->child;
		while(setting) {
			if( cJSON_GetObjectItem(ObjectDetectionSettings,setting->string ) )
				cJSON_ReplaceItemInObject(ObjectDetectionSettings,setting->string,cJSON_Duplicate(setting,1) );
			setting = setting->next;
		}
		FILE_Write( "localdata/ObjectDetection.json", ObjectDetectionSettings );
		cJSON_Delete(settings);
		HTTP_Respond_Text( response, "OK" );
		ObjectDetection_RefreshSetting();		
		return;
	}
	HTTP_Respond_JSON( response, ObjectDetectionSettings );
	LOG_TRACE("%s: Exit\n",__func__);
}


static void
ObjectDetection_HTTP_detections(const HTTP_Response response,const HTTP_Request request) {
	HTTP_Respond_JSON( response, ObjectDetectionsDetections );
}

void *ObjectDetection_some_user_data;

cJSON*
ObjectDetection( ObjectDetection_Callback callback ) {
	char *json;
	LOG_TRACE("%s:\n",__func__);

	if( callback == 0 ) {
		ObjectDetection_UserCallback = 0;
		return ObjectDetectionSettings;
	}

	if( ObjectDetection_UserCallback || ObjectDetection_Handler ) {
		LOG_TRACE("%s: ObjectDetection Already initialized\n",__func__);
		return ObjectDetectionSettings;
	}

	ObjectDetectionsDetections = cJSON_CreateArray();
	ObjectDetection_UserCallback = callback;

	ObjectDetectionSettings = FILE_Read( "html/config/ObjectDetection.json" );
	if(!ObjectDetectionSettings) {
		LOG_WARN("%s: Missing default settings\n",__func__);
		return cJSON_CreateNull();
	}

	cJSON* savedSettings = FILE_Read( "localdata/ObjectDetection.json" );
	if( savedSettings ) {
		cJSON* setting = savedSettings->child;
		while(setting) {
			if( cJSON_GetObjectItem(ObjectDetectionSettings,setting->string ) )
				cJSON_ReplaceItemInObject(ObjectDetectionSettings,setting->string,cJSON_Duplicate(setting,1) );
			setting = setting->next;
		}
		cJSON_Delete(savedSettings);
	}

	if( cJSON_GetObjectItem(ObjectDetectionSettings,"rotation") && cJSON_GetObjectItem(ObjectDetectionSettings,"rotation")->type == cJSON_NULL )
		cJSON_ReplaceItemInObject(ObjectDetectionSettings,"rotation", cJSON_CreateNumber( DEVICE_Prop_Int("rotation") ) );


	STATUS_SetString( "ObjectDetection","status", "Not available" );

	if( video_object_detection_subscriber_create(&ObjectDetection_Handler, &ObjectDetection_some_user_data, 0) != 0 ) {
		LOG_WARN("%s: Cannot open channel to ObjectDetection\n",__func__);
		ObjectDetection_Handler = 0;
		STATUS_SetString( "ObjectDetection","status", "Classificaton is not compatible with this device/firmware" );
		return cJSON_CreateNull();;
	}

	ObjectDetection_RefreshSetting();	
	
    if (video_object_detection_subscriber_set_get_detection_callback(ObjectDetection_Handler, ObjectDetection_Scene_Callback) != 0) {
		LOG_WARN("%s: Could not set callback\n",__func__);
		return cJSON_CreateNull();
	}

    if (video_object_detection_subscriber_set_receive_empty_hits(ObjectDetection_Handler, TRUE) != 0) {
        LOG_WARN("%s: Failed to set receive empty hits\n",__func__);
    }

	int status = video_object_detection_subscriber_subscribe(ObjectDetection_Handler);
    if ( status != 0) {
        LOG_WARN("%s: Subscription failed.  Error code=%d\n",__func__,status);
		STATUS_SetString("ObjectDetection","status","Failed to initialize AI model");
		return cJSON_CreateNull();
    }

	STATUS_SetString( "ObjectDetection","status", "OK" );
	
	HTTP_Node( "ObjectDetection", ObjectDetection_HTTP_callback );
	HTTP_Node( "ObjectDetections", ObjectDetection_HTTP_detections );

	LOG_TRACE("%s: Exit\n",__func__);
	return ObjectDetectionSettings;
} 
