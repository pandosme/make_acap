/*------------------------------------------------------------------
 *  Copyright Fred Juhlin (2021)
 *  License:
 *  All rights reserved.  This code may not be used in commercial
 *  products without permission from Fred Juhlin.
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
#include "MOTE.h"
#include "HTTP.h"
#include "FILE.h"

#define LOG(fmt, args...)    { syslog(LOG_INFO, fmt, ## args); printf(fmt, ## args);}
#define LOG_WARN(fmt, args...)    { syslog(LOG_WARNING, fmt, ## args); printf(fmt, ## args);}
//#define LOG_TRACE(fmt, args...)    { syslog(LOG_INFO, fmt, ## args); printf(fmt, ## args); }
#define LOG_TRACE(fmt, args...)    {}

typedef struct video_scene_subscriber_st video_scene_subscriber_t;
typedef void (*on_message_arrived_callback_t)(char *scene_data, void *user_data);
typedef void (*on_disconnect_callback_t)(void *user_data);

static video_scene_subscriber_t  *(*mote_create)(int channel, int lib_version, void *user_data)	= NULL;
static int (*mote_delete)(video_scene_subscriber_t *subscriber_p) = NULL;  
static int (*mote_subscribe)(video_scene_subscriber_t *subscriber_p) = NULL;
static int (*mote_unsubscribe)(video_scene_subscriber_t *subscriber_p) = NULL;
static int (*mote_disconnect_callback)(video_scene_subscriber_t *subscriber_p, on_disconnect_callback_t callback) = NULL;
static int (*mote_message_callback)(video_scene_subscriber_t *subscriber_p, on_message_arrived_callback_t callback)	= NULL;
static int (*mote_boundingbox)(video_scene_subscriber_t *subscriber_p) = NULL;
static int (*mote_polygon)(video_scene_subscriber_t *subscriber_p) = NULL;
static int (*mote_velocity)(video_scene_subscriber_t *subscriber_p) = NULL;
static int (*mote_low_confident)(video_scene_subscriber_t *subscriber_p) = NULL;

MOTE_Callback MOTE_UserCallback = NULL;
static video_scene_subscriber_t *MOTE_Handler = NULL;
int MOTE_Rotation = 0;
cJSON *MoteSettings = 0;
int MOTE_x1 = 0,MOTE_y1 = 0, MOTE_x2= 1000, MOTE_y2 = 1000;
int IGNORE_x1 = 0,IGNORE_y1 = 0, IGNORE_x2= 0, IGNORE_y2 = 0;
int MOTE_width = 10,MOTE_height = 10;
int MOTE_COG = 0;
int MOTE_LAST_EMPTY = 0;

cJSON*
MOTE_Settings() {
	return MoteSettings;
}

void
MOTE_Scene_Recived(char *scene_data, void *user_data) {
	const scene_object_t* object;
	unsigned long id;
	guint64 *data;
	gint16 *box;
	int mx, my, mw, mh,x,y,w,h,cx,cy;

	scene_t* const sceneData = scene_copy_from_area( scene_data );
	if( sceneData == NULL ) {
		g_free(scene_data);
		LOG_WARN("Invalid MOTE Scene\n");
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

		if( MOTE_Rotation == 180 ) {
			x =  4096 - mx - mw;
			y = 4096 + my;
			w = mw;
			h = mh;
		}
        
	
		x = (x * 1000)/8192;
		y = (y * 1000)/8192;
		w = (w * 1000)/8192;
		h = (h * 1000)/8192;
		
		if( MOTE_Rotation == 90 ) {
			int t = h;
			h = w;
			w = t;
			
			t = x;
			x = 1000 - y - w;//4096 + my + mh;
			y = t; //4096 + mx + mw;
		}

		if( MOTE_Rotation == 270 ) {
			int t = h;
			h = w;
			w = t;
			
			t = y;
			y = 1000 - x - h;//4096 + my + mh;
			x = t; //4096 + mx + mw;
		}
		
		
		if( MOTE_COG == 0 ) {  //Bottom-center (feet)
			cx = x+(w/2);
			cy = y+h;
		}
		if( MOTE_COG == 1 ) {  //Middle-center
			cx = x+(w/2);
			cy = y+(h/2);
		}
		if( MOTE_COG == 2 ) {  //Top-Left
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
		if(cx < MOTE_x1 || cx > MOTE_x2 || cy < MOTE_y1 || cy > MOTE_y2 )
			ignoreFlag = 1;
	
//		if( !ignoreFlag && (w < MOTE_width || h < MOTE_height ) )
//			ignoreFlag = 1;

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
	if( MOTE_LAST_EMPTY && cJSON_GetArraySize(list) == 0 ) {
		LOG_TRACE("%s: Empty\n",__func__);
		cJSON_Delete( list );
		return;
	}
	MOTE_LAST_EMPTY = cJSON_GetArraySize(list) == 0;
	MOTE_UserCallback( timestamp, list );
	cJSON_Delete( list );
}

int
MOTE_Subscribe( MOTE_Callback callback ) {
	LOG_TRACE("%s:\n",__func__);
	if( callback == 0 ) {
		MOTE_UserCallback = 0;
		return 1;
	}
	
	if( !MOTE_Handler ) {
		LOG_WARN("%s: MOTE_Handler is not initialized\n",__func__);
		return 0;
	}

	if( MOTE_UserCallback )
		return 1;

	STATUS_SetString("app","mote","Disabled");
	
	if( (*mote_message_callback)(MOTE_Handler, MOTE_Scene_Recived) < 0 ) {
		LOG_WARN("Could not set MOTE message arrive callback");
		(*mote_delete)(MOTE_Handler);
		MOTE_Handler = 0;
		return 0;
	}
	if( (*mote_subscribe)( MOTE_Handler ) < 0 ) {
		LOG_WARN("Could not subscribe to MOTE\n" );
		return 0;
	}

	MOTE_UserCallback = callback;
	
	STATUS_SetString("app","mote","OK");
	return 1;
}

void
MOTE_onDisconnect(void *user_data) {
	LOG_WARN("MOTE Disconnected");
}


void
MOTE_RefreshSetting() {
	LOG_TRACE("%s: Entry\n",__func__);

	cJSON* area = cJSON_GetObjectItem(MoteSettings,"area");
	if( area ) {
		MOTE_x1 = cJSON_GetObjectItem(area,"x1")?cJSON_GetObjectItem(area,"x1")->valueint:0;
		MOTE_y1 = cJSON_GetObjectItem(area,"y1")?cJSON_GetObjectItem(area,"y1")->valueint:0;
		MOTE_x2 = cJSON_GetObjectItem(area,"x2")?cJSON_GetObjectItem(area,"x2")->valueint:1000;
		MOTE_y2 = cJSON_GetObjectItem(area,"y2")?cJSON_GetObjectItem(area,"y2")->valueint:1000;
	}

	cJSON* ignore = cJSON_GetObjectItem(MoteSettings,"ignore");
	if( ignore ) {
		IGNORE_x1 = cJSON_GetObjectItem(ignore,"x1")?cJSON_GetObjectItem(ignore,"x1")->valueint:0;
		IGNORE_y1 = cJSON_GetObjectItem(ignore,"y1")?cJSON_GetObjectItem(ignore,"y1")->valueint:0;
		IGNORE_x2 = cJSON_GetObjectItem(ignore,"x2")?cJSON_GetObjectItem(ignore,"x2")->valueint:0;
		IGNORE_y2 = cJSON_GetObjectItem(ignore,"y2")?cJSON_GetObjectItem(ignore,"y2")->valueint:0;
	}
	
	MOTE_width = cJSON_GetObjectItem(MoteSettings,"width")?cJSON_GetObjectItem(MoteSettings,"width")->valueint:0;
	MOTE_height = cJSON_GetObjectItem(MoteSettings,"height")?cJSON_GetObjectItem(MoteSettings,"height")->valueint:0;

	MOTE_Rotation = cJSON_GetObjectItem( MoteSettings, "rotation" )?cJSON_GetObjectItem( MoteSettings, "rotation" )->valueint:DEVICE_Prop_Int("rotation");
	
	char *json = cJSON_PrintUnformatted(MoteSettings);
	if( json ) {
		LOG_TRACE("%s: %s\n",__func__,json);
		free(json);
	}

	MOTE_COG = 0;
	if( cJSON_GetObjectItem(MoteSettings,"centerOfGravity") && cJSON_GetObjectItem(MoteSettings,"centerOfGravity")->type == cJSON_Number )
		MOTE_COG = cJSON_GetObjectItem(MoteSettings,"centerOfGravity")->valueint;
	
	LOG_TRACE("%s: Exit\n",__func__);

}

int
MOTE_Close() {
	int result;
	if( !MOTE_Handler )
		return 1;
 
	result = (*mote_unsubscribe)(MOTE_Handler);
	if( result < 0 ) {
		LOG_WARN("Could not unsubscribe, error %d. Terminating.", result);
		return 0;
	}
	MOTE_Handler = 0;
	STATUS_SetString("app","mote","Disabled");
	return 1;
}

int MOTE_dummy_user_data = 42;  //Yes, the anser to everyting


static void
MOTE_HTTP(const HTTP_Response response,const HTTP_Request request) {
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
			if( cJSON_GetObjectItem(MoteSettings,prop->string ) )
				cJSON_ReplaceItemInObject(MoteSettings,prop->string,cJSON_Duplicate(prop,1) );
			prop = prop->next;
		}
		cJSON_Delete(settings);
		FILE_Write( "localdata/mote.json", MoteSettings);
		HTTP_Respond_Text( response, "OK" );
		MOTE_RefreshSetting();
		return;
	}
	HTTP_Respond_JSON( response, MoteSettings );
}

int 
MOTE_Init() {
	STATUS_SetString("app","mote","Initializing");

	MOTE_UserCallback = 0;

	MoteSettings = FILE_Read( "html/config/mote.json" );
	if( !MoteSettings ) {
		LOG_WARN("%s: Invalide default configuration\n",__func__);
		LOG_TRACE("%s: Exit Error\n",__func__);
		return 0;
	}

	cJSON* savedSettings = FILE_Read( "localdata/mote.json" );
	if( savedSettings ) {
		cJSON* prop = savedSettings->child;
		while(prop) {
			if( cJSON_GetObjectItem(MoteSettings,prop->string ) )
				cJSON_ReplaceItemInObject(MoteSettings,prop->string,cJSON_Duplicate(prop,1) );
			prop = prop->next;
		}
		cJSON_Delete(savedSettings);
	}
	cJSON* rotation = cJSON_GetObjectItem(MoteSettings,"rotation");
	if( !rotation ) {
		cJSON_AddNumberToObject(MoteSettings,"rotation",DEVICE_Prop_Int("rotation"));
	} else {
		if( rotation->type == cJSON_NULL )
			cJSON_ReplaceItemInObject(MoteSettings,"rotation",cJSON_CreateNumber(DEVICE_Prop_Int("rotation")));
	}
	MOTE_RefreshSetting();
	
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

	mote_create	= dlsym(handle, "video_scene_subscriber_create");
	mote_delete = dlsym(handle, "video_scene_subscriber_delete");  
	mote_subscribe	= dlsym(handle, "video_scene_subscriber_subscribe");
	mote_unsubscribe = dlsym(handle, "video_scene_subscriber_unsubscribe");
	mote_disconnect_callback = dlsym(handle, "video_scene_subscriber_set_on_disconnect_callback");
	mote_message_callback = dlsym(handle, "video_scene_subscriber_set_on_message_arrived_callback");
	mote_boundingbox	= dlsym(handle, "video_scene_subscriber_set_bounding_box_enabled");
	mote_polygon = dlsym(handle, "video_scene_subscriber_set_polygon_enabled");
	mote_velocity = dlsym(handle, "video_scene_subscriber_set_velocity_enabled");
	mote_low_confident = dlsym(handle, "video_scene_subscriber_set_low_confident_enabled");

	gchar *dl_error;
	if((dl_error = dlerror()) != NULL){
		LOG_WARN("Problems loading MOTE scene library\n");
		dlclose(handle);
		LOG_TRACE("%s: Exit Error\n",__func__);
		return 0;
	}

	// ------------ Setup MOTE subscriptions -------
	MOTE_Handler = (*mote_create)(0, 2, &MOTE_dummy_user_data);
	if( MOTE_Handler == NULL ) {
		LOG_WARN("MOTE not initialized");
		LOG_TRACE("%s: Exit Error\n",__func__);
		return 0;
	}

	if( (*mote_disconnect_callback)(MOTE_Handler, MOTE_onDisconnect) < 0 ) {
		LOG_WARN("Could not set MOTE disconnect callback");
		(*mote_delete)(MOTE_Handler);
		MOTE_Handler = 0;
		LOG_TRACE("%s: Exit Error\n",__func__);
		return 0;
	}
 
	if( (*mote_boundingbox)(MOTE_Handler ) < 0 ) {
		LOG_WARN("Could not enable MOTE bounding box");
	}

	STATUS_SetString("app","mote","Initialized");
	HTTP_Node( "mote", MOTE_HTTP );
	
	return 1;
} 