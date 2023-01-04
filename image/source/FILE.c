/*------------------------------------------------------------------
 *  Fred Juhlin (2022)
 *------------------------------------------------------------------*/
 
#include <syslog.h>
#include <stdio.h>
#include <stdlib.h>
#include "FILE.h"

#define LOG(fmt, args...)    { syslog(LOG_INFO, fmt, ## args); }
#define LOG_WARN(fmt, args...)    { syslog(LOG_WARNING, fmt, ## args); }
//#define LOG_TRACE(fmt, args...)    { syslog(LOG_INFO, fmt, ## args); printf(fmt, ## args); }
#define LOG_TRACE(fmt, args...)    {}

char FILE_Path[128] = "";

const char*
FILE_AppPath() {
	return FILE_Path;
}


int
FILE_Init( const char *package_id ) {
	LOG_TRACE("FILE_Init: %s",package_id);
	sprintf( FILE_Path,"/usr/local/packages/%s/", package_id );
	return 1;
}

FILE*
FILE_Open( const char *filepath, const char *mode ) {
	char   fullpath[128];
	sprintf( fullpath, "%s%s", FILE_Path, filepath );
	FILE *file = fopen( fullpath , mode );;
	if(file) {
		LOG_TRACE("Opening File %s (%s)\n",fullpath, mode);
	} else {
		LOG_TRACE("Error opening file %s\n",fullpath);
	}
	return file;
}

int
FILE_Delete( const char *filepath ) {

  char fullpath[128];

  sprintf( fullpath, "%s%s", FILE_Path, filepath );

  if( remove ( fullpath ) != 0 ) {
    LOG_WARN("FILE_delete: Could not delete %s\n", fullpath);
    return 0;
  }
  return 1;	
}

cJSON*
FILE_Read( const char* filepath ) {
  FILE*   file;
  char   *jsonString;
  cJSON  *object;
  size_t bytesRead, size;

  file = FILE_Open( filepath , "r");
  if( !file ) {
//    LOG("%s does not exist\n", filepath);
    return 0;
  }

  fseek( file, 0, SEEK_END );
  size = ftell( file );
  fseek( file , 0, SEEK_SET);
  
  if( size < 2 ) {
    fclose( file );
    LOG_WARN("FILE_read: File size error in %s\n", filepath);
    return 0;
  }

  jsonString = malloc( size + 1 );
  if( !jsonString ) {
    fclose( file );
    LOG_WARN("FILE_read: Memory allocation error\n");
    return 0;
  }

  bytesRead = fread ( (void *)jsonString, sizeof(char), size, file );
  fclose( file );
  if( bytesRead != size ) {
    free( jsonString );
    LOG_WARN("FILE_read: Read error in %s\n", filepath);
    return 0;
  }

  jsonString[bytesRead] = 0;
  object = cJSON_Parse( jsonString );
  free( jsonString );
  if( !object ) {
    LOG_WARN("FILE_read: JSON Parse error for %s\n", filepath);
    return 0;
  }
  return object;
}

int
FILE_WriteData( const char *filepath, const char *data ) {
  FILE *file;
  if( !filepath || !data ) {
    LOG_WARN("FILE_write: Invalid input\n");
    return 0;
  }
  
  file = FILE_Open( filepath, "w" );
  if( !file ) {
    LOG_WARN("FILE_write: Error opening %s\n", filepath);
    return 0;
  }


  if( !fputs( data,file ) ) {
    LOG_WARN("FILE_write: Could not save data to %s\n", filepath);
    fclose( file );
    return 0;
  }
  fclose( file );
  return 1;
}

int
FILE_Write( const char *filepath,  cJSON* object )  {
  FILE *file;
  char *jsonString;
  if( !filepath || !object ) {
    LOG_WARN("FILE_write: Invalid input\n");
    return 0;
  }
  
  file = FILE_Open( filepath, "w" );
  if( !file ) {
    LOG_WARN("FILE_write: Error opening %s\n", filepath);
    return 0;
  }

  jsonString = cJSON_Print( object );

  if( !jsonString ) {
    LOG_WARN("FILE_write: JSON parse error for %s\n", filepath);
    fclose( file );
    return 0;
  }

  if( !fputs( jsonString,file ) ){
    LOG_WARN("FILE_write: Could not save data to %s\n", filepath);
    free( jsonString );
    fclose( file );
    return 0;
  }
  free( jsonString );
  fclose( file );
  return 1;
}

int
FILE_Exists( const char *filepath ) {
  FILE *file;
  file = FILE_Open( filepath, "r");
  if( file )
    fclose( file );
  return (file != 0 );
}
