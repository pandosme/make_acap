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
#include "CLASSIFICATION.h"

#define LOG(fmt, args...)    { syslog(LOG_INFO, fmt, ## args); printf(fmt, ## args);}
#define LOG_WARN(fmt, args...)    { syslog(LOG_WARNING, fmt, ## args); printf(fmt, ## args);}
//#define LOG_TRACE(fmt, args...)    { syslog(LOG_INFO, fmt, ## args); printf(fmt, ## args); }
#define LOG_TRACE(fmt, args...)    {}

static GMainLoop *main_loop = NULL;

cJSON* detections = 0;

void
Classifications( double timestamp, cJSON* list) {
/*
	list: [
		{
			"id": number,		UID
			"x": number,		0-1000
			"y": number,		0-1000
			"w": number,		0-1000
			"h": number,		0-1000
			"confidenc": number,0-
			"classID":number,	0-8
			"class":string,		Human, Car, Truck, Bus, Motorcycle, Vehicle, Face,Licese plate, Other
			"type":string		Person, Vehicle, Other
		},
		...
	]
*/	

	if( detections )
		cJSON_Delete(detections);
	detections = cJSON_Duplicate( list,1);
}

static void
MAIN_http_detections(const HTTP_Response response,const HTTP_Request request) {

	if(!detections)
		detections = cJSON_CreateArray();
	HTTP_Respond_JSON( response ,detections );
}

int 
main(void) {
	openlog(APP_PACKAGE, LOG_PID|LOG_CONS, LOG_USER);
	
	FILE_Init( APP_PACKAGE );
	HTTP_Init( APP_PACKAGE );
	DEVICE_Init( APP_PACKAGE );
	STATUS_Init( APP_PACKAGE );
	APP_Init();

	CLASSIFICATION_Init(Classifications);

	HTTP_Node( "detections", MAIN_http_detections );

	main_loop = g_main_loop_new(NULL, FALSE);
	g_main_loop_run(main_loop);

	return 0;
}
