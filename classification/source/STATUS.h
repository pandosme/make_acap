/*------------------------------------------------------------------
 *  Fred Juhlin (2022)
 *------------------------------------------------------------------*/
 
#ifndef _STATUS_H_
#define _STATUS_H_

#include "cJSON.h"

#ifdef  __cplusplus
extern "C" {
#endif

int STATUS_Init();
//Get Data
cJSON*       STATUS();
cJSON*       STATUS_Group( const char *name );
int          STATUS_Bool( const char *group, const char *name );
int          STATUS_Int( const char *group, const char *name );
double       STATUS_Double( const char *group, const char *name );
char*        STATUS_String( const char *group, const char *name );

//Set Data
void STATUS_SetBool( const char *group, const char *name, int state );
void STATUS_SetNumber(  const char *group, const char *name, double value );
void STATUS_SetString(  const char *group, const char *name, const char *string );

#ifdef  __cplusplus
}
#endif

#endif
