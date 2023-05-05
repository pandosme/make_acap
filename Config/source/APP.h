/*------------------------------------------------------------------
 *  Fred Juhlin (2023)
 *------------------------------------------------------------------*/
 
#ifndef _APP_H_
#define _APP_H_

#include "cJSON.h"

#ifdef  __cplusplus
extern "C" {
#endif


typedef void (*APP_Callback) ( const char *service, cJSON* data);

cJSON*		APP( const char *package, APP_Callback updateCallback );
const char* APP_Package();
const char* APP_Name();
cJSON*  	APP_Service(const char* service);
int			APP_Register(const char* service, cJSON* serviceSettings );

#ifdef  __cplusplus
}
#endif

#endif
