/*------------------------------------------------------------------
 *  Fred Juhlin (2022)
 *------------------------------------------------------------------*/
 
#ifndef _APP_H_
#define _APP_H_

#include "cJSON.h"

#define APP_PACKAGE	"tracker"
#define APP_NAME	"Tracker"
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
