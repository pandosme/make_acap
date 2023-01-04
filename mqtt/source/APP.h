/*------------------------------------------------------------------
 *  Fred Juhlin (2022)
 *------------------------------------------------------------------*/
 
#ifndef _APP_H_
#define _APP_H_

#include "cJSON.h"

#define APP_PACKAGE	"acapp_mqtt"
#define APP_NAME	"ACAPP MQTT"
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
