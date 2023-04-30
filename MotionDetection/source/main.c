#include <stdio.h>
#include <stdlib.h>
#include <glib.h>
#include <glib/gi18n.h>
#include <string.h>
#include <syslog.h>

#include "APP.h"
#include "HTTP.h"
#include "MotionDetection.h"

#define LOG(fmt, args...)    { syslog(LOG_INFO, fmt, ## args); printf(fmt, ## args);}
#define LOG_WARN(fmt, args...)    { syslog(LOG_WARNING, fmt, ## args); printf(fmt, ## args);}
#define LOG_TRACE(fmt, args...)    { syslog(LOG_INFO, fmt, ## args); printf(fmt, ## args); }
//#define LOG_TRACE(fmt, args...)    {}

#define APP_PACKAGE	"motiondetection"

void
DetectionsCallback( double timestamp, cJSON* list) {
	//Process detections
	cJSON* detection = list->child;
	while( detection ) {
		int id = cJSON_GetObjectItem(detection,"id")->valueint;
		int x = cJSON_GetObjectItem(detection,"x")->valueint;
		int y = cJSON_GetObjectItem(detection,"y")->valueint;
		int w = cJSON_GetObjectItem(detection,"w")->valueint;
		int h = cJSON_GetObjectItem(detection,"h")->valueint;
		int cx = cJSON_GetObjectItem(detection,"cx")->valueint;
		int cy = cJSON_GetObjectItem(detection,"cy")->valueint;

		detection = detection->next;
	}
}

static GMainLoop *main_loop = NULL;

int 
main(void) {
	openlog(APP_PACKAGE, LOG_PID|LOG_CONS, LOG_USER);
	
	APP(APP_PACKAGE, NULL);  //No settings
	APP_Register("MotionDetection",MotionDetection(DetectionsCallback) );

	main_loop = g_main_loop_new(NULL, FALSE);
	g_main_loop_run(main_loop);

	return 0;
}
