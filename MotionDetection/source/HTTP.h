/*------------------------------------------------------------------
 *  Copyright Fred Juhlin (2023)
 *  
 *  Wrapper around ACAP HTTP CGI
 *------------------------------------------------------------------*/
 
#ifndef _HTTP_H_
#define _HTTP_H_

#ifdef  __cplusplus
extern "C" {
#endif

#include <glib.h>
#include <gio/gio.h>
#include "cJSON.h"

typedef GDataOutputStream *HTTP_Response;
typedef GHashTable        *HTTP_Request;

typedef void (*HTTP_Callback) (const HTTP_Response response,const HTTP_Request request);

void        HTTP( const char *package );
void        HTTP_Close();
int         HTTP_Node( const char *nodename, HTTP_Callback callback );

const char *HTTP_Request_Param( const HTTP_Request request, const char *param);
cJSON      *HTTP_Request_JSON( const HTTP_Request request, const char *param );
			//JSON Object allocated.  Don't forget cJSON_Delet(object)

int         HTTP_Header_XML( HTTP_Response response );
int         HTTP_Header_JSON( HTTP_Response response ); 
int         HTTP_Header_TEXT( HTTP_Response response );
int         HTTP_Header_FILE( HTTP_Response response, const char *filename, const char *contenttype, unsigned filelength );
int         HTTP_Respond_String( HTTP_Response response,const char *fmt, ...);
int         HTTP_Respond_JSON(  HTTP_Response response, cJSON *object);
int         HTTP_Respond_Data( HTTP_Response response, size_t count, void *data );
int         HTTP_Respond_Error( HTTP_Response response, int code, const char *message );
int         HTTP_Respond_Text( HTTP_Response response, const char *message );

#ifdef  __cplusplus
}
#endif

#endif
