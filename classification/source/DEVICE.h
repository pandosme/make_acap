/*------------------------------------------------------------------
 *  Fred Juhlin (2023)
 *------------------------------------------------------------------
 */
 
 #ifndef _DEVICEPROPERTIES_H_
#define _DEVICEPROPERTIES_H_

#include "cJSON.h"

#ifdef  __cplusplus
extern "C" {
#endif

cJSON*		DEVICE();
void        DEVICE_Init( const char *packageID );
const char *DEVICE_Prop( const char *name );
int         DEVICE_Prop_Int( const char *name );
cJSON      *DEVICE_JSON( const char *name );
int			DEVICE_Seconds_Since_Midnight();
double		DEVICE_Timestamp();  //UTC in milliseconds
const char* DEVICE_Local_Time(); //YYYY-MM-DD HH:MM:SS
const char* DEVICE_ISOTime(); //YYYY-MM-DDTHH:MM:SS+0000
const char* DEVICE_Date(); //YYYY-MM-DD
const char* DEVICE_Time(); //HH:MM:SS
double		DEVICE_Uptime();
double		DEVICE_CPU_Average();
double		DEVICE_Network_Average();
#ifdef  __cplusplus
}
#endif

#endif
