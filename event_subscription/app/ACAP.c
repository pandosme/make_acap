/*
 * Copyright (c) 2024 Fred Juhlin
 * MIT License - See LICENSE file for details
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <syslog.h>
#include <string.h>
#include <glib.h>
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

// Logging macros
#define LOG(fmt, args...) { syslog(LOG_INFO, fmt, ## args); printf(fmt, ## args); }
#define LOG_WARN(fmt, args...) { syslog(LOG_WARNING, fmt, ## args); printf(fmt, ## args); }
//#define LOG_TRACE(fmt, args...) { syslog(LOG_INFO, fmt, ## args); printf(fmt, ## args); }
#define LOG_TRACE(fmt, args...) {}

// Global variables
static cJSON* app = NULL;
static cJSON* status_container = NULL;

static char ACAP_package_name[ACAP_MAX_PACKAGE_NAME];
static ACAP_Config_Update ACAP_UpdateCallback = NULL;


/*-----------------------------------------------------
 * Core Functions Implementation
 *-----------------------------------------------------*/
gboolean
ACAP_Process(gpointer user_data) {
	(void)user_data;
    ACAP_HTTP_Process();
//    ACAP_TIMER_Process();
	return G_SOURCE_CONTINUE;
}

static void
ACAP_ENDPOINT_app(const ACAP_HTTP_Response response, const ACAP_HTTP_Request request) {
    // Check request method
    const char* method = ACAP_HTTP_Get_Method(request);
    
    if (method && strcmp(method, "POST") == 0) {
        // Return 405 Method Not Allowed for POST requests
        ACAP_HTTP_Respond_Error(response, 405, "Method Not Allowed - Use GET");
        return;
    }
    ACAP_HTTP_Respond_JSON(response, app);
}

static void
ACAP_ENDPOINT_settings(const ACAP_HTTP_Response response, const ACAP_HTTP_Request request) {
    const char* method = ACAP_HTTP_Get_Method(request);

    if (!method) {
        ACAP_HTTP_Respond_Error(response, 400, "Invalid Request Method");
        return;
    }

    // Handle GET request - return current settings
    if (strcmp(method, "GET") == 0) {
        ACAP_HTTP_Respond_JSON(response, cJSON_GetObjectItem(app, "settings"));
        return;
    }

    // Handle POST request - update settings
    if (strcmp(method, "POST") == 0) {
        // Verify content type
        const char* contentType = ACAP_HTTP_Get_Content_Type(request);
        if (!contentType || strcmp(contentType, "application/json") != 0) {
            ACAP_HTTP_Respond_Error(response, 415, "Unsupported Media Type - Use application/json");
            return;
        }

        // Check for POST data
        if (!request->postData || request->postDataLength == 0) {
            ACAP_HTTP_Respond_Error(response, 400, "Missing POST data");
            return;
        }

        // Parse POST data
        cJSON* params = cJSON_Parse(request->postData);
        if (!params) {
            ACAP_HTTP_Respond_Error(response, 400, "Invalid JSON data");
            return;
        }

        LOG_TRACE("%s: %s\n", __func__, request->postData);

        // Update settings
        cJSON* settings = cJSON_GetObjectItem(app, "settings");
        cJSON* param = params->child;
        while (param) {
            if (cJSON_GetObjectItem(settings, param->string)) {
                cJSON_ReplaceItemInObject(settings, param->string, 
                                        cJSON_Duplicate(param, 1));
            }
            param = param->next;
        }

        // Cleanup and save
        cJSON_Delete(params);
        ACAP_FILE_Write("localdata/settings.json", settings);

        // Notify about update
        if (ACAP_UpdateCallback) {
            ACAP_UpdateCallback("settings", settings);
        }

        ACAP_HTTP_Respond_Text(response, "Settings updated successfully");
        return;
    }

    // Handle unsupported methods
    ACAP_HTTP_Respond_Error(response, 405, "Method Not Allowed - Use GET or POST");
}

const char* ACAP_Name(void) {
	return ACAP_package_name;
}

 
cJSON* ACAP(const char* package, ACAP_Config_Update callback) {
    if (!package) {
        LOG_WARN("Invalid package name\n");
        return NULL;
    }

    
    LOG_TRACE("%s: Initializing ACAP for package %s\n", __func__, package);
    
    // Store package name
    strncpy(ACAP_package_name, package, ACAP_MAX_PACKAGE_NAME - 1);
    ACAP_package_name[ACAP_MAX_PACKAGE_NAME - 1] = '\0';
    
    // Initialize subsystems
    if (!ACAP_FILE_Init()) {
        LOG_WARN("Failed to initialize file system\n");
        return NULL;
    }

    // Store callback
    ACAP_UpdateCallback = callback;

    // Create main app object
    app = cJSON_CreateObject();
    if (!app) {
        LOG_WARN("Failed to create app object\n");
        return NULL;
    }

    // Load manifest
    cJSON* manifest = ACAP_FILE_Read("manifest.json");
    if (manifest) {
        cJSON_AddItemToObject(app, "manifest", manifest);
    }

    // Load and merge settings
    cJSON* settings = ACAP_FILE_Read("html/config/settings.json");
    if (!settings) {
        settings = cJSON_CreateObject();
    }
    
    cJSON* savedSettings = ACAP_FILE_Read("localdata/settings.json");
    if (savedSettings) {
        cJSON* prop = savedSettings->child;
        while (prop) {
            if (cJSON_GetObjectItem(settings, prop->string)) {
                cJSON_ReplaceItemInObject(settings, prop->string, 
                                        cJSON_Duplicate(prop, 1));
            }
            prop = prop->next;
        }
        cJSON_Delete(savedSettings);
    }

    cJSON_AddItemToObject(app, "settings", settings);

    // Initialize subsystems
    ACAP_HTTP();
    ACAP_EVENTS();
    
    // Register core services
    ACAP_Set_Config("status", ACAP_STATUS());
    ACAP_Set_Config("device", ACAP_DEVICE());
    
    // Register core endpoints
    ACAP_HTTP_Node("app", ACAP_ENDPOINT_app);
    ACAP_HTTP_Node("settings", ACAP_ENDPOINT_settings);

    // Notify about settings
    if (ACAP_UpdateCallback) {
        ACAP_UpdateCallback("settings", settings);
    }

    LOG_TRACE("%s: Initialization complete\n", __func__);
    return settings;
}

int
ACAP_Set_Config(const char* service, cJSON* serviceSettings ) {
	LOG_TRACE("%s: %s\n",__func__,service);
	if( cJSON_GetObjectItem(app,service) ) {
		LOG_TRACE("%s: %s already registered\n",__func__,service);
		return 1;
	}
	cJSON_AddItemToObject( app, service, serviceSettings );
	return 1;
}

cJSON*
ACAP_Get_Config(const char* service) {
	cJSON* reqestedService = cJSON_GetObjectItem(app, service );
	if( !reqestedService ) {
		LOG_WARN("%s: %s is undefined\n",__func__,service);
		return 0;
	}
	return reqestedService;
}

/*------------------------------------------------------------------
 * HTTP Request Processing Implementation
 *------------------------------------------------------------------*/

typedef struct {
    char path[ACAP_MAX_PATH_LENGTH];
    ACAP_HTTP_Callback callback;
} HTTPNode;

static int initialized = 0;
static int fcgi_sock = -1;
static HTTPNode http_nodes[ACAP_MAX_HTTP_NODES];
static int http_node_count = 0;


static const char* get_path_without_query(const char* uri) {
    static char path[ACAP_MAX_PATH_LENGTH];
    const char* query = strchr(uri, '?');
    
    if (query) {
        size_t path_length = query - uri;
        if (path_length >= ACAP_MAX_PATH_LENGTH) {
            path_length = ACAP_MAX_PATH_LENGTH - 1;
        }
        strncpy(path, uri, path_length);
        path[path_length] = '\0';
        return path;
    }
    return uri;
}

int
ACAP_HTTP() {
    if (!initialized) {
        if (FCGX_Init() != 0) {
            LOG_WARN("Failed to initialize FCGI\n");
            return 0;
        }
        initialized = 1;
    }
	return 1;
}

void ACAP_HTTP_Cleanup() {
	LOG_TRACE("%s:",__func__);
    if (fcgi_sock != -1) {
        close(fcgi_sock);
        fcgi_sock = -1;
    }
    initialized = 0;
}

const char* ACAP_HTTP_Get_Method(const ACAP_HTTP_Request request) {
    if (!request || !request->request) {
        return NULL;
    }
    return FCGX_GetParam("REQUEST_METHOD", request->request->envp);
}

const char* ACAP_HTTP_Get_Content_Type(const ACAP_HTTP_Request request) {
    if (!request || !request->request) {
        return NULL;
    }
    return FCGX_GetParam("CONTENT_TYPE", request->request->envp);
}

size_t ACAP_HTTP_Get_Content_Length(const ACAP_HTTP_Request request) {
    if (!request || !request->request) {
        return 0;
    }
    const char* contentLength = FCGX_GetParam("CONTENT_LENGTH", request->request->envp);
    return contentLength ? (size_t)atoll(contentLength) : 0;
}

int ACAP_HTTP_Node(const char *nodename, ACAP_HTTP_Callback callback) {
    // Prevent buffer overflow
    if (http_node_count >= ACAP_MAX_HTTP_NODES) {
        LOG_WARN("Maximum HTTP nodes reached");
        return 0;
    }
	LOG_TRACE("%s: %s\n",__func__,nodename);
	
    // Construct full path
    char full_path[ACAP_MAX_PATH_LENGTH];
    if (nodename[0] == '/') {
        snprintf(full_path, ACAP_MAX_PATH_LENGTH, "/local/%s%s", ACAP_Name(), nodename);
    } else {
        snprintf(full_path, ACAP_MAX_PATH_LENGTH, "/local/%s/%s", ACAP_Name(), nodename);
    }

    // Check for duplicate paths
    for (int i = 0; i < http_node_count; i++) {
        if (strcmp(http_nodes[i].path, full_path) == 0) {
            LOG_WARN("Duplicate HTTP node path: %s", full_path);
            return 0;
        }
    }

    // Add new node
	snprintf(http_nodes[http_node_count].path, ACAP_MAX_PATH_LENGTH, "%s", full_path);
	http_nodes[http_node_count].callback = callback;
	http_node_count++;

    return 1;
}


void ACAP_HTTP_Process() {
	FCGX_Request request;
    ACAP_HTTP_Request_DATA requestData = {0};
    char* socket_path = NULL;

    if (!initialized)
		return;

    // Get socket path
    socket_path = getenv("FCGI_SOCKET_NAME");
    if (!socket_path) {
        LOG_WARN("Failed to get FCGI_SOCKET_NAME\n");
        return;
    }

    // Open socket if not already open
    if (fcgi_sock == -1) {
        fcgi_sock = FCGX_OpenSocket(socket_path, 5);
        if (fcgi_sock < 0) {
            LOG_WARN("Failed to open FCGI socket\n");
            return;
        }
        chmod(socket_path, 0777);
    }

    // Initialize request
    if (FCGX_InitRequest(&request, fcgi_sock, 0) != 0) {
        LOG_WARN("FCGX_InitRequest failed\n");
        return;
    }

    // Accept the request
    if (FCGX_Accept_r(&request) != 0) {
        FCGX_Free(&request, 1);
        return;
    }

    // Setup request data structure
    requestData.request = &request;
    requestData.method = FCGX_GetParam("REQUEST_METHOD", request.envp);
    requestData.contentType = FCGX_GetParam("CONTENT_TYPE", request.envp);
    
    // Handle POST data
    if (requestData.method && strcmp(requestData.method, "POST") == 0) {
        size_t contentLength = ACAP_HTTP_Get_Content_Length(&requestData);
        if (contentLength > 0 && contentLength < ACAP_MAX_BUFFER_SIZE) {
            char* postData = malloc(contentLength + 1);
				if (postData) {
					size_t bytesRead = FCGX_GetStr(postData, contentLength, request.in);
					if (bytesRead < contentLength) {
						free(postData);
						goto cleanup;
					}
					postData[bytesRead] = '\0';
					requestData.postData = postData;
					requestData.postDataLength = bytesRead;
				}
        }
    }

    // Process the request
    const char* uriString = FCGX_GetParam("REQUEST_URI", request.envp);
    if (!uriString) {
        ACAP_HTTP_Respond_Error(&request, 400, "Invalid URI");
        goto cleanup;
    }

    //LOG_TRACE("%s: Processing URI: %s\n", __func__, uriString);

    // Find and execute matching callback
    const char* pathOnly = get_path_without_query(uriString);
    ACAP_HTTP_Callback matching_callback = NULL;

    for (int i = 0; i < http_node_count; i++) {
        if (strcmp(http_nodes[i].path, pathOnly) == 0) {
            matching_callback = http_nodes[i].callback;
            break;
        }
    }

    if (matching_callback) {
        matching_callback(&request, &requestData);
    } else {
        ACAP_HTTP_Respond_Error(&request, 404, "Not Found");
    }

cleanup:
    if (requestData.postData) {
        free((void*)requestData.postData);
    }
    FCGX_Finish_r(&request);
    return;
}

/*------------------------------------------------------------------
 * HTTP Request Parameter Handling Implementation
 *------------------------------------------------------------------*/

const char* ACAP_HTTP_Request_Param(const ACAP_HTTP_Request request, const char* name) {
    if (!request || !request->request || !name) {
        LOG_WARN("Invalid request parameters\n");
        return NULL;
    }

    // First check POST data for parameters
    if (request->method && strcmp(request->method, "POST") == 0 && 
        request->contentType && strstr(request->contentType, "application/x-www-form-urlencoded")) {
        
        if (request->postData) {
            char search_param[512];
            snprintf(search_param, sizeof(search_param), "%s=", name);
            char* found = strstr(request->postData, search_param);
            if (found) {
                found += strlen(search_param);
                char* end = strchr(found, '&');
                size_t len = end ? (size_t)(end - found) : strlen(found);
                
                // Allocate memory for the result
                char* result = (char*)malloc(len + 1);
                if (!result) {
                    return NULL;
                }
                
                strncpy(result, found, len);
                result[len] = '\0';
                return result;  // Caller must free this
            }
        }
    }

    // Then check query string
    const char* query_string = FCGX_GetParam("QUERY_STRING", request->request->envp);
    if (query_string) {
        char search_param[512];
        snprintf(search_param, sizeof(search_param), "%s=", name);
        
        const char* start = query_string;
        while ((start = strstr(start, search_param)) != NULL) {
            // Make sure we found the parameter at the start of the string or after an &
            if (start != query_string && *(start-1) != '&') {
                start++;
                continue;
            }
            
            start += strlen(search_param);
            char* end = strchr(start, '&');
            size_t len = end ? (size_t)(end - start) : strlen(start);
            
            // Allocate memory for the result
            char* result = (char*)malloc(len + 1);
            if (!result) {
                return NULL;
            }
            
            strncpy(result, start, len);
            result[len] = '\0';
            return result;  // Caller must free this
        }
    }

    return NULL;
}


/*------------------------------------------------------------------
 * HTTP Response Implementation
 *------------------------------------------------------------------*/

int ACAP_HTTP_Header_XML(ACAP_HTTP_Response response) {
    return ACAP_HTTP_Respond_String(response, 
        "Content-Type: text/xml; charset=utf-8\r\n"
        "Cache-Control: no-cache\r\n\r\n");
}

int ACAP_HTTP_Header_JSON(ACAP_HTTP_Response response) {
    return ACAP_HTTP_Respond_String(response, 
        "Content-Type: application/json; charset=utf-8\r\n"
        "Cache-Control: no-cache\r\n\r\n");
}

int ACAP_HTTP_Header_TEXT(ACAP_HTTP_Response response) {
    return ACAP_HTTP_Respond_String(response, 
        "Content-Type: text/plain; charset=utf-8\r\n"
        "Cache-Control: no-cache\r\n\r\n");
}

int ACAP_HTTP_Header_FILE(ACAP_HTTP_Response response, const char* filename, 
                         const char* contenttype, unsigned filelength) {
    if (!response || !filename || !contenttype) {
        return 0;
    }

    return ACAP_HTTP_Respond_String(response,
        "Content-Type: %s\r\n"
        "Content-Disposition: attachment; filename=%s\r\n"
        "Content-Transfer-Encoding: binary\r\n"
        "Content-Length: %u\r\n"
        "\r\n",
        contenttype, filename, filelength);
}

int ACAP_HTTP_Respond_String(ACAP_HTTP_Response response, const char *fmt, ...) {
    if (!response || !response->out || !fmt) {
        return 0;
    }

    char buffer[4096];
    va_list args;
    va_start(args, fmt);
    int written = vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);
    
    if (written < 0 || written >= (int)sizeof(buffer)) {
        return 0;
    }
    
    return FCGX_PutStr(buffer, written, response->out) == written;
}

int ACAP_HTTP_Respond_JSON(ACAP_HTTP_Response response, cJSON* object) {
    if (!response || !object) {
        LOG_WARN("Invalid response handle or JSON object\n");
        return 0;
    }

    char* jsonString = cJSON_Print(object);
    if (!jsonString) {
        LOG_WARN("Failed to serialize JSON\n");
        return 0;
    }

    int result = ACAP_HTTP_Header_JSON(response) &&
                 ACAP_HTTP_Respond_String(response, "%s", jsonString);
    
    free(jsonString);
    return result;
}

int ACAP_HTTP_Respond_Data(ACAP_HTTP_Response response, size_t count, const void* data) {
    if (!response || !response->out || !data || count == 0) {
        LOG_WARN("Invalid response parameters\n");
        return 0;
    }

    return FCGX_PutStr(data, count, response->out) == (int)count;
}

int ACAP_HTTP_Respond_Error(ACAP_HTTP_Response response, int code, const char* message) {
    if (!response || !message) {
        return 0;
    }

    const char* error_type = (code < 500) ? "Client" : 
                            (code < 600) ? "Server" : "Unknown";

    ACAP_HTTP_Respond_String(response, 
        "Status: %d %s Error\r\n"
        "Content-Type: text/plain\r\n"
        "\r\n"
        "%s", 
        code, error_type, message);

    LOG_WARN("HTTP Error %d: %s\n", code, message);
    return 1;
}

int ACAP_HTTP_Respond_Text(ACAP_HTTP_Response response, const char* message) {
    if (!response || !message) {
        return 0;
    }

    return ACAP_HTTP_Header_TEXT(response) &&
           ACAP_HTTP_Respond_String(response, "%s", message);
}

/*------------------------------------------------------------------
 * Status Management Implementation
 *------------------------------------------------------------------*/

static void
ACAP_ENDPOINT_status(const ACAP_HTTP_Response response, const ACAP_HTTP_Request request) {
    const char* method = ACAP_HTTP_Get_Method(request);
    
    if (!method || strcmp(method, "GET") != 0) {
        ACAP_HTTP_Respond_Error(response, 405, "Method Not Allowed - Only GET supported");
        return;
    }

	if(!status_container)
		status_container = cJSON_CreateObject();
    
    ACAP_HTTP_Respond_JSON(response, status_container);
}

cJSON* ACAP_STATUS(void) {
    if (!status_container) {
        status_container = cJSON_CreateObject();
		ACAP_HTTP_Node("status",ACAP_ENDPOINT_status);
	}
    return status_container;
}

cJSON* ACAP_STATUS_Group(const char* name) {
    if (!name || !status_container) {
        return NULL;
    }

    cJSON* group = cJSON_GetObjectItem(status_container, name);
    if (!group) {
        group = cJSON_CreateObject();
        if (!group) {
            LOG_WARN("Failed to create status group: %s\n", name);
            return NULL;
        }
        cJSON_AddItemToObject(status_container, name, group);
    }
    return group;
}


void ACAP_STATUS_SetBool(const char* group, const char* name, int state) {
    if (!group || !name) {
        LOG_WARN("%s: Invalid group or name parameter\n", __func__);
        return;
    }

    cJSON* groupObj = ACAP_STATUS_Group(group);
    if (!groupObj) {
		LOG_TRACE("%s: Unknown %s\n",__func__,group);
        return;
    }

    cJSON* item = cJSON_GetObjectItem(groupObj, name);
    if (item) {
        cJSON_ReplaceItemInObject(groupObj, name, cJSON_CreateBool(state));
    } else {
        cJSON_AddItemToObject(groupObj, name, cJSON_CreateBool(state));
    }
}

void ACAP_STATUS_SetNumber(const char* group, const char* name, double value) {
    if (!group || !name) {
        LOG_WARN("Invalid group or name parameter\n");
        return;
    }

    cJSON* groupObj = ACAP_STATUS_Group(group);
    if (!groupObj) {
        return;
    }

    cJSON* item = cJSON_GetObjectItem(groupObj, name);
    if (item) {
        cJSON_ReplaceItemInObject(groupObj, name, cJSON_CreateNumber(value));
    } else {
        cJSON_AddItemToObject(groupObj, name, cJSON_CreateNumber(value));
    }
}

void ACAP_STATUS_SetString(const char* group, const char* name, const char* string) {
    if (!group || !name || !string) {
        LOG_WARN("Invalid parameters\n");
        return;
    }

    cJSON* groupObj = ACAP_STATUS_Group(group);
    if (!groupObj) {
        return;
    }

    cJSON* item = cJSON_GetObjectItem(groupObj, name);
    if (item) {
        cJSON_ReplaceItemInObject(groupObj, name, cJSON_CreateString(string));
    } else {
        cJSON_AddItemToObject(groupObj, name, cJSON_CreateString(string));
    }
}

void ACAP_STATUS_SetObject(const char* group, const char* name, cJSON* data) {
    if (!group || !name || !data) {
        LOG_WARN("Invalid parameters\n");
        return;
    }

    cJSON* groupObj = ACAP_STATUS_Group(group);
    if (!groupObj) {
        return;
    }

    cJSON* item = cJSON_GetObjectItem(groupObj, name);
    if (item) {
        cJSON_ReplaceItemInObject(groupObj, name, cJSON_Duplicate(data, 1));
    } else {
        cJSON_AddItemToObject(groupObj, name, cJSON_Duplicate(data, 1));
    }
}

void ACAP_STATUS_SetNull(const char* group, const char* name) {
    if (!group || !name) {
        LOG_WARN("Invalid group or name parameter\n");
        return;
    }

    cJSON* groupObj = ACAP_STATUS_Group(group);
    if (!groupObj) {
        return;
    }

    cJSON* item = cJSON_GetObjectItem(groupObj, name);
    if (item) {
        cJSON_ReplaceItemInObject(groupObj, name, cJSON_CreateNull());
    } else {
        cJSON_AddItemToObject(groupObj, name, cJSON_CreateNull());
    }
}

/*------------------------------------------------------------------
 * Status Getters Implementation
 *------------------------------------------------------------------*/

int ACAP_STATUS_Bool(const char* group, const char* name) {
    cJSON* groupObj = ACAP_STATUS_Group(group);
    if (!groupObj) {
		LOG_TRACE("%s: Invalid group \n",__func__);
        return 0;
    }

    cJSON* item = cJSON_GetObjectItem(groupObj, name);
	if( !item ) {
		return 0;
	}
    return item->type == cJSON_True?1:0;
}

int ACAP_STATUS_Int(const char* group, const char* name) {
    cJSON* groupObj = ACAP_STATUS_Group(group);
    if (!groupObj) {
        return 0;
    }

    cJSON* item = cJSON_GetObjectItem(groupObj, name);
    return item && cJSON_IsNumber(item) ? item->valueint : 0;
}

double ACAP_STATUS_Double(const char* group, const char* name) {
    cJSON* groupObj = ACAP_STATUS_Group(group);
    if (!groupObj) {
        return 0.0;
    }

    cJSON* item = cJSON_GetObjectItem(groupObj, name);
    return item && cJSON_IsNumber(item) ? item->valuedouble : 0.0;
}

char* ACAP_STATUS_String(const char* group, const char* name) {
    cJSON* groupObj = ACAP_STATUS_Group(group);
    if (!groupObj) {
        return NULL;
    }

    cJSON* item = cJSON_GetObjectItem(groupObj, name);
    return item && cJSON_IsString(item) ? item->valuestring : NULL;
}

cJSON* ACAP_STATUS_Object(const char* group, const char* name) {
    cJSON* groupObj = ACAP_STATUS_Group(group);
    if (!groupObj) {
        return NULL;
    }

    return cJSON_GetObjectItem(groupObj, name);
}

/*------------------------------------------------------------------
 * Device Information Implementation
 *------------------------------------------------------------------*/

static cJSON* ACAP_DEVICE_Container = NULL;
static AXParameter* ACAP_DEVICE_parameter_handler = NULL;

static void
ACAP_ENDPOINT_device(ACAP_HTTP_Response response, ACAP_HTTP_Request request) {
    const char* method = ACAP_HTTP_Get_Method(request);
    
    // Verify method is GET
    if (!method || strcmp(method, "GET") != 0) {
        ACAP_HTTP_Respond_Error(response, 405, "Method Not Allowed - Only GET supported");
        return;
    }

    // Check if device container is initialized
    if (!ACAP_DEVICE_Container) {
        ACAP_HTTP_Respond_Error(response, 500, "Device properties not initialized");
        return;
    }

    // Update dynamic properties
    cJSON_ReplaceItemInObject(ACAP_DEVICE_Container, "date", 
                             cJSON_CreateString(ACAP_DEVICE_Date()));
    cJSON_ReplaceItemInObject(ACAP_DEVICE_Container, "time", 
                             cJSON_CreateString(ACAP_DEVICE_Time()));

    // Send response
    ACAP_HTTP_Respond_JSON(response, ACAP_DEVICE_Container);
}


double previousTransmitted = 0;
double previousNetworkTimestamp = 0;
double previousNetworkAverage = 0;
char** string_split( char* a_str,  char a_delim);

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


cJSON* ACAP_DEVICE(void) {
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


/*------------------------------------------------------------------
 * Time and System Information
 *------------------------------------------------------------------*/

int
ACAP_DEVICE_Seconds_Since_Midnight() {
	time_t t = time(NULL);
	struct tm tm = *localtime(&t);
	int seconds = tm.tm_hour * 3600;
	seconds += tm.tm_min * 60;
	seconds += tm.tm_sec;
	return seconds;
}

char ACAP_DEVICE_timestring[128] = "2020-01-01 00:00:00";
char ACAP_DEVICE_date[128] = "2023-01-01";
char ACAP_DEVICE_time[128] = "00:00:00";
char ACAP_DEVICE_isostring[128] = "2020-01-01T00:00:00+0000";

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


const char*
ACAP_DEVICE_Local_Time() {
	time_t t = time(NULL);
	struct tm *tm = localtime(&t);
	sprintf(ACAP_DEVICE_timestring,"%d-%02d-%02d %02d:%02d:%02d",tm->tm_year + 1900,tm->tm_mon + 1, tm->tm_mday,tm->tm_hour,tm->tm_min,tm->tm_sec);
	LOG_TRACE("Local Time: %s\n",ACAP_DEVICE_timestring);
	return ACAP_DEVICE_timestring;
}



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

/*------------------------------------------------------------------
 * File Operations Implementation
 *------------------------------------------------------------------*/

static char ACAP_FILE_Path[ACAP_MAX_PATH_LENGTH] = "";

const char* ACAP_FILE_AppPath(void) {
    return ACAP_FILE_Path;
}

int ACAP_FILE_Init(void) {
    if (!ACAP_package_name[0]) {
        LOG_WARN("Package name not initialized\n");
        return 0;
    }

    snprintf(ACAP_FILE_Path, sizeof(ACAP_FILE_Path), 
             "/usr/local/packages/%s/", ACAP_package_name);
    return 1;
}

FILE* ACAP_FILE_Open(const char* filepath, const char* mode) {
    if (!filepath || !mode) {
        LOG_WARN("Invalid parameters for file operation\n");
        return NULL;
    }

    char fullpath[ACAP_MAX_PATH_LENGTH];
    if (snprintf(fullpath, sizeof(fullpath), "%s%s", ACAP_FILE_Path, filepath) >= sizeof(fullpath)) {
        LOG_WARN("Path too long\n");
        return NULL;
    }
    
    FILE* file = fopen(fullpath, mode);
    if (!file) {
        LOG_TRACE("%s: Opening file %s failed\n", __func__, fullpath);
    }
    return file;
}

int ACAP_FILE_Delete(const char* filepath) {
    if (!filepath) {
        return 0;
    }

    char fullpath[ACAP_MAX_PATH_LENGTH];
    snprintf(fullpath, sizeof(fullpath), "%s%s", ACAP_FILE_Path, filepath);
    
    if (remove(fullpath) != 0) {
        LOG_WARN("Delete %s failed\n", fullpath);
        return 0;
    }
    return 1;
}

cJSON* ACAP_FILE_Read(const char* filepath) {
    if (!filepath) {
        LOG_WARN("Invalid filepath\n");
        return NULL;
    }

    FILE* file = ACAP_FILE_Open(filepath, "r");
    if (!file) {
        return NULL;
    }

    // Get file size
    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    fseek(file, 0, SEEK_SET);

    if (size < 2) {
        LOG_WARN("File size error in %s\n", filepath);
        fclose(file);
        return NULL;
    }

    // Allocate memory and read file
    char* jsonString = malloc(size + 1);
    if (!jsonString) {
        LOG_WARN("Memory allocation error\n");
        fclose(file);
        return NULL;
    }

    size_t bytesRead = fread(jsonString, 1, size, file);
    fclose(file);

    if (bytesRead != (size_t)size) {
        LOG_WARN("Read error in %s\n", filepath);
        free(jsonString);
        return NULL;
    }

    jsonString[bytesRead] = '\0';
    cJSON* object = cJSON_Parse(jsonString);
    free(jsonString);

    if (!object) {
        LOG_WARN("JSON Parse error for %s\n", filepath);
        return NULL;
    }

    return object;
}

int ACAP_FILE_Write(const char* filepath, cJSON* object) {
    if (!filepath || !object) {
        LOG_WARN("Invalid parameters for file write\n");
        return 0;
    }

    FILE* file = ACAP_FILE_Open(filepath, "w");
    if (!file) {
        LOG_WARN("Error opening %s for writing\n", filepath);
        return 0;
    }

    char* jsonString = cJSON_Print(object);
    if (!jsonString) {
        LOG_WARN("JSON serialization error for %s\n", filepath);
        fclose(file);
        return 0;
    }

    int result = fputs(jsonString, file);
    free(jsonString);
    fclose(file);

    if (result < 0) {
        LOG_WARN("Could not save data to %s\n", filepath);
        return 0;
    }

    return 1;
}

/*------------------------------------------------------------------
 * Events System Implementation
 *------------------------------------------------------------------*/

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


cJSON*
ACAP_EVENTS() {
	LOG_TRACE("%s:\n",__func__);
	//Get the ACAP package ID and Nice Name
	cJSON* manifest = ACAP_Get_Config("manifest");
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
	LOG_TRACE("%s: %s %s %s\n",__func__,id, name, state?"Stateful":"Stateless");

	cJSON_AddNumberToObject(ACAP_EVENTS_DECLARATIONS,id,declarationID);
	ax_event_key_value_set_free(set);
	return declarationID;
}	

int
ACAP_EVENTS_Remove_Event(const char *id ) {
	LOG_TRACE("%s: %s",__func__,id);
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
ACAP_EVENTS_Fire_State( const char* id, int value ) {
	if( value && ACAP_STATUS_Bool("events",id) )
		return 1;  //The state is already high
	if( !value && !ACAP_STATUS_Bool("events",id) )
		return 1;  //The state is already low

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
	ACAP_STATUS_SetBool("events",id,value);
	LOG_TRACE("EVENT_Fire_State: %s %d fired\n", id,value );
	return 1;
}


int
ACAP_EVENTS_Fire_JSON( const char* Id, cJSON* data ) {
	AXEventKeyValueSet *set = NULL;
	int boolValue;
	GError *error = NULL;
	if(!data) {
		LOG_WARN("%s: Invalid data",__func__);
		return 0;
	}

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
	success = ax_event_handler_send_event(ACAP_EVENTS_HANDLER, event->valueint, axEvent, &error);
	ax_event_key_value_set_free(set);
	ax_event_free(axEvent);
	if(!success)  {
		LOG_WARN("%s: Could not send event %s id = %d %s\n",__func__, Id, event->valueint, error->message);
		g_error_free(error);
		return 0;
	}
	return 1;
}

int
ACAP_EVENTS_Add_Event_JSON( cJSON* event ) {
	AXEventKeyValueSet *set = NULL;
	GError *error = NULL;
	guint declarationID;
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
	return declarationID;
}

/*------------------------------------------------------------------
 * Timer functions
 *------------------------------------------------------------------*/

typedef struct ACAP_Timer {
    char* name;
    int active;
    int repeat_rate_seconds;
    time_t next_trigger;
    ACAP_TIMER_Callback callback;
    struct ACAP_Timer* next;
} ACAP_Timer;

static ACAP_Timer* timer_list = NULL;

int ACAP_TIMER_Set(const char* name, int repeat_rate_seconds, ACAP_TIMER_Callback callback) {
    LOG_TRACE("%s: %s %d seconds", __func__, name, repeat_rate_seconds);
    ACAP_Timer* existing = timer_list;
    
    while (existing) {
        if (strcmp(existing->name, name) == 0) {
            existing->repeat_rate_seconds = repeat_rate_seconds;
            existing->callback = callback;
            existing->next_trigger = time(NULL) + repeat_rate_seconds;
            existing->active = 1;
            return 1;
        }
        existing = existing->next;
    }

    ACAP_Timer* new_timer = (ACAP_Timer*)malloc(sizeof(ACAP_Timer));
    if (!new_timer) return 0;

    new_timer->name = strdup(name);
    new_timer->active = 1;
    new_timer->repeat_rate_seconds = repeat_rate_seconds;
    new_timer->callback = callback;
    new_timer->next_trigger = time(NULL) + repeat_rate_seconds;
    new_timer->next = timer_list;
    timer_list = new_timer;

    return 1;
}

int ACAP_TIMER_Remove(const char* name) {
    LOG_TRACE("%s: %s", __func__, name);
    ACAP_Timer* current = timer_list;
    ACAP_Timer* previous = NULL;
    
    while (current) {
        if (strcmp(current->name, name) == 0) {
            if (previous) {
                previous->next = current->next;
            } else {
                timer_list = current->next;
            }
            free(current->name);
            free(current);
            return 1;
        }
        previous = current;
        current = current->next;
    }
    return 1;
}

void ACAP_TIMER_Cleanup(void) {
    LOG_TRACE("%s:", __func__);
    
    while (timer_list) {
        ACAP_Timer* temp = timer_list;
        timer_list = timer_list->next;
        free(temp->name);
        free(temp);
    }
    timer_list = NULL;
}

void ACAP_TIMER_Process(void) {
    ACAP_Timer* current = timer_list;
    ACAP_Timer* prev = NULL;
    time_t now = time(NULL);

    while (current) {
        if (current->active && now >= current->next_trigger) {
            int result = current->callback(current->name);
        
            if (result == 0) {
                ACAP_Timer* to_remove = current;
                if (prev) {
                    prev->next = current->next;
                    current = current->next;
                } else {
                    timer_list = current->next;
                    current = timer_list;
                }
                free(to_remove->name);
                free(to_remove);
            } else {
                current->next_trigger = now + current->repeat_rate_seconds;
                prev = current;
                current = current->next;
            }
        } else {
            prev = current;
            current = current->next;
        }
    }
}

/*------------------------------------------------------------------
 * Cleanup Implementation
 *------------------------------------------------------------------*/

void ACAP_Cleanup(void) {
    // Clean up other resources
    ACAP_HTTP_Cleanup();
    ACAP_TIMER_Cleanup();
	
    if (status_container) {
        cJSON_Delete(status_container);
        status_container = NULL;
    }

	LOG_TRACE("%s:",__func__);
    if (app) {
        cJSON_Delete(app);
        app = NULL;
    }
    
    if (ACAP_DEVICE_Container) {
        cJSON_Delete(ACAP_DEVICE_Container);
        ACAP_DEVICE_Container = NULL;
    }
    
    http_node_count = 0;
    ACAP_UpdateCallback = NULL;
}

/*------------------------------------------------------------------
 * Error Handling Implementation
 *------------------------------------------------------------------*/

const char* ACAP_Get_Error_String(ACAP_Status status) {
    switch (status) {
        case ACAP_SUCCESS:
            return "Success";
        case ACAP_ERROR_INIT:
            return "Initialization error";
        case ACAP_ERROR_PARAM:
            return "Invalid parameter";
        case ACAP_ERROR_MEMORY:
            return "Memory allocation error";
        case ACAP_ERROR_IO:
            return "I/O error";
        case ACAP_ERROR_HTTP:
            return "HTTP error";
        default:
            return "Unknown error";
    }
}

/*------------------------------------------------------------------
 * Helper functions
 *------------------------------------------------------------------*/

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

