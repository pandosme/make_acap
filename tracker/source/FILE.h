/*------------------------------------------------------------------
 *  Copyright Fred Juhlin (2021)
 *  License:
 *  All rights reserved.  This code may not be used in commercial
 *  products without permission from Fred Juhlin.
 *------------------------------------------------------------------*/
 
#ifndef _LOCALFILE_H_
#define _LOCALFILE_H_

#include <stdio.h>
#include <stdlib.h>
#include "cJSON.h"

#ifdef  __cplusplus
extern "C" {
#endif

int    		FILE_Init( const char *package_id );
const char *FILE_AppPath();
FILE*  		FILE_Open( const char *filepath, const char *mode );
int    		FILE_Delete( const char *filepath );
cJSON* 		FILE_Read( const char *filepath );
int    		FILE_Write( const char *filepath,  cJSON* object );
int			FILE_WriteData( const char *filepath, const char *data );
int    		FILE_exists( const char *filepath );

#ifdef  __cplusplus
}
#endif

#endif
