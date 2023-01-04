/*------------------------------------------------------------------
 *  Copyright Fred Juhlin (2021)
 *  License:
 *  All rights reserved.  This code may not be used in commercial
 *  products without permission from Fred Juhlin.
 *------------------------------------------------------------------*/
#include <stdio.h>
#include <stdlib.h>
#include <glib.h>
#include <glib/gi18n.h>
#include <string.h>
#include <syslog.h>
#include <capture.h>

#include "cJSON.h"
#include "HTTP.h"
#include "FILE.h"
#include "DEVICE.h"
#include "base64.h"


#define LOG(fmt, args...)    { syslog(LOG_INFO, fmt, ## args); printf(fmt, ## args);}
#define LOG_WARN(fmt, args...)    { syslog(LOG_WARNING, fmt, ## args); printf(fmt, ## args);}
//#define LOG_TRACE(fmt, args...)    { printf(fmt, ## args); }
#define LOG_TRACE(fmt, args...)    {}


//
cJSON*
Image_JSON_Base64( const char *resolution ) {
	LOG_TRACE("%s:(%s)\n",__func__,resolution);

	char mediaProfile[32];
	sprintf(mediaProfile,"resolution=%s",resolution);

	media_stream *stream = 0;

	stream = capture_open_stream( IMAGE_JPEG, mediaProfile );

	if( !stream ) {
		LOG_WARN("%s: Unable to open stream for %s\n",__func__,mediaProfile);
		return 0;
	}

	media_frame *frame = capture_get_frame( stream );
	capture_close_stream( stream );
	if( !frame ) {
		LOG_WARN("%s: Error capture image %s\n",__func__,mediaProfile);
		return 0;
	}

	size_t size = capture_frame_size( frame );
	const unsigned char *data = (const unsigned char *)capture_frame_data( frame );
	
	if( !size || !data ) {
		LOG_WARN("%s: No size or data\n");
		return 0;
	}

	LOG_TRACE("%s:Image captures\n",__func__);

	cJSON *object = cJSON_CreateObject();
	cJSON_AddStringToObject(object,"resolution",resolution);
	cJSON_AddStringToObject(object,"aspect",DEVICE_Prop("aspect"));
	cJSON_AddNumberToObject(object,"size",size);

	size_t encodedSize = 0;
	unsigned char *image64 = base64_encode(data, size, &encodedSize);
	cJSON* base64 = cJSON_CreateString("");
	free(base64->valuestring);
	base64->valuestring = (char *)image64;	
	capture_frame_free(frame);
	
	cJSON_AddItemToObject(object,"base64",base64);

	return object;
}
