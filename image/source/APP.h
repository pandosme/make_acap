/*------------------------------------------------------------------
 * Fred Juhlin (2022)
 * Store App specific settings
 * GET /app
 * GET /app?settings={"some":"Parameter":"value":1}
 *------------------------------------------------------------------*/
 
#ifndef _APP_H_
#define _APP_H_

#include "cJSON.h"

#define APP_PACKAGE	"acapp_image"
#define APP_NAME	"ACAP Image"
#define APP_VERSION	"1.0"
#define APP_ID		1


#ifdef  __cplusplus
extern "C" {
#endif

int		APP_Init();
cJSON*	APP_Settings();

#ifdef  __cplusplus
}
#endif

#endif
