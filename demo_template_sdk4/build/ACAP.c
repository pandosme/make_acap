/*------------------------------------------------------------------
 *  Fred Juhlin (2024)
 *------------------------------------------------------------------*/

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <syslog.h>
#include <string.h>
#include <time.h>
#include <math.h>
#include <unistd.h>
#include <ctype.h>
#include <sys/sysinfo.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <glib-object.h>
#include <glib.h>
#include <axsdk/axparameter.h>
#include <axsdk/axevent.h>
#include "ACAP.h"



#define LOG(fmt, args...)    { syslog(LOG_INFO, fmt, ## args); printf(fmt, ## args);}
#define LOG_WARN(fmt, args...)    { syslog(LOG_WARNING, fmt, ## args); printf(fmt, ## args);}
//#define LOG_TRACE(fmt, args...)    { syslog(LOG_INFO, fmt, ## args); printf(fmt, ## args); }
#define LOG_TRACE(fmt, args...)    {}

#define MAX_HTTP_NODES 32
#define MAX_PATH_LENGTH 128

typedef struct {
    char path[MAX_PATH_LENGTH];
    ACAP_HTTP_Callback callback;
} HTTPNode;

static HTTPNode http_nodes[MAX_HTTP_NODES];
static int http_node_count = 0;

cJSON* app = 0;
char ACAP_package_name[30];
ACAP_Config_Update	ACAP_UpdateCallback = 0;

static void
ACAP_ENDPOINT_app(const ACAP_HTTP_Response response,const ACAP_HTTP_Request request) {
	ACAP_HTTP_Respond_JSON( response, app );
}	

static void
ACAP_ENDPOINT_settings(const ACAP_HTTP_Response response,const ACAP_HTTP_Request request) {
	
	const char* json = ACAP_HTTP_Request_Param( request, "json");
	if( !json )
		json = ACAP_HTTP_Request_Param( request, "set");
	if( !json ) {
		ACAP_HTTP_Respond_JSON( response, cJSON_GetObjectItem(app,"settings") );
		return;
	}

	cJSON *params = cJSON_Parse(json);
	if(!params) {
		ACAP_HTTP_Respond_Error( response, 400, "Invalid JSON data");
		return;
	}
	LOG_TRACE("%s: %s\n",__func__,json);
	cJSON* settings = cJSON_GetObjectItem(app,"settings");
	cJSON* param = params->child;
	while(param) {
		if( cJSON_GetObjectItem(settings,param->string ) )
			cJSON_ReplaceItemInObject(settings,param->string,cJSON_Duplicate(param,1) );
		param = param->next;
	}
	cJSON_Delete(params);
	ACAP_FILE_Write( "localdata/settings.json", settings);	
	ACAP_HTTP_Respond_Text( response, "OK" );
	if( ACAP_UpdateCallback )
		ACAP_UpdateCallback("settings",settings);
}


cJSON*
ACAP( const char *package, ACAP_Config_Update callback ) {
	LOG_TRACE("%s: ENTRY\n",__func__);
	sprintf(ACAP_package_name,"%s",package);
	ACAP_FILE_Init();

	cJSON* device = ACAP_DEVICE();

	ACAP_UpdateCallback = callback;
	
	app = cJSON_CreateObject();

	cJSON_AddItemToObject(app, "manifest", ACAP_FILE_Read( "manifest.json" ));

	cJSON* settings = ACAP_FILE_Read( "html/config/settings.json" );
	if(!settings)
		settings = cJSON_CreateObject();

	cJSON* savedSettings = ACAP_FILE_Read( "localdata/settings.json" );
	if( savedSettings ) {
		cJSON* prop = savedSettings->child;
		while(prop) {
			if( cJSON_GetObjectItem(settings,prop->string ) )
				cJSON_ReplaceItemInObject(settings,prop->string,cJSON_Duplicate(prop,1) );
			prop = prop->next;
		}
		cJSON_Delete(savedSettings);
	}

	cJSON_AddItemToObject(app, "settings",settings);

	ACAP_HTTP();
	ACAP_EVENTS();

	ACAP_Register("status", ACAP_STATUS());
	ACAP_Register("device", device);

	
	ACAP_HTTP_Node( "app", ACAP_ENDPOINT_app );	
	ACAP_HTTP_Node( "settings", ACAP_ENDPOINT_settings );

	if( ACAP_UpdateCallback )
		ACAP_UpdateCallback("settings",settings);

	LOG_TRACE("%s:Exit\n",__func__);
	
	return settings;
}


const char* ACAP_Package() {
	cJSON* manifest = cJSON_GetObjectItem(app,"manifest");
	if(!manifest) {
		LOG_WARN("%s: invalid manifest\n",__func__);
		return "Undefined";
	}
	cJSON* acapPackageConf = cJSON_GetObjectItem(manifest,"acapPackageConf");
	if(!acapPackageConf) {
		LOG_WARN("%s: invalid acapPackageConf\n",__func__);
		return "Undefined";
	}
	cJSON* setup = cJSON_GetObjectItem(acapPackageConf,"setup");
	if(!setup) {
		LOG_WARN("%s: invalid setup\n",__func__);
		return "Undefined";
	}
	return cJSON_GetObjectItem(setup,"appName")->valuestring;
}
	
const char* ACAP_Name() {
	cJSON* manifest = cJSON_GetObjectItem(app,"manifest");
	if(!manifest) {
		LOG_WARN("%s: invalid manifest\n",__func__);
		return "Undefined";
	}
	cJSON* acapPackageConf = cJSON_GetObjectItem(manifest,"acapPackageConf");
	if(!acapPackageConf) {
		LOG_WARN("%s: invalid acapPackageConf\n",__func__);
		return "Undefined";
	}
	cJSON* setup = cJSON_GetObjectItem(acapPackageConf,"setup");
	if(!setup) {
		LOG_WARN("%s: invalid setup\n",__func__);
		return "Undefined";
	}
	return cJSON_GetObjectItem(setup,"friendlyName")->valuestring;
}

cJSON*
ACAP_Service(const char* service) {
	cJSON* reqestedService = cJSON_GetObjectItem(app, service );
	if( !reqestedService ) {
		LOG_WARN("%s: %s is undefined\n",__func__,service);
		return 0;
	}
	return reqestedService;
}

int
ACAP_Register(const char* service, cJSON* serviceSettings ) {
	LOG_TRACE("%s: %s\n",__func__,service);
	if( cJSON_GetObjectItem(app,service) ) {
		LOG_TRACE("%s: %s already registered\n",__func__,service);
		return 1;
	}
	cJSON_AddItemToObject( app, service, serviceSettings );
	return 1;
}


char ACAP_FILE_Path[128] = "";

const char*
ACAP_FILE_AppPath() {
	return ACAP_FILE_Path;
}


int
ACAP_FILE_Init() {
	LOG_TRACE("%s: ENTRY\n",__func__);
	sprintf( ACAP_FILE_Path,"/usr/local/packages/%s/", ACAP_package_name );
	return 1;
}

FILE*
ACAP_FILE_Open( const char *filepath, const char *mode ) {
	char   fullpath[128];
	sprintf( fullpath, "%s", filepath );
	FILE *file = fopen( fullpath , mode );;
	if(file) {
		LOG_TRACE("Opening File %s (%s)\n",fullpath, mode);
	} else {
		LOG_TRACE("Error opening file %s\n",fullpath);
	}
	return file;
}

int
ACAP_FILE_Delete( const char *filepath ) {

  char fullpath[128];

  sprintf( fullpath, "%s%s", ACAP_FILE_Path, filepath );

  if( remove ( fullpath ) != 0 ) {
    LOG_WARN("ACAP_FILE_delete: Could not delete %s\n", fullpath);
    return 0;
  }
  return 1;	
}

cJSON*
ACAP_FILE_Read( const char* filepath ) {
	FILE*   file;
	char   *jsonString;
	cJSON  *object;
	size_t bytesRead, size;

	LOG_TRACE("%s: %s\n",__func__,filepath);

	file = ACAP_FILE_Open( filepath , "r");
	if( !file ) {
		//    LOG("%s does not exist\n", filepath);
		return 0;
	}

	fseek( file, 0, SEEK_END );
	size = ftell( file );
	fseek( file , 0, SEEK_SET);

	if( size < 2 ) {
		fclose( file );
		LOG_WARN("ACAP_FILE_read: File size error in %s\n", filepath);
		return 0;
	}

	jsonString = malloc( size + 1 );
	if( !jsonString ) {
		fclose( file );
		LOG_WARN("ACAP_FILE_read: Memory allocation error\n");
		return 0;
	}

	bytesRead = fread ( (void *)jsonString, sizeof(char), size, file );
	fclose( file );
	if( bytesRead != size ) {
		free( jsonString );
		LOG_WARN("ACAP_FILE_read: Read error in %s\n", filepath);
	return 0;
	}

	jsonString[bytesRead] = 0;
	object = cJSON_Parse( jsonString );
	free( jsonString );
	if( !object ) {
		LOG_WARN("ACAP_FILE_read: JSON Parse error for %s\n", filepath);
		return 0;
	}
	return object;
}

int
ACAP_FILE_WriteData( const char *filepath, const char *data ) {
  FILE *file;
  if( !filepath || !data ) {
    LOG_WARN("ACAP_FILE_write: Invalid input\n");
    return 0;
  }
  
  file = ACAP_FILE_Open( filepath, "w" );
  if( !file ) {
    LOG_WARN("ACAP_FILE_write: Error opening %s\n", filepath);
    return 0;
  }


  if( !fputs( data,file ) ) {
    LOG_WARN("ACAP_FILE_write: Could not save data to %s\n", filepath);
    fclose( file );
    return 0;
  }
  fclose( file );
  return 1;
}

int
ACAP_FILE_Write( const char *filepath,  cJSON* object )  {
  FILE *file;
  char *jsonString;
  if( !filepath || !object ) {
    LOG_WARN("ACAP_FILE_write: Invalid input\n");
    return 0;
  }
  
  file = ACAP_FILE_Open( filepath, "w" );
  if( !file ) {
    LOG_WARN("ACAP_FILE_write: Error opening %s\n", filepath);
    return 0;
  }

  jsonString = cJSON_Print( object );

  if( !jsonString ) {
    LOG_WARN("ACAP_FILE_write: JSON parse error for %s\n", filepath);
    fclose( file );
    return 0;
  }

  if( !fputs( jsonString,file ) ){
    LOG_WARN("ACAP_FILE_write: Could not save data to %s\n", filepath);
    free( jsonString );
    fclose( file );
    return 0;
  }
  free( jsonString );
  fclose( file );
  return 1;
}

int
ACAP_FILE_Exists( const char *filepath ) {
  FILE *file;
  file = ACAP_FILE_Open( filepath, "r");
  if( file )
    fclose( file );
  return (file != 0 );
}


int ACAP_HTTP_Node(const char *nodename, ACAP_HTTP_Callback callback) {
    // Prevent buffer overflow
    if (http_node_count >= MAX_HTTP_NODES) {
        LOG_WARN("Maximum HTTP nodes reached");
        return 0;
    }

    // Construct full path
    char full_path[MAX_PATH_LENGTH];
    if (nodename[0] == '/') {
        snprintf(full_path, sizeof(full_path), "/local/%s%s", ACAP_Package(), nodename);
    } else {
        snprintf(full_path, sizeof(full_path), "/local/%s/%s", ACAP_Package(), nodename);
    }

    // Check for duplicate paths
    for (int i = 0; i < http_node_count; i++) {
        if (strcmp(http_nodes[i].path, full_path) == 0) {
            LOG_WARN("Duplicate HTTP node path: %s", full_path);
            return 0;
        }
    }

    // Add new node
    strncpy(http_nodes[http_node_count].path, full_path, MAX_PATH_LENGTH - 1);
    http_nodes[http_node_count].callback = callback;
    http_node_count++;

    return 1;
}

static const char*
get_path_without_query(const char* uri) {
    static char path[MAX_PATH_LENGTH];
    const char* query = strchr(uri, '?');
    
    if (query) {
        size_t path_length = query - uri;
        if (path_length >= MAX_PATH_LENGTH) {
            path_length = MAX_PATH_LENGTH - 1;
        }
        strncpy(path, uri, path_length);
        path[path_length] = '\0';
        return path;
    }
    
    return uri;
}


int ACAP_HTTP_Process() {
    FCGX_Request request;
    int sock;
    char* socket_path = NULL;

    // Initialize on first call
    static int initialized = 0;
    if (!initialized) {
        FCGX_Init();
        initialized = 1;
    }

    // Get socket path
    socket_path = getenv("FCGI_SOCKET_NAME");
    if (!socket_path) {
        LOG_WARN("Failed to get FCGI_SOCKET_NAME");
        return 0;
    }

    // Open socket if not already open
    static int fcgi_sock = -1;
    if (fcgi_sock == -1) {
        fcgi_sock = FCGX_OpenSocket(socket_path, 5);
        chmod(socket_path, 0777);  // Simplified chmod
    }

    // Initialize request
    if (FCGX_InitRequest(&request, fcgi_sock, 0) != 0) {
        LOG_WARN("FCGX_InitRequest failed");
        return 0;
    }

    // Accept the request
    if (FCGX_Accept_r(&request) != 0) {
        return 0;
    }

    // Get the request URI
    const char* uriString = FCGX_GetParam("REQUEST_URI", request.envp);

    if (!uriString) {
        // Use FCGX_FPrintF for error response
        FCGX_FPrintF(request.out, "Status: 400 Bad Request\r\n");
        FCGX_FPrintF(request.out, "Content-Type: text/plain\r\n\r\n");
        FCGX_FPrintF(request.out, "Invalid URI");
        FCGX_Finish_r(&request);
        return 0;
    }

    LOG_TRACE("%s: %s\n", __func__, uriString);


    // Find matching callback
	const char* pathOnly = get_path_without_query(uriString);	
    ACAP_HTTP_Callback matching_callback = NULL;
    for (int i = 0; i < http_node_count; i++) {
		LOG_TRACE("%s: %s %s\n",__func__,http_nodes[i].path,pathOnly);
        if (strcmp(http_nodes[i].path, pathOnly) == 0) {
            matching_callback = http_nodes[i].callback;
            break;
        }
    }

    // Process the request
    if (matching_callback) {
        matching_callback(&request, request);
    } else {
        // No matching handler found
        FCGX_FPrintF(request.out, "Status: 404 Not Found\r\n");
        FCGX_FPrintF(request.out, "Content-Type: text/plain\r\n\r\n");
        FCGX_FPrintF(request.out, "Not Found");
    }

    // Finish the request
    FCGX_Finish_r(&request);

    return 1;
}

int
ACAP_HTTP_Respond_String(ACAP_HTTP_Response response, const char *fmt, ...) {
    va_list ap;
    char tmp_str[4096];  // Allocate a fixed-size buffer instead of dynamic allocation

    if (!response->out) {
        LOG_WARN("ACAP_HTTP_Respond_String: Cannot send data to http. Handler = NULL\n");
        return 0;
    }
    
    va_start(ap, fmt);
    // Use vsnprintf to prevent buffer overflow
    vsnprintf(tmp_str, sizeof(tmp_str), fmt, ap);
    va_end(ap);

    // Use FCGX_FPrintF for writing to the FastCGI output stream
    int result = FCGX_FPrintF(response->out, "%s", tmp_str);

    return (result >= 0);
}

int
ACAP_HTTP_Respond_Data(ACAP_HTTP_Response response, size_t count, void *data) {
    if (count == 0 || data == 0) {
        LOG_WARN("%s: Invalid data\n",__func__);
        return 0;
    }

    int bytes_written = FCGX_PutStr((const char*)data, count, response->out);

    if (bytes_written != count) {
        LOG_WARN("%s: Error sending data. Wrote %d of %zu bytes\n", __func__, bytes_written, count);
        return 0;
    }
    return 1;
}

static char* url_decode(const char* src) {
    static char decoded[2048];  // Static buffer for the decoded string
    int i, j = 0;
    int len = strlen(src);
    
    for (i = 0; i < len && j < sizeof(decoded) - 1; i++) {
        if (src[i] == '%' && i + 2 < len) {
            char hex[3] = {src[i + 1], src[i + 2], 0};
            decoded[j++] = (char)strtol(hex, NULL, 16);
            i += 2;
        } else if (src[i] == '+') {
            decoded[j++] = ' ';
        } else {
            decoded[j++] = src[i];
        }
    }
    decoded[j] = '\0';
    
    return decoded;
}

const char* ACAP_HTTP_Request_Param(const ACAP_HTTP_Request request, const char* name) {
    if (!request.envp) {
        LOG_WARN("%s: Invalid request environment\n",__func__);
        return 0;
    }

    // Use FCGX_GetParam to retrieve query parameters or form data
    const char* value = FCGX_GetParam(name, request.envp);
    LOG_TRACE("%s: Environment %s %s\n",__func__,name,value?value:"No value");
    
    if (!value) {
        // If not found in environment, try parsing query string manually
        const char* query_string = FCGX_GetParam("QUERY_STRING", request.envp);
        if (query_string) {
            // Use multiple static buffers for different parameters
            static char param_values[10][1024];
            static int buffer_index = 0;
            
            char search_param[512];
            snprintf(search_param, sizeof(search_param), "%s=", name);
            
            char* found = strstr(query_string, search_param);
            if (found) {
                found += strlen(search_param);
                char* end = strchr(found, '&');
                
                // Use the next buffer in rotation
                char* param_value = param_values[buffer_index];
                buffer_index = (buffer_index + 1) % 10;
                
                if (end) {
                    strncpy(param_value, found, end - found);
                    param_value[end - found] = '\0';
                } else {
                    strcpy(param_value, found);
                }
                LOG_TRACE("%s: Query %s %s\n",__func__,name,param_value);
                
                // URL decode if it's the json parameter
                if (strcmp(name, "json") == 0) {
                    return url_decode(param_value);
                }
                return param_value;
            } else {
                LOG_WARN("%s: Query param %s not found\n",__func__,name);
            }
        }
        return 0;
    }
    
    // URL decode if it's the json parameter from environment
    if (strcmp(name, "json") == 0) {
        return url_decode(value);
    }
    return value;
}

cJSON*
ACAP_HTTP_Request_JSON( const ACAP_HTTP_Request request, const char *param ) {
  const char *jsonstring;
  cJSON *object;
  jsonstring = ACAP_HTTP_Request_Param( request, param);
  if( !jsonstring ) {
    return 0;
  }
  object = cJSON_Parse(jsonstring);
  if(!object) {
    LOG_WARN("ACAP_HTTP_Request_JSON: Invalid JSON: %s",jsonstring);
    return 0;
  }
  return object;
}

int
ACAP_HTTP_Header_XML( ACAP_HTTP_Response response ) {
  ACAP_HTTP_Respond_String( response,"Content-Type: text/xml; charset=utf-8; Cache-Control: no-cache\r\n\r\n");
  ACAP_HTTP_Respond_String( response,"<?xml version=\"1.0\"?>\r\n");
  return 1;
}

int
ACAP_HTTP_Header_JSON( ACAP_HTTP_Response response ) {
  ACAP_HTTP_Respond_String( response,"Content-Type: application/json; charset=utf-8; Cache-Control: no-cache\r\n\r\n");
  return 1;
}

int
ACAP_HTTP_Header_TEXT( ACAP_HTTP_Response response ) {
  ACAP_HTTP_Respond_String( response,"Content-Type: text/plain; charset=utf-8; Cache-Control: no-cache\r\n\r\n");
  return 1;
}

int
ACAP_HTTP_Header_FILE( const ACAP_HTTP_Response response, const char *filename, const char *contenttype, unsigned filelength ) {
  ACAP_HTTP_Respond_String( response, "HTTP/1.1 200 OK\r\n");
  ACAP_HTTP_Respond_String( response, "Content-Description: File Transfer\r\n");
  ACAP_HTTP_Respond_String( response, "Content-Type: %s\r\n", contenttype);
  ACAP_HTTP_Respond_String( response, "Content-Disposition: attachment; filename=%s\r\n", filename);
  ACAP_HTTP_Respond_String( response, "Content-Transfer-Encoding: binary\r\n");
  ACAP_HTTP_Respond_String( response, "Content-Length: %u\r\n", filelength );
  ACAP_HTTP_Respond_String( response, "\r\n");
  return 1;
}

int
ACAP_HTTP_Respond_Error( ACAP_HTTP_Response response, int code, const char *message ) {
  ACAP_HTTP_Respond_String( response,"status: %d %s Error\r\nContent-Type: text/plain\r\n\r\n", code, (code < 500) ? "Client" : (code < 600) ? "Server":"");
  if( code < 500 )
    LOG_WARN("HTTP response %d: %s\n", code, message);
  if( code >= 500 )
    LOG_WARN("HTTP response %d: %s\n", code, message);
  ACAP_HTTP_Respond_String( response,"%s", message);
  return 1;
}

int
ACAP_HTTP_Respond_Text( ACAP_HTTP_Response response, const char *message ) {
  ACAP_HTTP_Header_TEXT( response );
  ACAP_HTTP_Respond_String( response,"%s", message);
  return 1;
}

int
ACAP_HTTP_Respond_JSON( ACAP_HTTP_Response response, cJSON *object) {
  char *jsonstring;
  if( !object ) {
    LOG_WARN("ACAP_HTTP_Respond_JSON: Invalid object");
    return 0;
  }
  jsonstring = cJSON_Print( object );  
  ACAP_HTTP_Header_JSON( response );
  ACAP_HTTP_Respond_String( response, jsonstring );
  free( jsonstring );
  return 0;
}


void
ACAP_ENDPOINT_license(const ACAP_HTTP_Response response,const ACAP_HTTP_Request request) {

	FILE* f = ACAP_FILE_Open( "LICENSE", "r" );
	if( !f ) {
		ACAP_HTTP_Respond_Error( response, 500, "No license found");
		return;
	}
	char* buffer = malloc(10000);
	if(!buffer) {
		ACAP_HTTP_Respond_Error( response, 500, "No license found");
		return;
	}
	int bytes = fread(buffer, sizeof(char), 10000, f);
	fclose(f);
	if( !bytes ) {
		free(buffer);
		ACAP_HTTP_Respond_Error( response, 500, "No license found");
		return;
	}
	ACAP_HTTP_Header_TEXT(response );	
	ACAP_HTTP_Respond_Data( response, bytes, buffer );
	free(buffer);
}

void
ACAP_HTTP() {
}


void
ACAP_HTTP_Close() {
}

cJSON *ACAP_DEVICE_Container = 0;
AXParameter *ACAP_DEVICE_parameter_handler;

char**
string_split( char* a_str,  char a_delim) {
    char** result    = 0;
    size_t count     = 0;
    const char* tmp  = a_str;
    const char* last_comma = 0;
    char delim[2];
    delim[0] = a_delim;
    delim[1] = 0;
    /* Count how many elements will be extracted. */
    while (*tmp)
    {
        if (a_delim == *tmp)
        {
            count++;
            last_comma = tmp;
        }
        tmp++;
    }

    /* Add space for trailing token. */
    count += last_comma < (a_str + strlen(a_str) - 1);

    /* Add space for terminating null string so caller
       knows where the list of returned strings ends. */
    count++;
    result = malloc(sizeof(char*) * count);
    if (result) {
        size_t idx  = 0;
        char* token = strtok(a_str, delim);
        while (token) {
//            assert(idx < count);
            *(result + idx++) = strdup(token);
            token = strtok(0, delim);
        }
//        assert(idx == count - 1);
        *(result + idx) = 0;
    }
    return result;
}

const char* 
ACAP_DEVICE_Prop( const char *attribute ) {
	if( !ACAP_DEVICE_Container )
		return 0;
	return cJSON_GetObjectItem(ACAP_DEVICE_Container,attribute) ? cJSON_GetObjectItem(ACAP_DEVICE_Container,attribute)->valuestring : 0;
}

int
ACAP_DEVICE_Prop_Int( const char *attribute ) {
  if( !ACAP_DEVICE_Container )
    return 0;
  return cJSON_GetObjectItem(ACAP_DEVICE_Container,attribute) ? cJSON_GetObjectItem(ACAP_DEVICE_Container,attribute)->valueint : 0;
}

cJSON* 
ACAP_DEVICE_JSON( const char *attribute ) {
  if( !ACAP_DEVICE_Container )
    return 0;
  return cJSON_GetObjectItem(ACAP_DEVICE_Container,attribute);
}


int
ACAP_DEVICE_Seconds_Since_Midnight() {
	time_t t = time(NULL);
	struct tm tm = *localtime(&t);
	int seconds = tm.tm_hour * 3600;
	seconds += tm.tm_min * 60;
	seconds += tm.tm_sec;
	return seconds;
}

char ACAP_DEVICE_date[128] = "2023-01-01";
char ACAP_DEVICE_time[128] = "00:00:00";

const char*
ACAP_DEVICE_Date() {
	time_t t = time(NULL);
	struct tm *tm = localtime(&t);
	sprintf(ACAP_DEVICE_date,"%d-%02d-%02d",tm->tm_year + 1900,tm->tm_mon + 1, tm->tm_mday);
	return ACAP_DEVICE_date;
}

const char*
ACAP_DEVICE_Time() {
	time_t t = time(NULL);
	struct tm *tm = localtime(&t);
	sprintf(ACAP_DEVICE_time,"%02d:%02d:%02d",tm->tm_hour,tm->tm_min,tm->tm_sec);
	return ACAP_DEVICE_time;
}

char ACAP_DEVICE_timestring[128] = "2020-01-01 00:00:00";

const char*
ACAP_DEVICE_Local_Time() {
	time_t t = time(NULL);
	struct tm *tm = localtime(&t);
	sprintf(ACAP_DEVICE_timestring,"%d-%02d-%02d %02d:%02d:%02d",tm->tm_year + 1900,tm->tm_mon + 1, tm->tm_mday,tm->tm_hour,tm->tm_min,tm->tm_sec);
	LOG_TRACE("Local Time: %s\n",ACAP_DEVICE_timestring);
	return ACAP_DEVICE_timestring;
}

char ACAP_DEVICE_isostring[128] = "2020-01-01T00:00:00+0000";

const char*
ACAP_DEVICE_ISOTime() {
	time_t t = time(NULL);
	struct tm *tm = localtime(&t);
	strftime(ACAP_DEVICE_isostring, 50, "%Y-%m-%dT%T%z",tm);
	return ACAP_DEVICE_isostring;
}

double
ACAP_DEVICE_Timestamp(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    // Convert seconds and microseconds to milliseconds
    return tv.tv_sec * 1000 + tv.tv_usec / 1000;
}

static void
ACAP_ENDPOINT_device(ACAP_HTTP_Response response, ACAP_HTTP_Request request) {
	if( !ACAP_DEVICE_Container) {
		ACAP_HTTP_Respond_Error( response, 500, "Device properties not initialized");
		return;
	}
	cJSON_ReplaceItemInObject(ACAP_DEVICE_Container,"date", cJSON_CreateString(ACAP_DEVICE_Date()) );
	cJSON_ReplaceItemInObject(ACAP_DEVICE_Container,"time", cJSON_CreateString(ACAP_DEVICE_Time()) );
	ACAP_HTTP_Respond_JSON( response, ACAP_DEVICE_Container );
}

double previousTransmitted = 0;
double previousNetworkTimestamp = 0;
double previousNetworkAverage = 0;

double
ACAP_DEVICE_Network_Average() {
	FILE *fd;
	char   data[500] = "";
	char   readstr[500];
	char **stringArray;
	char   *subString;
	char *ptr;
	double transmitted = 0, rx = 0;
	 

	fd = fopen("/proc/net/dev","r");
	if( !fd ) {
		LOG_WARN("Error opening /proc/net/dev");
		return 0;
	}
  
	while( fgets(readstr,500,fd) ) {
		if( strstr( readstr,"eth0") != 0 )
			strcpy( data, readstr );
	}
	fclose(fd);
  
	if( strlen( data ) < 20 ) {
		LOG_WARN("Read_Ethernet_Traffic: read error");
		return 0;
	}
  
	stringArray = string_split(data, ' ');
	if( stringArray ) {
		int i;
		for (i = 0; *(stringArray + i); i++) {
			if( i == 1 ) {
				subString = *(stringArray + i);
				rx = (double)strtol( subString, &ptr, 10);
			}
			if( i == 9 ) {
				subString = *(stringArray + i);
				if(strlen(subString) > 9 )
					subString++;
				if(strlen(subString) > 9 )
					subString++;
				if(strlen(subString) > 9 )
					subString++;
				transmitted = (double)strtol( subString, &ptr, 10);
			}
			free(*(stringArray + i));
		}
		free(stringArray);
	}
	(void)rx;
	double diff = transmitted - previousTransmitted;
	previousTransmitted = transmitted;
	if( diff < 0 )
		return previousNetworkAverage;
	double timestamp = ACAP_DEVICE_Timestamp();
	double timeDiff = timestamp - previousNetworkTimestamp;
	previousNetworkTimestamp = timestamp;
	timeDiff /= 1000;
	diff *= 8;  //Make to bits;
	diff /= 1024;  //Make Kbits;
	
	previousNetworkAverage = diff / timeDiff;
	if( previousNetworkAverage < 0.001 )
		previousNetworkAverage = 0;
	return previousNetworkAverage;
}

double
ACAP_DEVICE_CPU_Average() {
	double loadavg = 0;
	struct sysinfo info;
	
	sysinfo(&info);
	loadavg = (double)info.loads[2];
	loadavg /= 65536.0;
	LOG_TRACE("%f\n",loadavg);
	return loadavg; 
}

double
ACAP_DEVICE_Uptime() {
	struct sysinfo info;
	sysinfo(&info);
	return (double)info.uptime; 
};	


cJSON*
ACAP_DEVICE() {
	gchar *value = 0, *pHead,*pTail;

	ACAP_DEVICE_Container = cJSON_CreateObject();
	ACAP_DEVICE_parameter_handler = ax_parameter_new(ACAP_package_name, 0);
	if( !ACAP_DEVICE_parameter_handler ) {
		LOG_WARN("Cannot create parameter ACAP_DEVICE_parameter_handler");
		return cJSON_CreateNull();
	}
	

	if(ax_parameter_get(ACAP_DEVICE_parameter_handler, "root.Properties.System.SerialNumber", &value, 0)) {
		cJSON_AddItemToObject(ACAP_DEVICE_Container,"serial",cJSON_CreateString(value));
		g_free(value);
	}
  
	if(ax_parameter_get(ACAP_DEVICE_parameter_handler, "root.brand.ProdShortName", &value, 0)) {
		cJSON_AddItemToObject(ACAP_DEVICE_Container,"model",cJSON_CreateString(value));
		g_free(value);
	}

	if(ax_parameter_get(ACAP_DEVICE_parameter_handler, "root.Properties.System.Architecture", &value, 0)) {
		cJSON_AddItemToObject(ACAP_DEVICE_Container,"platform",cJSON_CreateString(value));
		g_free(value);
	}

	if(ax_parameter_get(ACAP_DEVICE_parameter_handler, "root.Properties.System.Soc", &value, 0)) {
		cJSON_AddItemToObject(ACAP_DEVICE_Container,"chip",cJSON_CreateString(value));
		g_free(value);
	}

	if(ax_parameter_get(ACAP_DEVICE_parameter_handler, "root.brand.ProdType", &value, 0)) {
		cJSON_AddItemToObject(ACAP_DEVICE_Container,"type",cJSON_CreateString(value));
		g_free(value);
	}

//	if(ax_parameter_get(ACAP_DEVICE_parameter_handler, "root.Network.VolatileHostName.HostName", &value, 0)) {
//		cJSON_AddItemToObject(ACAP_DEVICE_Container,"hostname",cJSON_CreateString(value));
//		g_free(value);
//	}
  
	int aspect = 169;

	if(ax_parameter_get(ACAP_DEVICE_parameter_handler, "root.ImageSource.I0.Sensor.AspectRatio",&value,0 )) {
		cJSON_AddItemToObject(ACAP_DEVICE_Container,"aspect",cJSON_CreateString(value));
		if(strcmp(value,"4:3") == 0)
			aspect = 43;
		if(strcmp(value,"16:10") == 0)
			aspect = 1610;
		if(strcmp(value,"1:1") == 0)
			aspect = 11;
		g_free(value);
	} else {
		cJSON_AddStringToObject(ACAP_DEVICE_Container,"aspect","16:9");
	}
  
	cJSON* resolutionList = cJSON_CreateArray();
	cJSON* resolutions = cJSON_CreateObject();
	cJSON_AddItemToObject(ACAP_DEVICE_Container,"resolutions",resolutions);
	cJSON* resolutions169 = cJSON_CreateArray();
	cJSON_AddItemToObject(resolutions,"16:9",resolutions169);
	cJSON* resolutions43 = cJSON_CreateArray();
	cJSON_AddItemToObject(resolutions,"4:3",resolutions43);
	cJSON* resolutions11 = cJSON_CreateArray();
	cJSON_AddItemToObject(resolutions,"1:1",resolutions11);
	cJSON* resolutions1610 = cJSON_CreateArray();
	cJSON_AddItemToObject(resolutions,"16:10",resolutions1610);
	if(ax_parameter_get(ACAP_DEVICE_parameter_handler, "root.Properties.Image.Resolution", &value, 0)) {
		pHead = value;
		pTail = value;
		while( *pHead ) {
			if( *pHead == ',' ) {
				*pHead = 0;
				cJSON_AddItemToArray( resolutionList, cJSON_CreateString(pTail) );
				pTail = pHead + 1;
			}
			pHead++;
		}
		cJSON_AddItemToArray( resolutionList, cJSON_CreateString(pTail) );
		g_free(value);

		int length = cJSON_GetArraySize(resolutionList);
		int index;
		char data[30];
		int width = 0;
		int height = 0;
		for( index = 0; index < length; index++ ) {
			char* resolution = strcpy(data,cJSON_GetArrayItem(resolutionList,index)->valuestring);
			if( resolution ) {
				char* sX = resolution;
				char* sY = 0;
				while( *sX != 0 ) {
					if( *sX == 'x' ) {
						*sX = 0;
						sY = sX + 1;
					}
					sX++;
				}
				if( sY ) {
					int x = atoi(resolution);
					int y = atoi(sY);
					if( x && y ) {
						int a = (x*100)/y;
						if( a == 177 ) {
							cJSON_AddItemToArray(resolutions169, cJSON_CreateString(cJSON_GetArrayItem(resolutionList,index)->valuestring));
							if(aspect == 169 && x > width )
								width = x;
							if(aspect == 169 && y > height )
								height = y;
						}
						if( a == 133 ) {
							cJSON_AddItemToArray(resolutions43, cJSON_CreateString(cJSON_GetArrayItem(resolutionList,index)->valuestring));
							if(aspect == 43 && x > width )
								width = x;
							if(aspect == 43 && y > height )
								height = y;
						}
						if( a == 160 ) {
							cJSON_AddItemToArray(resolutions1610, cJSON_CreateString(cJSON_GetArrayItem(resolutionList,index)->valuestring));
							if(aspect == 1610 && x > width )
								width = x;
							if(aspect == 1610 && y > height )
								height = y;
						}
						if( a == 100 ) {
							cJSON_AddItemToArray(resolutions11, cJSON_CreateString(cJSON_GetArrayItem(resolutionList,index)->valuestring));
							if(aspect == 11 && x > width )
								width = x;
							if(aspect == 11 && y > height )
								height = y;
						}
					}
				}
			}
		}
		cJSON_AddItemToObject(ACAP_DEVICE_Container,"width",cJSON_CreateNumber(width));
		cJSON_AddItemToObject(ACAP_DEVICE_Container,"height",cJSON_CreateNumber(height));
		
		int a = (width*100)/height;
		if( a == 133 )
			cJSON_ReplaceItemInObject(ACAP_DEVICE_Container,"aspect",cJSON_CreateString("4:3") );
		if( a == 160 )
			cJSON_ReplaceItemInObject(ACAP_DEVICE_Container,"aspect",cJSON_CreateString("16:10") );
		if( a == 100 )
			cJSON_ReplaceItemInObject(ACAP_DEVICE_Container,"aspect",cJSON_CreateString("1:1") );
		cJSON_Delete(resolutionList);
	}
 
	if(ax_parameter_get(ACAP_DEVICE_parameter_handler, "root.Properties.Firmware.Version",&value,0 )) {
		cJSON_AddItemToObject(ACAP_DEVICE_Container,"firmware",cJSON_CreateString(value));
		g_free(value);
	}  

 
	ax_parameter_free(ACAP_DEVICE_parameter_handler);
	
	ACAP_HTTP_Node("device",ACAP_ENDPOINT_device);
	return ACAP_DEVICE_Container;
}

cJSON *ACAP_STATUS_Container = 0;

static void
ACAP_ENDPOINT_status(const ACAP_HTTP_Response response,const ACAP_HTTP_Request request) {
	if( ACAP_STATUS_Container == 0 ) {
		ACAP_HTTP_Respond_Error( response, 500, "Status container not initialized");
		return;
	}
	ACAP_HTTP_Respond_JSON( response, ACAP_STATUS_Container);	
}

cJSON*
ACAP_STATUS_Group( const char *group ) {
	if(!ACAP_STATUS_Container)
		ACAP_STATUS_Container = cJSON_CreateObject();
	cJSON* g = cJSON_GetObjectItem(ACAP_STATUS_Container, group );
	return g;
}

int
ACAP_STATUS_Bool( const char *group, const char *name ) {
	cJSON* g = ACAP_STATUS_Group( group );
	if(!g) {
		LOG_WARN("%s: %s is undefined\n",__func__,group);
		return 0;
	}
	cJSON *object = cJSON_GetObjectItem(g, name);
	if( !object ) {
		LOG_WARN("%s: %s/%s is undefined\n",__func__,group,name);
		return 0;
	}
	if( !( object->type == cJSON_True || object->type == cJSON_False || object->type == cJSON_NULL)) {
		LOG_WARN("%s: %s/%s is not bool (Type = %d)\n",__func__,group,name,object->type); 
		return 0;
	}
	if( object->type == cJSON_True )
		return 1;
	return 0;
}

double
ACAP_STATUS_Double( const char *group, const char *name ) {
	cJSON* g = ACAP_STATUS_Group( group );
	if(!g) {
		LOG_WARN("%s: %s is undefined\n",__func__,group);
		return 0;
	}
	cJSON *object = cJSON_GetObjectItem(g, name);
	if( !object ) {
		LOG_WARN("%s: %s/%s is undefined\n",__func__,group,name);
		return 0;
	}
	if( object->type != cJSON_Number ) {
		LOG_WARN("%s: %s/%s is not a number\n",__func__,group,name); 
		return 0;
	}
	return object->valuedouble;
}

int
ACAP_STATUS_Int( const char *group, const char *name ) {
	cJSON* g = ACAP_STATUS_Group( group );
	if(!g) {
		LOG_WARN("%s: %s is undefined\n",__func__,group);
		return 0;
	}
	cJSON *object = cJSON_GetObjectItem(g, name);
	if( !object ) {
		LOG_WARN("%s: %s/%s is undefined\n",__func__,group,name);
		return 0;
	}
	if( object->type != cJSON_Number ) {
		LOG_WARN("%s: %s/%s is not a number\n",__func__,group,name); 
		return 0;
	}
	return object->valueint;
}

char*
ACAP_STATUS_String( const char *group, const char *name ) {
	cJSON* g = ACAP_STATUS_Group(group);
	if(!g) {
		LOG_WARN("%s: %s is undefined\n",__func__,group);
		return 0;
	}
	cJSON *object = cJSON_GetObjectItem(g, name);
	if( !object ) {
		LOG_WARN("%s: %s/%s is undefined\n",__func__,group,name);
		return 0;
	}
	if( object->type != cJSON_String ) {
		LOG_WARN("%s: %s/%s is not a string\n",__func__,group,name); 
		return 0;
	}
	return object->valuestring;
}

cJSON*
ACAP_STATUS_Object( const char *group, const char *name ) {
	cJSON* g = ACAP_STATUS_Group(group);
	if(!g) {
		LOG_WARN("%s: %s is undefined\n",__func__,group);
		return 0;
	}
	cJSON *object = cJSON_GetObjectItem(g, name);
	if( !object ) {
		LOG_WARN("%s: %s/%s is undefined\n",__func__,group,name);
		return 0;
	}
	return object;
}

void
ACAP_STATUS_SetBool( const char *group, const char *name, int state ) {
	LOG_TRACE("%s: %s.%s=%s \n",__func__, group, name, state?"true":"false" );
	cJSON* g = ACAP_STATUS_Group(group);
	if(!g) {
		g = cJSON_CreateObject();
		cJSON_AddItemToObject(ACAP_STATUS_Container, group, g);
	}
	cJSON *object = cJSON_GetObjectItem( g, name);
	if( !object ) {
		if( state )
			cJSON_AddTrueToObject( g, name );
		else
			cJSON_AddFalseToObject( g, name );
		return;
	}
	if( state )
		cJSON_ReplaceItemInObject(g, name, cJSON_CreateTrue() );
	else
		cJSON_ReplaceItemInObject(g, name, cJSON_CreateFalse() );
}

void
ACAP_STATUS_SetNumber( const char *group, const char *name, double value ) {
	LOG_TRACE("%s: %s.%s=%f \n",__func__, group, name, value );
	cJSON* g = ACAP_STATUS_Group(group);
	if(!g) {
		g = cJSON_CreateObject();
		cJSON_AddItemToObject(ACAP_STATUS_Container, group, g);
	}
	cJSON *object = cJSON_GetObjectItem( g, name);
	if( !object ) {
		cJSON_AddNumberToObject( g, name, value );
		return;
	}
	cJSON_ReplaceItemInObject(g, name, cJSON_CreateNumber( value ));
}

void
ACAP_STATUS_SetString( const char *group, const char *name, const char *string ) {
	LOG_TRACE("%s: %s.%s=%s \n",__func__, group, name, string );
	cJSON* g = ACAP_STATUS_Group(group);
	if(!g) {
		g = cJSON_CreateObject();
		cJSON_AddItemToObject(ACAP_STATUS_Container, group, g);
	}
	cJSON *object = cJSON_GetObjectItem( g, name);
	if( !object ) {
		cJSON_AddStringToObject( g, name, string );
		return;
	}
	cJSON_ReplaceItemInObject(g, name, cJSON_CreateString( string ));
}

void
ACAP_STATUS_SetObject( const char *group, const char *name, cJSON* data ) {
	LOG_TRACE("%s: %s.%s\n",__func__, group, name );
	
	cJSON* g = ACAP_STATUS_Group(group);
	if(!g) {
		g = cJSON_CreateObject();
		cJSON_AddItemToObject(ACAP_STATUS_Container, group, g);
	}

	cJSON *object = cJSON_GetObjectItem( g, name);
	if( !object ) {
		cJSON_AddItemToObject( g, name, cJSON_Duplicate(data,1) );
		return;
	}
	cJSON_ReplaceItemInObject(g, name, cJSON_Duplicate(data,1));
}

void
ACAP_STATUS_SetNull( const char *group, const char *name ) {
	LOG_TRACE("%s: %s.%s=NULL\n",__func__, group, name );
	
	cJSON* g = ACAP_STATUS_Group(group);
	if(!g) {
		g = cJSON_CreateObject();
		cJSON_AddItemToObject(ACAP_STATUS_Container, group, g);
	}

	cJSON *object = cJSON_GetObjectItem( g, name);
	if( !object ) {
		cJSON_AddItemToObject( g, name, cJSON_CreateNull() );
		return;
	}
	cJSON_ReplaceItemInObject(g, name, cJSON_CreateNull());
}

cJSON*
ACAP_STATUS() {
	if( ACAP_STATUS_Container != 0 )
		return ACAP_STATUS_Container;
	LOG_TRACE("%s:\n",__func__);
	ACAP_STATUS_Container = cJSON_CreateObject();
	ACAP_HTTP_Node( "status", ACAP_ENDPOINT_status );
	return ACAP_STATUS_Container;
}

ACAP_EVENTS_Callback EVENT_USER_CALLBACK = 0;
static void ACAP_EVENTS_Main_Callback(guint subscription, AXEvent *axEvent, cJSON* event);
cJSON* ACAP_EVENTS_SUBSCRIPTIONS = 0;
cJSON* ACAP_EVENTS_DECLARATIONS = 0;
AXEventHandler *ACAP_EVENTS_HANDLER = 0;


double ioFilterTimestamp0 = 0;
double ioFilterTimestamp1 = 0;
double ioFilterTimestamp2 = 0;
double ioFilterTimestamp3 = 0;

char ACAP_EVENTS_PACKAGE[64];
char ACAP_EVENTS_APPNAME[64];


cJSON* SubscriptionsArray = 0;
typedef struct S_AXEventKeyValueSet {
	GHashTable *key_values;
} T_ValueSet;

typedef struct S_NamespaceKeyPair {
  gchar *name_space;
  gchar *key;
} T_KeyPair;

typedef struct S_ValueElement {
  gint int_value;
  gboolean bool_value;
  gdouble double_value;
  gchar *str_value;
  AXEventElementItem *elem_value;
  gchar *elem_str_value;
  GList *tags;
  gchar *key_nice_name;
  gchar *value_nice_name;

  gboolean defined;
  gboolean onvif_data;

  AXEventValueType value_type;
} T_ValueElement;


int
ACAP_EVENTS_SetCallback( ACAP_EVENTS_Callback callback ){
	LOG_TRACE("%s: Entry\n",__func__);
	EVENT_USER_CALLBACK = callback;
	return 1;
}

cJSON*
ACAP_EVENTS_Parse( AXEvent *axEvent ) {
	LOG_TRACE("%s: Entry\n",__func__);
	
	const T_ValueSet *set = (T_ValueSet *)ax_event_get_key_value_set(axEvent);
	
	cJSON *object = cJSON_CreateObject();
	GHashTableIter iter;
	T_KeyPair *nskp;
	T_ValueElement *value_element;
	char topics[6][32];
	char topic[200];
	int i;
	for(i=0;i<6;i++)
		topics[i][0]=0;
	
	g_hash_table_iter_init(&iter, set->key_values);
//	LOG_TRACE("ACAP_EVENTS_ParsePayload:\n");
	while (g_hash_table_iter_next(&iter, (gpointer*)&nskp,(gpointer*)&value_element)) {
		int isTopic = 0;
		
		if( strcmp(nskp->key,"topic0") == 0 ) {
//			LOG_TRACE("Parse: Topic 0: %s\n",(char*)value_element->str_value);
			if( strcmp((char*)value_element->str_value,"CameraApplicationPlatform") == 0 )
				sprintf(topics[0],"acap");
			else
				sprintf(topics[0],"%s",(char*)value_element->str_value);
			isTopic = 1;
		}
		if( strcmp(nskp->key,"topic1") == 0 ) {
//			LOG_TRACE("Parse: Topic 1: %s\n",(char*)value_element->str_value);
			sprintf(topics[1],"%s",value_element->str_value);
			isTopic = 1;
		}
		if( strcmp(nskp->key,"topic2") == 0 ) {
//			LOG_TRACE("Parse: Topic 2: %s\n",(char*)value_element->str_value);
			sprintf(topics[2],"%s",value_element->str_value);
			isTopic = 1;
		}
		if( strcmp(nskp->key,"topic3") == 0 ) {
//			LOG_TRACE("Parse: Topic 3: %s\n",(char*)value_element->str_value);
			sprintf(topics[3],"%s",value_element->str_value);
			isTopic = 1;
		}
		if( strcmp(nskp->key,"topic4") == 0 ) {
			sprintf(topics[4],"%s",value_element->str_value);
			isTopic = 1;
		}
		if( strcmp(nskp->key,"topic5") == 0 ) {
			sprintf(topics[5],"%s",value_element->str_value);
			isTopic = 1;
		}
		
		if( isTopic == 0 ) {
			if (value_element->defined) {
				switch (value_element->value_type) {
					case AX_VALUE_TYPE_INT:
//						LOG_TRACE("Parse: Int %s = %d\n", nskp->key, value_element->int_value);
						cJSON_AddNumberToObject(object,nskp->key,(double)value_element->int_value);
					break;

					case AX_VALUE_TYPE_BOOL:
						LOG_TRACE("Bool %s = %d\n", nskp->key, value_element->bool_value);
//						if( strcmp(nskp->key,"state") != 0 )
//							cJSON_AddNumberToObject(object,nskp->key,(double)value_element->bool_value);
//						if( !value_element->bool_value )
//							cJSON_ReplaceItemInObject(object,"state",cJSON_CreateFalse());
						cJSON_AddBoolToObject(object,nskp->key,value_element->bool_value);
					break;

					case AX_VALUE_TYPE_DOUBLE:
						LOG_TRACE("Parse: Double %s = %f\n", nskp->key, value_element->double_value);
						cJSON_AddNumberToObject(object,nskp->key,(double)value_element->double_value);
					break;

					case AX_VALUE_TYPE_STRING:
						LOG_TRACE("Parse: String %s = %s\n", nskp->key, value_element->str_value);
						cJSON_AddStringToObject(object,nskp->key, value_element->str_value);
					break;

					case AX_VALUE_TYPE_ELEMENT:
						LOG_TRACE("Parse: Element %s = %s\n", nskp->key, value_element->str_value);
						cJSON_AddStringToObject(object,nskp->key, value_element->elem_str_value);
					break;

					default:
						LOG_TRACE("Parse: Undefined %s = %s\n", nskp->key, value_element->str_value);
						cJSON_AddNullToObject(object,nskp->key);
					break;
				}
			}
		}
	}
	strcpy(topic,topics[0]);
	for(i=1;i<6;i++) {
		if( strlen(topics[i]) > 0 ) {
			strcat(topic,"/");
			strcat(topic,topics[i]);
		}
	}

	//Special Device Event Filter
	if( strcmp(topic,"Device/IO/Port") == 0 ) {
		int port = cJSON_GetObjectItem(object,"port")?cJSON_GetObjectItem(object,"port")->valueint:-1;
		if( port == -1 ) {
			cJSON_Delete(object);
			return 0;
		}
		if( port == 0 ) {
			if( ACAP_DEVICE_Timestamp() - ioFilterTimestamp0 < 1000 ) {
				cJSON_Delete(object);
				return 0;
			}
			ioFilterTimestamp0 = ACAP_DEVICE_Timestamp();
		}
		
		if( port == 1 ) {
			if( ACAP_DEVICE_Timestamp() - ioFilterTimestamp1 < 1000 ) {
				cJSON_Delete(object);
				return 0;
			}
			ioFilterTimestamp1 = ACAP_DEVICE_Timestamp();
		}
		
		if( port == 2 ) {
			if( ACAP_DEVICE_Timestamp() - ioFilterTimestamp2 < 1000 ) {
				cJSON_Delete(object);
				return 0;
			}
			ioFilterTimestamp2 = ACAP_DEVICE_Timestamp();
		}

		if( port == 3 ) {
			if( ACAP_DEVICE_Timestamp() - ioFilterTimestamp3 < 1000 ) {
				cJSON_Delete(object);
				return 0;
			}
			ioFilterTimestamp3 = ACAP_DEVICE_Timestamp();
		}
	}

	cJSON_AddStringToObject(object,"event",topic);
	return object;
}

static void
ACAP_EVENTS_Main_Callback(guint subscription, AXEvent *axEvent, cJSON* event) {
	LOG_TRACE("%s: Entry\n",__func__);

	cJSON* eventData = ACAP_EVENTS_Parse(axEvent);
	if( !eventData )
		return;
	if( EVENT_USER_CALLBACK )
		EVENT_USER_CALLBACK( eventData );
}

int
ACAP_EVENTS_Subscribe( cJSON *event ) {
	AXEventKeyValueSet *keyset = 0;	
	cJSON *topic;
	guint declarationID = 0;
	int result;
	
	if( !event ) {
		LOG_WARN("ACAP_EVENTS_Subscribe: Invalid event\n")
		return 0;
	}

	char* json = cJSON_PrintUnformatted(event);
	if( json ) {
		LOG_TRACE("%s: %s\n",__func__,json);
		free(json);
	}


	if( !cJSON_GetObjectItem( event,"topic0" ) ) {
		LOG_WARN("ACAP_EVENTS_Subscribe: Producer event has no topic0\n");
		return 0;
	}
	
	keyset = ax_event_key_value_set_new();
	if( !keyset ) {	LOG_WARN("ACAP_EVENTS_Subscribe: Unable to create keyset\n"); return 0;}


	// ----- TOPIC 0 ------
	topic = cJSON_GetObjectItem( event,"topic0" );
	if(!topic) {
		LOG_WARN("ACAP_EVENTS_Subscribe: Invalid tag for topic 0");
		return 0;
	}
	result = ax_event_key_value_set_add_key_value( keyset, "topic0", topic->child->string, topic->child->valuestring, AX_VALUE_TYPE_STRING,NULL);
	if( !result ) {
		LOG_WARN("ACAP_EVENTS_Subscribe: Topic 0 keyset error");
		ax_event_key_value_set_free(keyset);
		return 0;
	}
	LOG_TRACE("%s: TOPIC0 %s:%s\n",__func__, topic->child->string, topic->child->valuestring );
	
	// ----- TOPIC 1 ------
	if( cJSON_GetObjectItem( event,"topic1" ) ) {
		topic = cJSON_GetObjectItem( event,"topic1" );
		result = ax_event_key_value_set_add_key_value( keyset, "topic1", topic->child->string, topic->child->valuestring, AX_VALUE_TYPE_STRING,NULL);
		if( !result ) {
			LOG_WARN("ACAP_EVENTS_Subscribe: Unable to subscribe to event (1)");
			ax_event_key_value_set_free(keyset);
			return 0;
		}
		LOG_TRACE("%s: TOPIC1 %s:%s\n",__func__, topic->child->string, topic->child->valuestring );
	}
	//------ TOPIC 2 -------------
	if( cJSON_GetObjectItem( event,"topic2" ) ) {
		topic = cJSON_GetObjectItem( event,"topic2" );
		result = ax_event_key_value_set_add_key_value( keyset, "topic2", topic->child->string, topic->child->valuestring, AX_VALUE_TYPE_STRING,NULL);
		if( !result ) {
			LOG_WARN("ACAP_EVENTS_Subscribe: Unable to subscribe to event (2)");
			ax_event_key_value_set_free(keyset);
			return 0;
		}
		LOG_TRACE("%s: TOPIC2 %s:%s\n",__func__, topic->child->string, topic->child->valuestring );
	}
	
	if( cJSON_GetObjectItem( event,"topic3" ) ) {
		topic = cJSON_GetObjectItem( event,"topic3" );
		result = ax_event_key_value_set_add_key_value( keyset, "topic3", topic->child->string, topic->child->valuestring, AX_VALUE_TYPE_STRING,NULL);
		if( !result ) {
			LOG_WARN("ACAP_EVENTS_Subscribe: Unable to subscribe to event (3)");
			ax_event_key_value_set_free(keyset);
			return 0;
		}
		LOG_TRACE("%s: TOPIC3 %s %s\n",__func__, topic->child->string, topic->child->valuestring );
	}
	int ax = ax_event_handler_subscribe( ACAP_EVENTS_HANDLER, keyset, &(declarationID),(AXSubscriptionCallback)ACAP_EVENTS_Main_Callback,(gpointer)event,NULL);
	ax_event_key_value_set_free(keyset);
	if( !ax ) {
		LOG_WARN("ACAP_EVENTS_Subscribe: Unable to subscribe to event\n");
		return 0;
	}
	cJSON_AddItemToArray(ACAP_EVENTS_SUBSCRIPTIONS,cJSON_CreateNumber(declarationID));
	if( cJSON_GetObjectItem(event,"id") )
		cJSON_ReplaceItemInObject( event,"subid",cJSON_CreateNumber(declarationID));
	else
		cJSON_AddNumberToObject(event,"subid",declarationID);
	LOG_TRACE("Event subsription %d OK\n",declarationID);
	return declarationID;
}

int
ACAP_EVENTS_Unsubscribe() {
	LOG_TRACE("ACAP_EVENTS_Unsubscribe\n");
	if(!ACAP_EVENTS_SUBSCRIPTIONS)
		return 0;
	cJSON* event = ACAP_EVENTS_SUBSCRIPTIONS->child;
	while( event ) {
		ax_event_handler_unsubscribe( ACAP_EVENTS_HANDLER, (guint)event->valueint, 0);
		event = event->next;
	};
	cJSON_Delete( ACAP_EVENTS_SUBSCRIPTIONS );
	ACAP_EVENTS_SUBSCRIPTIONS = cJSON_CreateArray();
	return 1;
}


int
ACAP_EVENTS_Add_Event( const char *id, const char* name, int state ) {
	AXEventKeyValueSet *set = NULL;
	int dummy_value = 0;
	guint declarationID;

	if( !ACAP_EVENTS_HANDLER || !id || !name || !ACAP_EVENTS_DECLARATIONS) {
		LOG_WARN("ACAP_EVENTS_Add_Event: Invalid input\n");
		return 0;
	}

	LOG_TRACE("%s: Entry: ID=%s Name=%s State=%s\n",__func__,id, name, state?"Stateful":"Stateless");

	set = ax_event_key_value_set_new();
	
	ax_event_key_value_set_add_key_value( set, "topic0", "tnsaxis", "CameraApplicationPlatform", AX_VALUE_TYPE_STRING,NULL);
	ax_event_key_value_set_add_key_value( set, "topic1", "tnsaxis", ACAP_EVENTS_PACKAGE , AX_VALUE_TYPE_STRING,NULL);
	ax_event_key_value_set_add_nice_names( set, "topic1", "tnsaxis", ACAP_EVENTS_PACKAGE, ACAP_EVENTS_APPNAME, NULL);
	ax_event_key_value_set_add_key_value( set, "topic2", "tnsaxis", id, AX_VALUE_TYPE_STRING,NULL);
	ax_event_key_value_set_add_nice_names( set, "topic2", "tnsaxis", id, name, NULL);

	int ax = 0;
	if( state ) {
		ax_event_key_value_set_add_key_value(set,"state", NULL, &dummy_value, AX_VALUE_TYPE_BOOL,NULL);
		ax_event_key_value_set_mark_as_data(set, "state", NULL, NULL);
		ax = ax_event_handler_declare(ACAP_EVENTS_HANDLER, set, 0,&declarationID,NULL,NULL,NULL);
	} else {
		ax_event_key_value_set_add_key_value(set,"value", NULL, &dummy_value, AX_VALUE_TYPE_INT,NULL);
		ax_event_key_value_set_mark_as_data(set, "value", NULL, NULL);
		ax = ax_event_handler_declare(ACAP_EVENTS_HANDLER, set, 1,&declarationID,NULL,NULL,NULL);
	}

	if( !ax ) {
		LOG_WARN("Error declaring event\n");
		ax_event_key_value_set_free(set);
		return 0;
	}
	LOG_TRACE("ACAP_EVENTS_Add_Event: %s %s\n", id, name );
	cJSON_AddNumberToObject(ACAP_EVENTS_DECLARATIONS,id,declarationID);
	ax_event_key_value_set_free(set);
	return declarationID;
}	

int
ACAP_EVENTS_Remove_Event(const char *id ) {

	if( !ACAP_EVENTS_HANDLER || !id || !ACAP_EVENTS_DECLARATIONS) {
		LOG_TRACE("ACAP_EVENTS_Remove_Event: Invalid input\n");
		return 0;
	}
	
	cJSON *event = cJSON_DetachItemFromObject(ACAP_EVENTS_DECLARATIONS,id);
	if( !event ) {
		LOG_WARN("Error remving event %s.  Event not found\n",id);
		return 0;
	}

	ax_event_handler_undeclare( ACAP_EVENTS_HANDLER, event->valueint, NULL);
	cJSON_Delete( event);
	return 1;
}

int
ACAP_EVENTS_Fire_State( const char* id, int value ) {
	LOG_TRACE("%s: %s %d\n", __func__, id, value );
	
	if( !ACAP_EVENTS_DECLARATIONS || !id ) {
		LOG_WARN("EVENTs_Fire_State: Error send event\n");
		return 0;
	}

	cJSON *event = cJSON_GetObjectItem(ACAP_EVENTS_DECLARATIONS, id );
	if(!event) {
		LOG_WARN("Error sending event %s.  Event not found\n", id);
		return 0;
	}

	AXEventKeyValueSet* set = ax_event_key_value_set_new();

	if( !ax_event_key_value_set_add_key_value(set,"state",NULL , &value,AX_VALUE_TYPE_BOOL,NULL) ) {
		ax_event_key_value_set_free(set);
		LOG_WARN("EVENT_Fire_State: Could not send event %s.  Internal error\n", id);
		return 0;
	}

	AXEvent* axEvent = ax_event_new2(set, NULL);
	ax_event_key_value_set_free(set);

	if( !ax_event_handler_send_event(ACAP_EVENTS_HANDLER, event->valueint, axEvent, NULL) )  {
		LOG_WARN("EVENT_Fire_State: Could not send event %s\n", id);
		ax_event_free(axEvent);
		return 0;
	}
	ax_event_free(axEvent);
	LOG_TRACE("EVENT_Fire_State: %s %d fired\n", id,value );
	return 1;
}

int
ACAP_EVENTS_Fire( const char* id ) {
	GError *error = NULL;
	
	LOG_TRACE("%s: %s\n", __func__, id );
	
	if( !ACAP_EVENTS_DECLARATIONS || !id ) {
		LOG_WARN("EVENTs_Fire_State: Error send event\n");
		return 0;
	}

	cJSON *event = cJSON_GetObjectItem(ACAP_EVENTS_DECLARATIONS, id );
	if(!event) {
		LOG_WARN("%s: Event %s not found\n",__func__, id);
		return 0;
	}

	AXEventKeyValueSet* set = ax_event_key_value_set_new();

	guint value = 1;
	if( !ax_event_key_value_set_add_key_value(set,"value",NULL, &value,AX_VALUE_TYPE_INT,&error) ) {
		ax_event_key_value_set_free(set);
		LOG_WARN("%s: %s error %s\n", __func__,id,error->message);
		g_error_free(error);
		return 0;
	}

	AXEvent* axEvent = ax_event_new2(set,NULL);
	ax_event_key_value_set_free(set);

	if( !ax_event_handler_send_event(ACAP_EVENTS_HANDLER, event->valueint, axEvent, &error) )  {
		LOG_WARN("%s: Could not send event %s %s\n",__func__, id, error->message);
		ax_event_free(axEvent);
		g_error_free(error);
		return 0;
	}
	ax_event_free(axEvent);
	LOG_TRACE("%s: %s fired %d\n",__func__,  id,value );
	return 1;
}

int
ACAP_EVENTS_Add_Event_JSON( cJSON* event ) {
	AXEventKeyValueSet *set = NULL;
	GError *error = NULL;
//	int dummy_value = 0;
	guint declarationID;
//	char* json = 0;
	int success = 0;	
	set = ax_event_key_value_set_new();
	
	char *eventID = cJSON_GetObjectItem(event,"id")?cJSON_GetObjectItem(event,"id")->valuestring:0;
	char *eventName = cJSON_GetObjectItem(event,"name")?cJSON_GetObjectItem(event,"name")->valuestring:0;
	
	if(!eventID) {
		LOG_WARN("%s: Invalid id\n",__func__);
		return 0;
	}

	ax_event_key_value_set_add_key_value( set, "topic0", "tnsaxis", "CameraApplicationPlatform", AX_VALUE_TYPE_STRING,NULL);
	ax_event_key_value_set_add_key_value( set, "topic1", "tnsaxis", ACAP_EVENTS_PACKAGE , AX_VALUE_TYPE_STRING,NULL);
	ax_event_key_value_set_add_nice_names( set, "topic1", "tnsaxis", ACAP_EVENTS_PACKAGE, ACAP_EVENTS_APPNAME, NULL);
	ax_event_key_value_set_add_key_value( set, "topic2", "tnsaxis", eventID, AX_VALUE_TYPE_STRING,NULL);
	if( eventName )
		ax_event_key_value_set_add_nice_names( set, "topic2", "tnsaxis", eventID, eventName, NULL);

	if( cJSON_GetObjectItem(event,"show") && cJSON_GetObjectItem(event,"show")->type == cJSON_False )
		ax_event_key_value_set_mark_as_user_defined(set,eventID,"tnsaxis","isApplicationData",NULL);

	int defaultInt = 0;
	double defaultDouble = 0;
	char defaultString[] = "";
	cJSON* source = cJSON_GetObjectItem(event,"source")?cJSON_GetObjectItem(event,"source")->child:0;
	while( source ) {
		cJSON* property = source->child;
		if( property ) {
			if(strcmp(property->valuestring,"string") == 0 )
				success = ax_event_key_value_set_add_key_value(set,property->string,NULL, &defaultString,AX_VALUE_TYPE_STRING,NULL);
			if(strcmp(property->valuestring,"int") == 0 )
				success = ax_event_key_value_set_add_key_value(set,property->string,NULL, &defaultInt,AX_VALUE_TYPE_INT,NULL);
			if(strcmp(property->valuestring,"bool") == 0 )
				success = ax_event_key_value_set_add_key_value(set,property->string,NULL, &defaultInt,AX_VALUE_TYPE_BOOL,NULL);
			ax_event_key_value_set_mark_as_source(set, property->string, NULL, NULL);
			LOG_TRACE("%s: %s Source %s %s\n",__func__,eventID,property->string,property->valuestring);
		}
		source = source->next;
	}

	cJSON* data = cJSON_GetObjectItem(event,"data")?cJSON_GetObjectItem(event,"data")->child:0;
	
	int propertyCounter = 0;
	while( data ) {
		cJSON* property = data->child;
		if( property ) {
			propertyCounter++;
			if(strcmp(property->valuestring,"string") == 0 ) {
				if( !ax_event_key_value_set_add_key_value(set,property->string,NULL, &defaultString,AX_VALUE_TYPE_STRING,&error) ) {
					LOG_WARN("%s: Unable to add string %s %s\n",__func__,property->string,error->message);
					g_error_free(error);
				} else {
					LOG_TRACE("%s: Added string %s\n",__func__,property->string);
				}
			}
			if(strcmp(property->valuestring,"int") == 0 )
				ax_event_key_value_set_add_key_value(set,property->string,NULL, &defaultInt,AX_VALUE_TYPE_INT,NULL);
			if(strcmp(property->valuestring,"double") == 0 )
				ax_event_key_value_set_add_key_value(set,property->string,NULL, &defaultDouble,AX_VALUE_TYPE_DOUBLE,NULL);
			if(strcmp(property->valuestring,"bool") == 0 )
				ax_event_key_value_set_add_key_value(set,property->string,NULL, &defaultInt,AX_VALUE_TYPE_BOOL,NULL);
			ax_event_key_value_set_mark_as_data(set, property->string, NULL, NULL);
			LOG_TRACE("%s: %s Data %s %s\n",__func__,eventID,property->string,property->valuestring);
		}
		data = data->next;
	}

	if( propertyCounter == 0 )  //An empty event requires at least one property
		ax_event_key_value_set_add_key_value(set,"value",NULL, &defaultInt,AX_VALUE_TYPE_INT,NULL);
	
	if( cJSON_GetObjectItem(event,"state") && cJSON_GetObjectItem(event,"state")->type == cJSON_True ) {
		ax_event_key_value_set_add_key_value(set,"state",NULL, &defaultInt,AX_VALUE_TYPE_BOOL,NULL);
		ax_event_key_value_set_mark_as_data(set, "state", NULL, NULL);
		LOG_TRACE("%s: %s is stateful\n",__func__,eventID);
		success = ax_event_handler_declare(ACAP_EVENTS_HANDLER, set, 0,&declarationID,NULL,NULL,NULL);
	} else {
		success = ax_event_handler_declare(ACAP_EVENTS_HANDLER, set, 1,&declarationID,NULL,NULL,NULL);
	}
	LOG_TRACE("%s: %s ID = %d\n",__func__,eventID,declarationID);
	if( !success ) {
		ax_event_key_value_set_free(set);
		LOG_WARN("Unable to register event %s\n",eventID);
		return 0;
	}
	cJSON_AddNumberToObject(ACAP_EVENTS_DECLARATIONS,eventID,declarationID);
	ax_event_key_value_set_free(set);
	
/*	
	char *json;
	json = cJSON_PrintUnformatted(event);
	if( json ) {
		LOG("%s: %s\n",__func__,json);
		free(json);
	}
*/	
	return declarationID;
}

int
ACAP_EVENTS_Fire_JSON( const char* Id, cJSON* data ) {
	AXEventKeyValueSet *set = NULL;
	int boolValue;
	GError *error = NULL;
	LOG_TRACE("%s: Entry\n",__func__);
	if(!data)
		return 0;

	cJSON *event = cJSON_GetObjectItem(ACAP_EVENTS_DECLARATIONS, Id );
	if(!event) {
		LOG_WARN("%s: Error sending event %s.  Event not found\n",__func__,Id);
		return 0;
	}


	set = ax_event_key_value_set_new();
	int success = 0;
	cJSON* property = data->child;
	while(property) {
		if(property->type == cJSON_True ) {
			boolValue = 1;
			success = ax_event_key_value_set_add_key_value(set,property->string,NULL , &boolValue,AX_VALUE_TYPE_BOOL,NULL);
			LOG_TRACE("%s: Set %s %s = True\n",__func__,Id,property->string);
		}
		if(property->type == cJSON_False ) {
			boolValue = 0;
			success = ax_event_key_value_set_add_key_value(set,property->string,NULL , &boolValue,AX_VALUE_TYPE_BOOL,NULL);
			LOG_TRACE("%s: Set %s %s = False\n",__func__,Id,property->string);
		}
		if(property->type == cJSON_String ) {
			success = ax_event_key_value_set_add_key_value(set,property->string,NULL , property->valuestring,AX_VALUE_TYPE_STRING,NULL);
			LOG_TRACE("%s: Set %s %s = %s\n",__func__,Id,property->string,property->valuestring);
		}
		if(property->type == cJSON_Number ) {
			success = ax_event_key_value_set_add_key_value(set,property->string,NULL , &property->valuedouble,AX_VALUE_TYPE_DOUBLE,NULL);
			LOG_TRACE("%s: Set %s %s = %f\n",__func__,Id,property->string,property->valuedouble);
		}
		if(!success)
			LOG_WARN("%s: Unable to add property\n",__func__);
		property = property->next;
	}

	AXEvent* axEvent = ax_event_new2(set, NULL);

	ax_event_key_value_set_free(set);

	success = ax_event_handler_send_event(ACAP_EVENTS_HANDLER, event->valueint, axEvent, &error);
	if(!success)  {
		LOG_WARN("%s: Could not send event %s id = %d %s\n",__func__, Id, event->valueint, error->message);
		ax_event_free(axEvent);
		g_error_free(error);
		return 0;
	}
	ax_event_free(axEvent);
	return 1;
}

cJSON*
ACAP_EVENTS() {
	LOG_TRACE("%s:\n",__func__);
	//Get the ACAP package ID and Nice Name
	cJSON* manifest = ACAP_Service("manifest");
	if(!manifest)
		return cJSON_CreateNull();
	cJSON* acapPackageConf = cJSON_GetObjectItem(manifest,"acapPackageConf");
	if(!acapPackageConf)
		return cJSON_CreateNull();
	cJSON* setup = cJSON_GetObjectItem(acapPackageConf,"setup");
	if(!setup)
		return cJSON_CreateNull();
	sprintf(ACAP_EVENTS_PACKAGE,"%s", cJSON_GetObjectItem(setup,"appName")->valuestring);
	sprintf(ACAP_EVENTS_APPNAME,"%s", cJSON_GetObjectItem(setup,"friendlyName")->valuestring);
	
	LOG_TRACE("%s: %s %s\n",__func__,ACAP_EVENTS_PACKAGE,ACAP_EVENTS_APPNAME);

	ACAP_EVENTS_SUBSCRIPTIONS = cJSON_CreateArray();
	ACAP_EVENTS_DECLARATIONS = cJSON_CreateObject();
	ACAP_EVENTS_HANDLER = ax_event_handler_new();
	
	cJSON* events = ACAP_FILE_Read( "html/config/events.json" );
	if(!events)
		LOG_WARN("Cannot load even event list\n")
	cJSON* event = events?events->child:0;
	while( event ) {
		ACAP_EVENTS_Add_Event_JSON( event );
		event = event->next;
	}
	return events;
}
