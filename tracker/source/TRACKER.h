/*------------------------------------------------------------------
 *  Fred Juhlin (2022)
 *------------------------------------------------------------------*/

#ifndef _TRACKER_H_
#define _TRACKER_H_

#include <stdio.h>
#include "cJSON.h"

#ifdef  __cplusplus
extern "C" {
#endif

typedef void (*TRACKER_Callback) (cJSON* tracker);

int		TRACKER_Init( TRACKER_Callback callback );
cJSON*  TRACKER_Settings();

#ifdef  __cplusplus
}
#endif

#endif
