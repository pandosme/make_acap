/*------------------------------------------------------------------
 *  Fred Juhlin (2023)
 *  
 *  STATUS enables a simple way for service to report
 *  its status to (web) client
 *------------------------------------------------------------------*/
 
#ifndef _STATUS_H_
#define _STATUS_H_

#include "cJSON.h"

#ifdef  __cplusplus
extern "C" {
#endif


//Get Data
cJSON*       STATUS();  //Initialize
cJSON*       STATUS_Group( const char *name ); //Get a status group
int          STATUS_Bool( const char *group, const char *name );
int          STATUS_Int( const char *group, const char *name );
double       STATUS_Double( const char *group, const char *name );
char*        STATUS_String( const char *group, const char *name );
cJSON*       STATUS_Object( const char *group, const char *name );

//Set Data
void STATUS_SetBool( const char *group, const char *name, int state );
void STATUS_SetNumber(  const char *group, const char *name, double value );
void STATUS_SetString(  const char *group, const char *name, const char *string );
void STATUS_SetObject(  const char *group, const char *name, cJSON* data );

#ifdef  __cplusplus
}
#endif

#endif
