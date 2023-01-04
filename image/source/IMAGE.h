/*------------------------------------------------------------------
 *  (C) Fred Juhlin (2021)
 *------------------------------------------------------------------*/
 
#ifndef IMAGE_H
#define IMAGE_H

#include <capture.h>
#include "cJSON.h"


media_frame *Image_Capture( const char *profile );


cJSON* Image_JSON_Base64( const char *resolution );

#endif
