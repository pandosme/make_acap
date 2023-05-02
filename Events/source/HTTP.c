/*------------------------------------------------------------------
 *  Copyright Fred Juhlin (2023)
 *------------------------------------------------------------------*/

#include <stdio.h>
#include <stdlib.h>
#include <syslog.h>
#include <string.h>
#include <axsdk/axhttp.h>
#include "HTTP.h"
#include "FILE.h"


#define LOG(fmt, args...)    { syslog(LOG_INFO, fmt, ## args); printf(fmt, ## args); }
#define LOG_WARN(fmt, args...)    { syslog(LOG_WARNING, fmt, ## args); printf(fmt, ## args); }
//#define LOG_TRACE(fmt, args...)    { syslog(LOG_INFO, fmt, ## args); printf(fmt, ## args); }
#define LOG_TRACE(fmt, args...)    {}

AXHttpHandler  *HTTP_handler = 0;
GHashTable     *HTTP_node_table = 0; 
gchar          *HTTP_Package_ID = 0;
static void     HTTP_main_callback(const gchar *path,const gchar *method,const gchar *query,GHashTable *params,GOutputStream *output_stream,gpointer user_data);


void
HTTP_Close() {
  g_free( HTTP_Package_ID );
  if( HTTP_handler ) {
    ax_http_handler_free( HTTP_handler );
    HTTP_handler = 0;
  }
  if( HTTP_node_table ) {
    g_hash_table_destroy( HTTP_node_table );
    HTTP_node_table = 0;
  } 
}

int
HTTP_Node( const char *name, HTTP_Callback callback ) {
	if( !HTTP_handler ) {
		LOG_WARN("HTTP_cgi: HTTP handler not initialized\n");
		return 0;
	}

	gchar path[128];
	g_snprintf (path, 128, "/local/%s/%s", HTTP_Package_ID, name );

	LOG_TRACE("%s:%s", __func__, path );

	if( !HTTP_node_table )
		HTTP_node_table = g_hash_table_new_full(g_str_hash, g_str_equal,g_free, NULL);
	g_hash_table_insert( HTTP_node_table, g_strdup( path ), (gpointer*)callback);

	LOG_TRACE("%s: Exit", __func__);

	return 1;
}

int
HTTP_Respond_String( HTTP_Response response,const gchar *fmt, ...) {
  va_list ap;
  gchar *tmp_str;
  GDataOutputStream *stream = (GDataOutputStream *)response;
  if( !stream ) {
    LOG_WARN("HTTP_Respond_String: Cannot send data to http.  Handler = NULL\n");
    return 0;
  }
  
  va_start(ap, fmt);
  tmp_str = g_strdup_vprintf(fmt, ap);
  g_data_output_stream_put_string((GDataOutputStream *)response, tmp_str, NULL, NULL);
  
  g_free(tmp_str);

  va_end(ap);
  return 1;
}

int
HTTP_Respond_Data( HTTP_Response response, size_t count, void *data ) {
  gsize data_sent;
  
  if( count == 0 || data == 0 ) {
    LOG_WARN("HTTP_Data: Invalid data\n");
    return 0;
  }
  
  if( !g_output_stream_write_all((GOutputStream *)response, data, count, &data_sent, NULL, NULL) ) {
    LOG_WARN("HTTP_Data: Error sending data.");
    return 0;
  }  
  return 1;
}

const char*
HTTP_Request_Param( const HTTP_Request request, const char* name) {
  gchar *value;
  if( !request )
    return 0;
  if(!g_hash_table_lookup_extended((GHashTable *)request, name, NULL, (gpointer*)&value)) {
//    printf("HTTP_Request_Param: Invalid option %s\n", name);
    return 0;
  }
  return value;   
}

cJSON*
HTTP_Request_JSON( const HTTP_Request request, const char *param ) {
  const char *jsonstring;
  cJSON *object;
  jsonstring = HTTP_Request_Param( request, param);
  if( !jsonstring ) {
    return 0;
  }
  object = cJSON_Parse(jsonstring);
  if(!object) {
    LOG_WARN("HTTP_Request_JSON: Invalid JSON: %s",jsonstring);
    return 0;
  }
  return object;
}

int
HTTP_Header_XML( HTTP_Response response ) {
  HTTP_Respond_String( response,"Content-Type: text/xml; charset=utf-8; Cache-Control: no-cache\r\n\r\n");
  HTTP_Respond_String( response,"<?xml version=\"1.0\"?>\r\n");
  return 1;
}

int
HTTP_Header_JSON( HTTP_Response response ) {
  HTTP_Respond_String( response,"Content-Type: application/json; charset=utf-8; Cache-Control: no-cache\r\n\r\n");
  return 1;
}

int
HTTP_Header_TEXT( HTTP_Response response ) {
  HTTP_Respond_String( response,"Content-Type: text/plain; charset=utf-8; Cache-Control: no-cache\r\n\r\n");
  return 1;
}

int
HTTP_Header_FILE( const HTTP_Response response, const char *filename, const char *contenttype, unsigned filelength ) {
  HTTP_Respond_String( response, "HTTP/1.1 200 OK\r\n");
  HTTP_Respond_String( response, "Content-Description: File Transfer\r\n");
  HTTP_Respond_String( response, "Content-Type: %s\r\n", contenttype);
  HTTP_Respond_String( response, "Content-Disposition: attachment; filename=%s\r\n", filename);
  HTTP_Respond_String( response, "Content-Transfer-Encoding: binary\r\n");
  HTTP_Respond_String( response, "Content-Length: %u\r\n", filelength );
  HTTP_Respond_String( response, "\r\n");
  return 1;
}

int
HTTP_Respond_Error( HTTP_Response response, int code, const char *message ) {
  HTTP_Respond_String( response,"status: %d %s Error\r\nContent-Type: text/plain\r\n\r\n", code, (code < 500) ? "Client" : (code < 600) ? "Server":"");
  if( code < 500 )
    LOG_WARN("HTTP response %d: %s\n", code, message);
  if( code >= 500 )
    LOG_WARN("HTTP response %d: %s\n", code, message);
  HTTP_Respond_String( response,"%s", message);
  return 1;
}

int
HTTP_Respond_Text( HTTP_Response response, const char *message ) {
  HTTP_Header_TEXT( response );
  HTTP_Respond_String( response,"%s", message);
  return 1;
}

int
HTTP_Respond_JSON( HTTP_Response response, cJSON *object) {
  char *jsonstring;
  if( !object ) {
    LOG_WARN("HTTP_Respond_JSON: Invalid object");
    return 0;
  }
  jsonstring = cJSON_Print( object );  
  HTTP_Header_JSON( response );
  HTTP_Respond_String( response, jsonstring );
  free( jsonstring );
  return 0;
}

static void
HTTP_main_callback(const gchar *path,const gchar *method, const gchar *query, GHashTable *request, GOutputStream *output_stream, gpointer user_data){
  GDataOutputStream *response;
  gchar *key;
  HTTP_Callback callback = 0;

  if( request ) {
    LOG_TRACE("HTTP request: %s?%s\n", path, query);
  } else  {
    LOG_TRACE("HTTP request: %s\n", path);
  }
  
  response = g_data_output_stream_new(output_stream);

  if( !HTTP_node_table ) {
    HTTP_Respond_Error( response, 500, "HTTP_main_callback: Invalid table" );
    return;
  }
  if( !g_hash_table_lookup_extended( HTTP_node_table, path, (gpointer*)&key,(gpointer*)&callback) ) {
    LOG_WARN("HTTP_main_callback: CGI table lookup failed for %s (key = %s)\n", path, key );
  }

  if( callback ) {
    callback( (HTTP_Response)response, (HTTP_Request) request);
  } else {
    HTTP_Respond_Error( response,400,"HTTP_main_callback: No valid HTTP consumer");
  }
  g_object_unref(response);
  (void) user_data;
}

void
HTTP_LICENSE(const HTTP_Response response,const HTTP_Request request) {
	LOG("License");

	FILE* f = FILE_Open( "LICENSE", "r" );
	if( !f ) {
		HTTP_Respond_Error( response, 500, "No license found");
		return;
	}
	char* buffer = malloc(10000);
	if(!buffer) {
		HTTP_Respond_Error( response, 500, "No license found");
		return;
	}
	int bytes = fread(buffer, sizeof(char), 10000, f);
	fclose(f);
	if( !bytes ) {
		free(buffer);
		HTTP_Respond_Error( response, 500, "No license found");
		return;
	}
	HTTP_Header_TEXT(response );	
	HTTP_Respond_Data( response, bytes, buffer );
	free(buffer);
}

void
HTTP( const char *package ) {
LOG_TRACE("%s:%s", __func__, package);
	HTTP_Package_ID = g_strdup( package );

	if( !HTTP_handler )
		HTTP_handler = ax_http_handler_new( HTTP_main_callback, NULL);
	if( !HTTP_handler )
		LOG_WARN("HTTP_init: Failed to initialize HTTP\n");
	
	gchar path[128];
	g_snprintf (path, 128, "/local/%s/LICENSE", package );

	if( !HTTP_node_table )
		HTTP_node_table = g_hash_table_new_full(g_str_hash, g_str_equal,g_free, NULL);
	g_hash_table_insert( HTTP_node_table, g_strdup( path ), HTTP_LICENSE);
LOG_TRACE("%s: Exit", __func__);
	
}
