/*------------------------------------------------------------------
 *  Fred Juhlin (2023)
 *------------------------------------------------------------------*/

#include <glib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>
#include <dlfcn.h>
#include <dirent.h>
#include "cJSON.h"
#include "DEVICE.h"
#include "STATUS.h"
#include "scene.h"
#include "MotionDetection.h"
#include "HTTP.h"
#include "FILE.h"

#define LOG(fmt, args...)    { syslog(LOG_INFO, fmt, ## args); printf(fmt, ## args);}
#define LOG_WARN(fmt, args...)    { syslog(LOG_WARNING, fmt, ## args); printf(fmt, ## args);}
//#define LOG_TRACE(fmt, args...)    { syslog(LOG_INFO, fmt, ## args); printf(fmt, ## args); }
#define LOG_TRACE(fmt, args...)    {}

typedef struct video_scene_subscriber_st video_scene_subscriber_t;
typedef void (*on_message_arrived_callback_t)(char *scene_data, void *user_data);
typedef void (*on_disconnect_callback_t)(void *user_data);

static video_scene_subscriber_t  *(*MotionDetection_create)(int channel, int lib_version, void *user_data)	= NULL;
static int (*MotionDetection_delete)(video_scene_subscriber_t *subscriber_p) = NULL;  
static int (*MotionDetection_subscribe)(video_scene_subscriber_t *subscriber_p) = NULL;
static int (*MotionDetection_unsubscribe)(video_scene_subscriber_t *subscriber_p) = NULL;
static int (*MotionDetection_disconnect_callback)(video_scene_subscriber_t *subscriber_p, on_disconnect_callback_t callback) = NULL;
static int (*MotionDetection_message_callback)(video_scene_subscriber_t *subscriber_p, on_message_arrived_callback_t callback)	= NULL;
static int (*MotionDetection_boundingbox)(video_scene_subscriber_t *subscriber_p) = NULL;
static int (*MotionDetection_polygon)(video_scene_subscriber_t *subscriber_p) = NULL;
static int (*MotionDetection_velocity)(video_scene_subscriber_t *subscriber_p) = NULL;
static int (*MotionDetection_low_confident)(video_scene_subscriber_t *subscriber_p) = NULL;

MotionDetection_Callback MotionDetection_UserCallback = NULL;
static video_scene_subscriber_t *MotionDetection_Handler = NULL;
int MotionDetection_Rotation = 0;
cJSON *MotionDetectionSettings = 0;
int MotionDetection_x1 = 0,MotionDetection_y1 = 0, MotionDetection_x2= 1000, MotionDetection_y2 = 1000;
int IGNORE_x1 = 0,IGNORE_y1 = 0, IGNORE_x2= 0, IGNORE_y2 = 0;
int MotionDetection_width = 10,MotionDetection_height = 10;
int MotionDetection_COG = 0;
int MotionDetection_LAST_EMPTY = 0;
cJSON* MotionDetections = 0;

void
MotionDetection_Scene_Recived(char *scene_data, void *user_data) {
	const scene_object_t* object;
	unsigned long id;
	guint64 *data;
	gint16 *box;
	int mx, my, mw, mh,x,y,w,h,cx,cy;

	scene_t* const sceneData = scene_copy_from_area( scene_data );
	if( sceneData == NULL ) {
		g_free(scene_data);
		LOG_WARN("Invalid MotionDetection Scene\n");
		return;
	}

	scene_get_prop( sceneData, SCENE_PROP_TIME, (const void **)&data);
	object = scene_next_object( sceneData, NULL );

	double timestamp = DEVICE_Timestamp();

	cJSON *list = cJSON_CreateArray();
	int counter = 0;
	while( object ) {
		counter++;
		id = scene_get_object_id( object );
		scene_get_obj_prop( object, SCENE_PROP_BOX, (const void **)&box );
		mx = *(box);
		my = *(box+1);
		mw = *(box+2);
		mh = *(box+3);
		
		x =  4096 + mx; 
		y = 4096 - my - mh;
		w = mw;
		h = mh;

		if( MotionDetection_Rotation == 180 ) {
			x =  4096 - mx - mw;
			y = 4096 + my;
			w = mw;
			h = mh;
		}
        
	
		x = (x * 1000)/8192;
		y = (y * 1000)/8192;
		w = (w * 1000)/8192;
		h = (h * 1000)/8192;
		
		if( MotionDetection_Rotation == 90 ) {
			int t = h;
			h = w;
			w = t;
			
			t = x;
			x = 1000 - y - w;//4096 + my + mh;
			y = t; //4096 + mx + mw;
		}

		if( MotionDetection_Rotation == 270 ) {
			int t = h;
			h = w;
			w = t;
			
			t = y;
			y = 1000 - x - h;//4096 + my + mh;
			x = t; //4096 + mx + mw;
		}
		
		
		if( MotionDetection_COG == 0 ) {  //Bottom-center (feet)
			cx = x+(w/2);
			cy = y+h;
		}
		if( MotionDetection_COG == 1 ) {  //Middle-center
			cx = x+(w/2);
			cy = y+(h/2);
		}
		if( MotionDetection_COG == 2 ) {  //Top-Left
			cx = x;
			cy = y;
		}
		cJSON* boundingbox = cJSON_CreateObject();
		cJSON_AddNumberToObject(boundingbox,"id",id);
		cJSON_AddNumberToObject(boundingbox,"x",x);
		cJSON_AddNumberToObject(boundingbox,"y",y);
		cJSON_AddNumberToObject(boundingbox,"w",w);
		cJSON_AddNumberToObject(boundingbox,"h",h);
		cJSON_AddNumberToObject(boundingbox,"cx",cx);
		cJSON_AddNumberToObject(boundingbox,"cy",cy);

		int ignoreFlag = 0;
		if(cx < MotionDetection_x1 || cx > MotionDetection_x2 || cy < MotionDetection_y1 || cy > MotionDetection_y2 )
			ignoreFlag = 1;
	
		if( !ignoreFlag && (w < MotionDetection_width || h < MotionDetection_height ) )
			ignoreFlag = 1;

		if( !ignoreFlag && (cx > IGNORE_x1 && cx < IGNORE_x2 && cy > IGNORE_y1 && cy < IGNORE_y2) )
			ignoreFlag = 1;

		if( !ignoreFlag )
			cJSON_AddItemToArray(list,boundingbox);
		else
			cJSON_Delete(boundingbox);
		
		object = scene_next_object( sceneData, object);
	}
	scene_delete( sceneData );
	g_free(scene_data);
	if( MotionDetection_LAST_EMPTY && cJSON_GetArraySize(list) == 0 ) {
		LOG_TRACE("%s: Empty\n",__func__);
		cJSON_Delete( list );
		return;
	}
	MotionDetection_LAST_EMPTY = cJSON_GetArraySize(list) == 0;
	if( MotionDetection_UserCallback )
		MotionDetection_UserCallback( timestamp, list );

	if( !MotionDetections )
		MotionDetections = cJSON_CreateArray();
	
	cJSON* detection = cJSON_CreateObject();
	cJSON_AddNumberToObject( detection,"timestamp", timestamp);
	cJSON_AddItemToObject( detection, "list", cJSON_Duplicate(list,1));

	cJSON_AddItemToArray( MotionDetections, detection );
	//Keep the list to max 5 items
	while( cJSON_GetArraySize( MotionDetections ) > 5 )
		cJSON_DeleteItemFromArray( MotionDetections, 0 );
	
	cJSON_Delete( list );
}


void
MotionDetection_onDisconnect(void *user_data) {
	LOG_WARN("MotionDetection Disconnected");
}


void
MotionDetection_RefreshSetting() {
	LOG_TRACE("%s: Entry\n",__func__);

	cJSON* area = cJSON_GetObjectItem(MotionDetectionSettings,"area");
	if( area ) {
		MotionDetection_x1 = cJSON_GetObjectItem(area,"x1")?cJSON_GetObjectItem(area,"x1")->valueint:0;
		MotionDetection_y1 = cJSON_GetObjectItem(area,"y1")?cJSON_GetObjectItem(area,"y1")->valueint:0;
		MotionDetection_x2 = cJSON_GetObjectItem(area,"x2")?cJSON_GetObjectItem(area,"x2")->valueint:1000;
		MotionDetection_y2 = cJSON_GetObjectItem(area,"y2")?cJSON_GetObjectItem(area,"y2")->valueint:1000;
	}

	cJSON* ignore = cJSON_GetObjectItem(MotionDetectionSettings,"ignore");
	if( ignore ) {
		IGNORE_x1 = cJSON_GetObjectItem(ignore,"x1")?cJSON_GetObjectItem(ignore,"x1")->valueint:0;
		IGNORE_y1 = cJSON_GetObjectItem(ignore,"y1")?cJSON_GetObjectItem(ignore,"y1")->valueint:0;
		IGNORE_x2 = cJSON_GetObjectItem(ignore,"x2")?cJSON_GetObjectItem(ignore,"x2")->valueint:0;
		IGNORE_y2 = cJSON_GetObjectItem(ignore,"y2")?cJSON_GetObjectItem(ignore,"y2")->valueint:0;
	}
	
	MotionDetection_width = cJSON_GetObjectItem(MotionDetectionSettings,"width")?cJSON_GetObjectItem(MotionDetectionSettings,"width")->valueint:0;
	MotionDetection_height = cJSON_GetObjectItem(MotionDetectionSettings,"height")?cJSON_GetObjectItem(MotionDetectionSettings,"height")->valueint:0;

	MotionDetection_Rotation = cJSON_GetObjectItem( MotionDetectionSettings, "rotation" )?cJSON_GetObjectItem( MotionDetectionSettings, "rotation" )->valueint:DEVICE_Prop_Int("rotation");
	
	char *json = cJSON_PrintUnformatted(MotionDetectionSettings);
	if( json ) {
		LOG_TRACE("%s: %s\n",__func__,json);
		free(json);
	}

	MotionDetection_COG = 0;
	if( cJSON_GetObjectItem(MotionDetectionSettings,"centerOfGravity") && cJSON_GetObjectItem(MotionDetectionSettings,"centerOfGravity")->type == cJSON_Number )
		MotionDetection_COG = cJSON_GetObjectItem(MotionDetectionSettings,"centerOfGravity")->valueint;
	
	LOG_TRACE("%s: Exit\n",__func__);

}

int
MotionDetection_Close() {
	int result;
	if( !MotionDetection_Handler )
		return 1;
 
	result = (*MotionDetection_unsubscribe)(MotionDetection_Handler);
	if( result < 0 ) {
		LOG_WARN("Could not unsubscribe, error %d. Terminating.", result);
		return 0;
	}
	MotionDetection_Handler = 0;
	STATUS_SetString("MotionDetection","status","Disabled");
	return 1;
}

int MotionDetection_dummy_user_data = 42;  //Yes, the anser to everyting


static void
MotionDetection_HTTP(const HTTP_Response response,const HTTP_Request request) {
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
			if( cJSON_GetObjectItem(MotionDetectionSettings,prop->string ) )
				cJSON_ReplaceItemInObject(MotionDetectionSettings,prop->string,cJSON_Duplicate(prop,1) );
			prop = prop->next;
		}
		cJSON_Delete(settings);
		FILE_Write( "localdata/MotionDetection.json", MotionDetectionSettings);
		HTTP_Respond_Text( response, "OK" );
		MotionDetection_RefreshSetting();
		return;
	}
	HTTP_Respond_JSON( response, MotionDetectionSettings );
}

static void
MotionDetections_HTTP(const HTTP_Response response,const HTTP_Request request) {
	HTTP_Respond_JSON( response ,MotionDetections );
}


cJSON*
MotionDetection( MotionDetection_Callback callback ) {
	STATUS_SetString("MotionDetection","status","Initializing");
	MotionDetection_UserCallback = callback;

	MotionDetectionSettings = FILE_Read( "html/config/MotionDetection.json" );
	if( !MotionDetectionSettings ) {
		LOG_WARN("%s: Invalide default configuration\n",__func__);
		LOG_TRACE("%s: Exit Error\n",__func__);
		return cJSON_CreateNull();
	}

	cJSON* savedSettings = FILE_Read( "localdata/MotionDetection.json" );
	if( savedSettings ) {
		cJSON* prop = savedSettings->child;
		while(prop) {
			if( cJSON_GetObjectItem(MotionDetectionSettings,prop->string ) )
				cJSON_ReplaceItemInObject(MotionDetectionSettings,prop->string,cJSON_Duplicate(prop,1) );
			prop = prop->next;
		}
		cJSON_Delete(savedSettings);
	}
	cJSON* rotation = cJSON_GetObjectItem(MotionDetectionSettings,"rotation");
	if( !rotation ) {
		cJSON_AddNumberToObject(MotionDetectionSettings,"rotation",DEVICE_Prop_Int("rotation"));
	} else {
		if( rotation->type == cJSON_NULL )
			cJSON_ReplaceItemInObject(MotionDetectionSettings,"rotation",cJSON_CreateNumber(DEVICE_Prop_Int("rotation")));
	}
	MotionDetection_RefreshSetting();
	
	//  ----------- LOAD Library  -------------
	const gchar *filesearch = 0;
	const gchar *filename;
	void *handle = NULL;
	GError *error;
	GDir *dir; 

	
	dir = g_dir_open("/usr/lib/", 0, &error);
	while ((filename = g_dir_read_name(dir))) {
		if(g_strrstr(filename,"libvideo_scene_subscriber"))
			filesearch = g_strdup(filename);                       
	}  
	g_dir_close(dir);

	handle = dlopen (filesearch, RTLD_LAZY);
	dlerror();

	MotionDetection_create	= dlsym(handle, "video_scene_subscriber_create");
	MotionDetection_delete = dlsym(handle, "video_scene_subscriber_delete");  
	MotionDetection_subscribe	= dlsym(handle, "video_scene_subscriber_subscribe");
	MotionDetection_unsubscribe = dlsym(handle, "video_scene_subscriber_unsubscribe");
	MotionDetection_disconnect_callback = dlsym(handle, "video_scene_subscriber_set_on_disconnect_callback");
	MotionDetection_message_callback = dlsym(handle, "video_scene_subscriber_set_on_message_arrived_callback");
	MotionDetection_boundingbox	= dlsym(handle, "video_scene_subscriber_set_bounding_box_enabled");
	MotionDetection_polygon = dlsym(handle, "video_scene_subscriber_set_polygon_enabled");
	MotionDetection_velocity = dlsym(handle, "video_scene_subscriber_set_velocity_enabled");
	MotionDetection_low_confident = dlsym(handle, "video_scene_subscriber_set_low_confident_enabled");

	gchar *dl_error;
	if((dl_error = dlerror()) != NULL){
		LOG_WARN("Problems loading MotionDetection scene library\n");
		dlclose(handle);
		LOG_TRACE("%s: Exit Error\n",__func__);
		return cJSON_CreateNull();
	}

	// ------------ Setup MotionDetection subscriptions -------
	MotionDetection_Handler = (*MotionDetection_create)(0, 2, &MotionDetection_dummy_user_data);
	if( MotionDetection_Handler == NULL ) {
		LOG_WARN("MotionDetection not initialized");
		LOG_TRACE("%s: Exit Error\n",__func__);
		return cJSON_CreateNull();
	}

	if( (*MotionDetection_disconnect_callback)(MotionDetection_Handler, MotionDetection_onDisconnect) < 0 ) {
		LOG_WARN("Could not set MotionDetection disconnect callback");
		(*MotionDetection_delete)(MotionDetection_Handler);
		MotionDetection_Handler = 0;
		LOG_TRACE("%s: Exit Error\n",__func__);
		return cJSON_CreateNull();
	}
 
	if( (*MotionDetection_boundingbox)(MotionDetection_Handler ) < 0 ) {
		LOG_WARN("Could not enable MotionDetection bounding box");
	}


	if( (*MotionDetection_message_callback)(MotionDetection_Handler, MotionDetection_Scene_Recived) < 0 ) {
		LOG_WARN("Could not set MOTE message arrive callback");
		(*MotionDetection_delete)(MotionDetection_Handler);
		MotionDetection_Handler = 0;
		return cJSON_CreateNull();
	}
	if( (*MotionDetection_subscribe)( MotionDetection_Handler ) < 0 ) {
		LOG_WARN("Could not subscribe to MOTE\n" );
		return cJSON_CreateNull();
	}

	STATUS_SetString("MotionDetection","status","Initialized");
	HTTP_Node( "MotionDetection", MotionDetection_HTTP );
	HTTP_Node( "MotionDetections", MotionDetections_HTTP );
	return MotionDetectionSettings;
} 