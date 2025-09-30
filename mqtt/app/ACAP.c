/*
 * ACAP SDK wrapper for verion ACAP SDK 12.x
 * Copyright (c) 2025 Fred Juhlin
 * MIT License - See LICENSE file for details
 * Version 4.1
   - Copy all saved setting into settings includin objects
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <syslog.h>
#include <string.h>
#include <errno.h>
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
#include <curl/curl.h>
#include <gio/gio.h>
#include <axsdk/axevent.h>
#include <pthread.h>
#include "ACAP.h"

// Logging macros
#define LOG(fmt, args...) { syslog(LOG_INFO, fmt, ## args); printf(fmt, ## args); }
#define LOG_WARN(fmt, args...) { syslog(LOG_WARNING, fmt, ## args); printf(fmt, ## args); }
//#define LOG_TRACE(fmt, args...) { syslog(LOG_INFO, fmt, ## args); printf(fmt, ## args); }
#define LOG_TRACE(fmt, args...) {}

// Global variables
static cJSON* app = NULL;
static cJSON* status_container = NULL;

cJSON* 		ACAP_STATUS(void);
int			ACAP_HTTP(void);
void		ACAP_HTTP_Process(void);
void		ACAP_HTTP_Cleanup(void);
cJSON*		ACAP_EVENTS(void);
int 		ACAP_FILE_Init(void);
cJSON* 		ACAP_DEVICE(void);
void 		ACAP_VAPIX_Init(void);

static void ACAP_ENDPOINT_settings(const ACAP_HTTP_Response response, const ACAP_HTTP_Request request);
static void ACAP_ENDPOINT_app(const ACAP_HTTP_Response response, const ACAP_HTTP_Request request);
static char ACAP_package_name[ACAP_MAX_PACKAGE_NAME];
static ACAP_Config_Update ACAP_UpdateCallback = NULL;
cJSON* SplitString(const char* input, const char* delimiter);


/*-----------------------------------------------------
 * Core Functions Implementation
 *-----------------------------------------------------*/


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
    cJSON* settings = ACAP_FILE_Read("settings/settings.json");
    if (!settings) {
        settings = cJSON_CreateObject();
    }
    
    cJSON* savedSettings = ACAP_FILE_Read("localdata/settings.json");
    if (savedSettings) {
        cJSON* prop = savedSettings->child;
        while (prop) {
            if (cJSON_GetObjectItem(settings, prop->string))
				cJSON_ReplaceItemInObject(settings, prop->string, cJSON_Duplicate(prop, 1));
            prop = prop->next;
        }
        cJSON_Delete(savedSettings);
    }

    cJSON_AddItemToObject(app, "settings", settings);

    // Initialize subsystems
	ACAP_VAPIX_Init();
    ACAP_EVENTS();
	ACAP_HTTP();
    
    // Register core services
    ACAP_Set_Config("status", ACAP_STATUS());
    ACAP_Set_Config("device", ACAP_DEVICE());
    
    // Register core endpoints
    ACAP_HTTP_Node("app", ACAP_ENDPOINT_app);
    ACAP_HTTP_Node("settings", ACAP_ENDPOINT_settings);

    // Notify about settings
    if (ACAP_UpdateCallback) {
  		cJSON* setting = settings->child;
		while( setting ) {
			ACAP_UpdateCallback(setting->string, setting);
			setting = setting->next;
		}
    }

    LOG_TRACE("%s: Initialization complete\n", __func__);
    return settings;
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
                cJSON_ReplaceItemInObject(settings, param->string, cJSON_Duplicate(param, 1));
				if (ACAP_UpdateCallback)
					ACAP_UpdateCallback(param->string, cJSON_GetObjectItem(settings,param->string) );
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

static pthread_t http_thread;
static int http_thread_running = 0; // Flag to track thread state
static pthread_mutex_t http_nodes_mutex = PTHREAD_MUTEX_INITIALIZER;

typedef struct {
    char path[ACAP_MAX_PATH_LENGTH];
    ACAP_HTTP_Callback callback;
} HTTPNode;

// Thread function for FastCGI processing
void* fastcgi_thread_func(void* arg) {
    while (http_thread_running) {
        ACAP_HTTP_Process(); // Process FastCGI requests
    }
	LOG_TRACE("%s: Exit\n",__func__);
    return NULL;
}

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

int ACAP_HTTP() {
	LOG_TRACE("%s:\n",__func__);
    if (!initialized) {
        if (FCGX_Init() != 0) {
            LOG_WARN("Failed to initialize FCGI\n");
            return 0;
        }
        initialized = 1;

        // Start the FastCGI thread
        http_thread_running = 1;
        if (pthread_create(&http_thread, NULL, fastcgi_thread_func, NULL) != 0) {
            LOG_WARN("Failed to create FastCGI thread\n");
            initialized = 0; // Roll back initialization
            return 0;
        }
    }
    return 1;
}

void ACAP_HTTP_Cleanup() {
    LOG_TRACE("%s:", __func__);

	if(!initialized)
		return;

    // Close the FastCGI socket
    if (fcgi_sock != -1) {
        close(fcgi_sock);
        fcgi_sock = -1;
    }

    // Stop the FastCGI thread
    if (http_thread_running) {
        http_thread_running = 0; // Signal the thread to stop
		pthread_cancel(http_thread); // Request cancellation
        pthread_join(http_thread, NULL); // Wait for the thread to finish
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
    pthread_mutex_lock(&http_nodes_mutex); // Lock the mutex

    // Prevent buffer overflow
    if (http_node_count >= ACAP_MAX_HTTP_NODES) {
        LOG_WARN("Maximum HTTP nodes reached");
        pthread_mutex_unlock(&http_nodes_mutex); // Unlock the mutex
        return 0;
    }

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
            pthread_mutex_unlock(&http_nodes_mutex); // Unlock the mutex
            return 0;
        }
    }

    // Add new node
    snprintf(http_nodes[http_node_count].path, ACAP_MAX_PATH_LENGTH, "%s", full_path);
    http_nodes[http_node_count].callback = callback;
    http_node_count++;

    pthread_mutex_unlock(&http_nodes_mutex); // Unlock the mutex
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

char* url_decode(const char* src) {
    size_t src_len = strlen(src);
    char* decoded = malloc(src_len + 1);
    if (!decoded) return NULL;

    size_t i, j = 0;
    for (i = 0; i < src_len; i++) {
        if (src[i] == '%' && i + 2 < src_len) {
            int high = tolower(src[i + 1]);
            int low = tolower(src[i + 2]);
            if (isxdigit(high) && isxdigit(low)) {
                decoded[j++] = (high >= 'a' ? high - 'a' + 10 : high - '0') * 16 +
                               (low >= 'a' ? low - 'a' + 10 : low - '0');
                i += 2;
            } else {
                decoded[j++] = src[i];
            }
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
                
                char* encoded = malloc(len + 1);
                if (!encoded) return NULL;
                
                strncpy(encoded, found, len);
                encoded[len] = '\0';
                
                char* decoded = url_decode(encoded);
                free(encoded);
                return decoded;  // Caller must free this
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
            if (start != query_string && *(start-1) != '&') {
                start++;
                continue;
            }
            
            start += strlen(search_param);
            char* end = strchr(start, '&');
            size_t len = end ? (size_t)(end - start) : strlen(start);
            
            char* encoded = malloc(len + 1);
            if (!encoded) return NULL;
            
            strncpy(encoded, start, len);
            encoded[len] = '\0';
            
            char* decoded = url_decode(encoded);
            free(encoded);
            return decoded;  // Caller must free this
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
		LOG_WARN("%s: Response failed\n",__func__);
        return 0;
    }
    
    return FCGX_PutStr(buffer, written, response->out) == written;
}

int ACAP_HTTP_Respond_JSON(ACAP_HTTP_Response response, cJSON* object) {
    if (!response || !object) {
        return 0;
    }

    char* jsonString = cJSON_PrintUnformatted(object);
    if (!jsonString) {
        return 0;
    }
    
    ACAP_HTTP_Header_JSON(response);
    
    size_t json_len = strlen(jsonString);
    int result = FCGX_PutStr(jsonString, json_len, response->out) == (int)json_len;
    
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

pthread_mutex_t status_mutex = PTHREAD_MUTEX_INITIALIZER;

static void ACAP_ENDPOINT_status(const ACAP_HTTP_Response response, const ACAP_HTTP_Request request) {
																						  
    const char* method = ACAP_HTTP_Get_Method(request);
    
    if (!method || strcmp(method, "GET") != 0) {
        ACAP_HTTP_Respond_Error(response, 405, "Method Not Allowed - Only GET supported");
        return;
    }

    pthread_mutex_lock(&status_mutex);

    if (!status_container)
        status_container = cJSON_CreateObject();

    ACAP_HTTP_Respond_JSON(response, status_container);

    pthread_mutex_unlock(&status_mutex);
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

    pthread_mutex_lock(&status_mutex);

    cJSON* item = cJSON_GetObjectItem(groupObj, name);
    if (item) {
        cJSON_ReplaceItemInObject(groupObj, name, cJSON_CreateBool(state));
    } else {
        cJSON_AddItemToObject(groupObj, name, cJSON_CreateBool(state));
    }
    pthread_mutex_unlock(&status_mutex);

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

    pthread_mutex_lock(&status_mutex);

    cJSON* item = cJSON_GetObjectItem(groupObj, name);
    if (item) {
        cJSON_ReplaceItemInObject(groupObj, name, cJSON_CreateNumber(value));
    } else {
        cJSON_AddItemToObject(groupObj, name, cJSON_CreateNumber(value));
    }
    pthread_mutex_unlock(&status_mutex);
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

    pthread_mutex_lock(&status_mutex);

    cJSON* item = cJSON_GetObjectItem(groupObj, name);
    if (item) {
        cJSON_ReplaceItemInObject(groupObj, name, cJSON_CreateString(string));
    } else {
        cJSON_AddItemToObject(groupObj, name, cJSON_CreateString(string));
    }
    pthread_mutex_unlock(&status_mutex);
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

    pthread_mutex_lock(&status_mutex);

    cJSON* item = cJSON_GetObjectItem(groupObj, name);
    if (item) {
        cJSON_ReplaceItemInObject(groupObj, name, cJSON_Duplicate(data, 1));
    } else {
        cJSON_AddItemToObject(groupObj, name, cJSON_Duplicate(data, 1));
    }

    pthread_mutex_unlock(&status_mutex);
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
    pthread_mutex_lock(&status_mutex);

    cJSON* item = cJSON_GetObjectItem(groupObj, name);
    if (item) {
        cJSON_ReplaceItemInObject(groupObj, name, cJSON_CreateNull());
    } else {
        cJSON_AddItemToObject(groupObj, name, cJSON_CreateNull());
    }
    pthread_mutex_unlock(&status_mutex);
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

/* -------------------------------------------------------------
LOCATION 
 -------------------------------------------------------------*/
char* ExtractValue(const char* xml, const char* tag) {
    char startTag[64], endTag[64];
    snprintf(startTag, sizeof(startTag), "<%s>", tag);
    snprintf(endTag, sizeof(endTag), "</%s>", tag);

    char* start = strstr(xml, startTag);
    if (!start) return NULL;
    start += strlen(startTag);

    char* end = strstr(start, endTag);
    if (!end) return NULL;

    size_t valueLength = end - start;
    char* value = (char*)malloc(valueLength + 1);
    if (!value) return NULL;

    strncpy(value, start, valueLength);
    value[valueLength] = '\0';
    return value;
}

cJSON* GetLocationData() {
    char *xmlResponse = ACAP_VAPIX_Get("geolocation/get.cgi");
    cJSON* locationData = cJSON_CreateObject();

    if (xmlResponse) {
        // Extract values from the XML response
        char* latStr = ExtractValue(xmlResponse, "Lat");
        char* lonStr = ExtractValue(xmlResponse, "Lng");
        char* headingStr = ExtractValue(xmlResponse, "Heading");
        char* text = ExtractValue(xmlResponse, "Text");

        // Parse latitude
        if (latStr) {
            double lat = atof(latStr); // Convert to double
            cJSON_AddNumberToObject(locationData, "lat", lat);
            free(latStr);
        } else {
            cJSON_AddNumberToObject(locationData, "lat", 0.0); // Default value
        }

        // Parse longitude
        if (lonStr) {
            double lon = atof(lonStr); // Convert to double
            cJSON_AddNumberToObject(locationData, "lon", lon);
            free(lonStr);
        } else {
            cJSON_AddNumberToObject(locationData, "lon", 0.0); // Default value
        }

        // Parse heading
        if (headingStr) {
            int heading = atoi(headingStr); // Convert to integer
            cJSON_AddNumberToObject(locationData, "heading", heading);
            free(headingStr);
        } else {
            cJSON_AddNumberToObject(locationData, "heading", 0); // Default value
        }

        // Parse text
        if (text) {
            cJSON_AddStringToObject(locationData, "text", text);
            free(text);
        } else {
            cJSON_AddStringToObject(locationData, "text", ""); // Default value
        }

        // Free the XML response memory if dynamically allocated
        free(xmlResponse);
    } else {
        fprintf(stderr, "%s: No response from API\n", __func__);
        
        // Add default values in case of no response
        cJSON_AddNumberToObject(locationData, "lat", 0.0);
        cJSON_AddNumberToObject(locationData, "lon", 0.0);
        cJSON_AddNumberToObject(locationData, "heading", 0);
        cJSON_AddStringToObject(locationData, "text", "");
    }

    return locationData;
}

void AppendQueryParameter(char* query, const char* key, const char* value) {
    // If the query string is not empty, add an '&' separator
    if (strlen(query) > 0) {
        strcat(query, "&");
    }
    // Append the key=value pair to the query string
    strcat(query, key);
    strcat(query, "=");
    strcat(query, value);
}

void FormatCoordinates(double lat, double lon, char* latBuffer, size_t latSize, char* lonBuffer, size_t lonSize) {
    // Format latitude: DD.DDDD
    snprintf(latBuffer, latSize, "%.8f", lat);

    // Format longitude: DDD.DDDD (with leading zero if necessary)
    if (lon >= 0 && lon < 100) {
        snprintf(lonBuffer, lonSize, "0%.8f", lon); // Add leading zero for positive two-digit longitude
    } else if (lon > -100 && lon < 0) {
        snprintf(lonBuffer, lonSize, "-0%.8f", -lon); // Add leading zero for negative two-digit longitude
    } else {
        snprintf(lonBuffer, lonSize, "%.8f", lon); // No leading zero needed for three-digit longitude
    }
}

int SetLocationData(cJSON* location) {
    char query[512] = {0}; // Buffer for query string
    char latBuffer[24], lonBuffer[24]; // Buffers for formatted coordinates
    char valueBuffer[64];  // Buffer for other parameters

    // Check and append "lat" and "lon" if present
    cJSON* lat = cJSON_GetObjectItem(location, "lat");
    cJSON* lon = cJSON_GetObjectItem(location, "lon");
    if (lat && lon && !cJSON_IsNull(lat) && !cJSON_IsNull(lon)) {
        // Format latitude and longitude according to the API requirements
        FormatCoordinates(lat->valuedouble, lon->valuedouble, latBuffer, sizeof(latBuffer), lonBuffer, sizeof(lonBuffer));
        AppendQueryParameter(query, "lat", latBuffer);
        AppendQueryParameter(query, "lng", lonBuffer);
    }

    // Check and append "heading" if present
    cJSON* heading = cJSON_GetObjectItem(location, "heading");
    if (heading && !cJSON_IsNull(heading)) {
        snprintf(valueBuffer, sizeof(valueBuffer), "%d", heading->valueint);
        AppendQueryParameter(query, "heading", valueBuffer);
    }

    // Check and append "text" if present
    cJSON* text = cJSON_GetObjectItem(location, "text");
    if (text && !cJSON_IsNull(text)) {
        AppendQueryParameter(query, "text", text->valuestring);
    }

    // If no parameters were added, return an error
    if (strlen(query) == 0) {
        fprintf(stderr, "%s: No valid parameters to set.\n", __func__);
        return 0; // Indicate failure
    }

    // Construct full API URL
    char url[1024];
    snprintf(url, sizeof(url), "geolocation/set.cgi?%s", query);

    // Perform API call (replace with actual implementation)
    char *xmlResponse = ACAP_VAPIX_Get(url);

    // Check response for success or failure
    if (xmlResponse) {
        if (strstr(xmlResponse, "<GeneralSuccess/>")) {
            free(xmlResponse); // Free response memory if dynamically allocated
            return 1;          // Success
        } else {
            fprintf(stderr, "%s: Error in response: %s\n", __func__, xmlResponse);
            free(xmlResponse); // Free response memory if dynamically allocated
            return 0;          // Failure
        }
    }

    fprintf(stderr, "%s: Failed to contact API.\n", __func__);
    return 0; // Indicate failure due to no response
}


cJSON* ACAP_DEVICE(void) {
	ACAP_DEVICE_Container = cJSON_CreateObject();
	cJSON* data = 0;
	cJSON* apiData = 0;
	cJSON* items = 0;
	char *response = 0;

	// Get Device Info
    const char* basicInfo =
		"{"
		  "\"apiVersion\": \"1.0\","
		  "\"context\": \"ACAP\","
		  "\"method\": \"getAllProperties\""
		"}";
	response = ACAP_VAPIX_Post("basicdeviceinfo.cgi",basicInfo);
	if( response ) {
		apiData = cJSON_Parse(response);
		if( apiData ) {
			data  = cJSON_GetObjectItem(apiData,"data");
			if( data )
				data = cJSON_GetObjectItem(data,"propertyList");
		}
	}
	if( data && cJSON_GetObjectItem(data,"SerialNumber") )
		cJSON_AddStringToObject(ACAP_DEVICE_Container,"serial",cJSON_GetObjectItem(data,"SerialNumber")->valuestring);
	else
		cJSON_AddStringToObject(ACAP_DEVICE_Container,"serial","000000000000");
	if( data && cJSON_GetObjectItem(data,"ProdNbr") )
		cJSON_AddStringToObject(ACAP_DEVICE_Container,"model",cJSON_GetObjectItem(data,"ProdNbr")->valuestring);
	else
		cJSON_AddStringToObject(ACAP_DEVICE_Container,"model","Unknown");
	if( data && cJSON_GetObjectItem(data,"Soc") )
		cJSON_AddStringToObject(ACAP_DEVICE_Container,"platform",cJSON_GetObjectItem(data,"Soc")->valuestring);
	else
		cJSON_AddStringToObject(ACAP_DEVICE_Container,"platform","Unknown");
	if( data && cJSON_GetObjectItem(data,"Architecture") )
		cJSON_AddStringToObject(ACAP_DEVICE_Container,"chip",cJSON_GetObjectItem(data,"Architecture")->valuestring);
	else
		cJSON_AddStringToObject(ACAP_DEVICE_Container,"chip","Unknown");
	if( data && cJSON_GetObjectItem(data,"Version") )
		cJSON_AddStringToObject(ACAP_DEVICE_Container,"firmware",cJSON_GetObjectItem(data,"Version")->valuestring);
	else
		cJSON_AddStringToObject(ACAP_DEVICE_Container,"firmware","0.0.0");
	if( apiData )
		cJSON_Delete(apiData);
	apiData = 0;
	if( response )
		free(response);

	//Get IP address
	response = ACAP_VAPIX_Get("param.cgi?action=list&group=root.Network.eth0.IPAddress");
	if( response ) {
		items = SplitString( response, "=" );
		if( items && cJSON_GetArraySize(items) == 2 )
			cJSON_AddStringToObject(ACAP_DEVICE_Container,"IPv4",cJSON_GetArrayItem(items,1)->valuestring);
		else
			cJSON_AddStringToObject(ACAP_DEVICE_Container,"IPv4","");
		free(response);
	} else {
		cJSON_AddStringToObject(ACAP_DEVICE_Container,"IPv4","");
	}
	if( items )
		cJSON_Delete(items);

	cJSON_AddItemToObject(ACAP_DEVICE_Container,"location",GetLocationData());
	
	//Get Camera Aspect ratio
	response = ACAP_VAPIX_Get("param.cgi?action=list&group=root.ImageSource.I0.Sensor.AspectRatio");
	if( response ) {
		items = SplitString( response, "=" );
		if( items && cJSON_GetArraySize(items) == 2 )
			cJSON_AddStringToObject(ACAP_DEVICE_Container,"aspect",cJSON_GetArrayItem(items,1)->valuestring);
		else
			cJSON_AddStringToObject(ACAP_DEVICE_Container,"aspect","16:9");
		free(response);
	} else {
		cJSON_AddStringToObject(ACAP_DEVICE_Container,"aspect","16:9");
	}
	if( items )
		cJSON_Delete(items);
 
	//Get Camera Resolutions
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

	response = ACAP_VAPIX_Get("param.cgi?action=list&group=root.Properties.Image.Resolution");
	if( response ) {
		items = SplitString( response, "=" );
		if( items && cJSON_GetArraySize(items) == 2 ) {
			cJSON* apiResolutions = SplitString( cJSON_GetArrayItem(items,1)->valuestring, "," );
			cJSON_Delete(items);
			if( apiResolutions && cJSON_GetArraySize(apiResolutions) > 4 ) {
				cJSON* resolution = apiResolutions->child;
				while( resolution ) {
					items = SplitString(resolution->valuestring,"x");
					if( items && cJSON_GetArraySize(items) == 2 ) {
						int w = atoi(cJSON_GetArrayItem(items,0)->valuestring);
						int h = atoi(cJSON_GetArrayItem(items,1)->valuestring);
						int a = (w*100)/h;
						if( a == 177 )
							cJSON_AddItemToArray(resolutions169, cJSON_CreateString(resolution->valuestring));
						if( a == 133 )
							cJSON_AddItemToArray(resolutions43, cJSON_CreateString(resolution->valuestring));
						if( a == 160 )
							cJSON_AddItemToArray(resolutions1610, cJSON_CreateString(resolution->valuestring));
						if( a == 100 )
							cJSON_AddItemToArray(resolutions11, cJSON_CreateString(resolution->valuestring));
					}
					if( items )
						cJSON_Delete(items);
					resolution = resolution->next;
				}
				if( apiResolutions )
					cJSON_Delete(apiResolutions);
			}
		}
		free(response);
	}
	return ACAP_DEVICE_Container;
}


/*------------------------------------------------------------------
 * Time and System Information
 *------------------------------------------------------------------*/

double
ACAP_DEVICE_Longitude() {
	if( !ACAP_DEVICE_Container )
		return 0;
	cJSON* location = cJSON_GetObjectItem(ACAP_DEVICE_Container,"location");
	if(!location)
		return 0;
	return cJSON_GetObjectItem(location,"lon")?cJSON_GetObjectItem(location,"lon")->valuedouble:0;
}

double
ACAP_DEVICE_Latitude() {
	if( !ACAP_DEVICE_Container )
		return 0;
	cJSON* location = cJSON_GetObjectItem(ACAP_DEVICE_Container,"location");
	if(!location)
		return 0;
	return cJSON_GetObjectItem(location,"lat")?cJSON_GetObjectItem(location,"lat")->valuedouble:0;
}

int
ACAP_DEVICE_Set_Location( double lat, double lon) {
	LOG_TRACE("%s: %f %f\n",__func__,lat,lon);
	if( !ACAP_DEVICE_Container )
		return 0;
	cJSON* location = cJSON_GetObjectItem(ACAP_DEVICE_Container,"location");
	if(!location ) {
		LOG_WARN("%s: Missing location data\n",__func__);
		return 0;
	}
	cJSON_ReplaceItemInObject(location,"lat",cJSON_CreateNumber(lat));
	cJSON_ReplaceItemInObject(location,"lon",cJSON_CreateNumber(lon));
	return SetLocationData(location);
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
    if (!filepath ) {
        LOG_WARN("Invalid parameters for file operation\n");
        return NULL;
    }

	LOG_TRACE("%s: %s\n", __func__, filepath);

    char fullpath[ACAP_MAX_PATH_LENGTH];
    if (snprintf(fullpath, sizeof(fullpath), "%s%s", ACAP_FILE_Path, filepath) >= sizeof(fullpath)) {
        LOG_WARN("Path too long\n");
        return NULL;
    }
    
    FILE* file = fopen(fullpath, mode);
    if (!file) {
        LOG_TRACE("%s: Opening file %s failed: %s\n", __func__, fullpath, strerror(errno));
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
	LOG_TRACE("%s: %s\n",__func__,filepath);
    if (!filepath) {
        LOG_WARN("Invalid filepath\n");
        return NULL;
    }

    FILE* file = ACAP_FILE_Open(filepath, "r");
    if (!file) {
		LOG_TRACE("%s: File open error %s\n",__func__,filepath);
        return NULL;
    }

    // Get file size
    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    fseek(file, 0, SEEK_SET);

    if (size < 2) {
        LOG_WARN("%s: File size error in %s\n", __func__, filepath);
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
static void ACAP_EVENTS_Main_Callback(guint subscription, AXEvent *event, gpointer user_data);
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
	
	cJSON* events = ACAP_FILE_Read( "settings/events.json" );
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
	if( strcmp(topic,"Device/Configuration") == 0 ) {
		cJSON_Delete(object);
		return 0;
	}
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


// gpointer points to the name used when subscribing to event
static void
ACAP_EVENTS_Main_Callback(guint subscription, AXEvent *axEvent, gpointer user_data) {
	LOG_TRACE("%s:\n",__func__);

	cJSON* eventData = ACAP_EVENTS_Parse(axEvent);
	if( !eventData ) {
		ax_event_free(axEvent);	
		return;
	}

	cJSON_AddItemReferenceToObject(eventData, "source", (cJSON*)user_data);	
	if( EVENT_USER_CALLBACK )
		EVENT_USER_CALLBACK( eventData, (void*)user_data );
	cJSON_Delete(eventData);
	ax_event_free(axEvent);	
}

/*
event structure
{
	"name": "All ACAP Events",
	"topic0": {"tnsaxis":"CameraApplicationPlatform"},
	"topic1": {"tnsaxis":"someService"},
	"topic2": {"tnsaxis":"someEvent"}
}
 
*/

int
ACAP_EVENTS_Subscribe( cJSON *event, void* user_data ) {
	AXEventKeyValueSet *keyset = 0;	
	cJSON *topic;
	guint declarationID = 0;

	if(!ACAP_EVENTS_HANDLER) {
		LOG_WARN("%s: Event handler not initialize\n",__func__);
		return 0;
	}

	char *json = cJSON_PrintUnformatted(event);
	if( json ) {
		LOG_TRACE("%s: %s\n",__func__,json);
		free(json);
	}

	if( !event ) {
		LOG_WARN("%s: Invalid event\n",__func__);
		return 0;
	}


	if( !cJSON_GetObjectItem( event,"name" ) || !cJSON_GetObjectItem( event,"name" )->valuestring || !strlen(cJSON_GetObjectItem( event,"name" )->valuestring) ) {
		LOG_WARN("%s: Event declaration is missing name\n",__func__);
		return 0;
	}

	if( !cJSON_GetObjectItem( event,"topic0" ) ) {
		LOG_WARN("%s: Event declaration is missing topic0\n",__func__);
		return 0;
	}
	
	keyset = ax_event_key_value_set_new();
	if( !keyset ) {
		LOG_WARN("ACAP_EVENTS_Subscribe: Unable to create keyset\n");
		return 0;
	}


	// ----- TOPIC 0 ------
	topic = cJSON_GetObjectItem( event,"topic0" );
	if(!topic) {
		LOG_WARN("ACAP_EVENTS_Subscribe: Invalid tag for topic 0");
		return 0;
	}
	if( !ax_event_key_value_set_add_key_value( keyset, "topic0", topic->child->string, topic->child->valuestring, AX_VALUE_TYPE_STRING,NULL) ) {
		LOG_WARN("ACAP_EVENTS_Subscribe: Topic 0 keyset error");
		ax_event_key_value_set_free(keyset);
		return 0;
	}
	LOG_TRACE("%s: topic0 %s:%s\n",__func__, topic->child->string, topic->child->valuestring );
	
	// ----- TOPIC 1 ------
	if( cJSON_GetObjectItem( event,"topic1" ) ) {
		topic = cJSON_GetObjectItem( event,"topic1" );
		if( !ax_event_key_value_set_add_key_value( keyset, "topic1", topic->child->string, topic->child->valuestring, AX_VALUE_TYPE_STRING,NULL) ) {
			LOG_WARN("ACAP_EVENTS_Subscribe: Unable to subscribe to event (1)");
			ax_event_key_value_set_free(keyset);
			return 0;
		}
		LOG_TRACE("%s: topic1 %s:%s\n",__func__, topic->child->string, topic->child->valuestring );
	}
	//------ TOPIC 2 -------------
	if( cJSON_GetObjectItem( event,"topic2" ) ) {
		topic = cJSON_GetObjectItem( event,"topic2" );
		if( !ax_event_key_value_set_add_key_value( keyset, "topic2", topic->child->string, topic->child->valuestring, AX_VALUE_TYPE_STRING,NULL) ) {
			LOG_WARN("ACAP_EVENTS_Subscribe: Unable to subscribe to event (2)");
			ax_event_key_value_set_free(keyset);
			return 0;
		}
		LOG_TRACE("%s: topic2 %s:%s\n",__func__, topic->child->string, topic->child->valuestring );
	}
	
	if( cJSON_GetObjectItem( event,"topic3" ) ) {
		LOG_TRACE("%s: topic3:%s:%s", __func__,topic->child->string,topic->child->valuestring);
		topic = cJSON_GetObjectItem( event,"topic3" );
		if( !ax_event_key_value_set_add_key_value( keyset, "topic3", topic->child->string, topic->child->valuestring, AX_VALUE_TYPE_STRING,NULL) ) {
			LOG_WARN("ACAP_EVENTS_Subscribe: Unable to subscribe to event (3)");
			ax_event_key_value_set_free(keyset);
			return 0;
		}
		LOG_TRACE("%s: topic3 %s %s\n",__func__, topic->child->string, topic->child->valuestring );
	}

	int ax = ax_event_handler_subscribe(
		ACAP_EVENTS_HANDLER, 
		keyset, 
		&declarationID,
		ACAP_EVENTS_Main_Callback,
		(gpointer)user_data,
		NULL
	);
	
	ax_event_key_value_set_free(keyset);
	if( !ax ) {
		LOG_WARN("ACAP_EVENTS_Subscribe: Unable to subscribe to event\n");
		return 0;
	}
	cJSON_AddItemToArray(ACAP_EVENTS_SUBSCRIPTIONS,cJSON_CreateNumber(declarationID));
	return declarationID;
}

int ACAP_EVENTS_Unsubscribe(int id) {
    LOG_TRACE("%s: Unsubscribing id=%d\n", __func__, id);
    
    if (!ACAP_EVENTS_SUBSCRIPTIONS) {
        return 0;
    }

    if (id == 0) {  // Unsubscribe all
        cJSON* event = ACAP_EVENTS_SUBSCRIPTIONS->child;
        while (event) {
            ax_event_handler_unsubscribe(ACAP_EVENTS_HANDLER, (guint)event->valueint, 0);
            event = event->next;
        }
        cJSON_Delete(ACAP_EVENTS_SUBSCRIPTIONS);
        ACAP_EVENTS_SUBSCRIPTIONS = cJSON_CreateArray();
    } else {
        // Find and remove specific subscription
        cJSON* event = ACAP_EVENTS_SUBSCRIPTIONS->child;
        cJSON* prev = NULL;
        
        while (event) {
            if (event->valueint == id) {
                ax_event_handler_unsubscribe(ACAP_EVENTS_HANDLER, (guint)id, 0);
                if (prev) {
                    prev->next = event->next;
                } else {
                    ACAP_EVENTS_SUBSCRIPTIONS->child = event->next;
                }
                cJSON_Delete(event);
                break;
            }
            prev = event;
            event = event->next;
        }
    }
    
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

/*-----------------------------------------------------
 * VAPIX
 *-----------------------------------------------------*/
char *VAPIX_Credentials = 0;
CURL* VAPIX_CURL = 0;

// Callback function to append data to a dynamically allocated buffer
static size_t append_to_buffer_callback(char* ptr, size_t size, size_t nmemb, void* userdata) {
    size_t processed_bytes = size * nmemb;
    char** response_buffer = (char**)userdata;

    // Reallocate buffer to accommodate new data
    size_t current_length = *response_buffer ? strlen(*response_buffer) : 0;
    char* new_buffer = realloc(*response_buffer, current_length + processed_bytes + 1);
    if (!new_buffer) {
        LOG_WARN("%s: Memory allocation failed", __func__);
        return 0; // Signal an error to libcurl
    }

    // Append new data to the buffer
    memcpy(new_buffer + current_length, ptr, processed_bytes);
    new_buffer[current_length + processed_bytes] = '\0'; // Null-terminate the string

    *response_buffer = new_buffer;
    return processed_bytes;
}

char* ACAP_VAPIX_Get(const char* endpoint) {
    if (!VAPIX_Credentials || !VAPIX_CURL || !endpoint) {
        LOG_WARN("%s: Invalid input\n", __func__);
        return NULL;
    }

    char* response = NULL; // Initialize response buffer
    char* url = malloc(strlen("http://127.0.0.12/axis-cgi/") + strlen(endpoint) + 1);
    if (!url) {
        LOG_WARN("%s: Memory allocation failed", __func__);
        return NULL;
    }
    sprintf(url, "http://127.0.0.12/axis-cgi/%s", endpoint);

    curl_easy_setopt(VAPIX_CURL, CURLOPT_URL, url);
    curl_easy_setopt(VAPIX_CURL, CURLOPT_USERPWD, VAPIX_Credentials);
    curl_easy_setopt(VAPIX_CURL, CURLOPT_HTTPAUTH, CURLAUTH_BASIC);
    curl_easy_setopt(VAPIX_CURL, CURLOPT_HTTPGET, 1L); // Explicitly set GET method
    curl_easy_setopt(VAPIX_CURL, CURLOPT_WRITEFUNCTION, append_to_buffer_callback);
    curl_easy_setopt(VAPIX_CURL, CURLOPT_WRITEDATA, &response);

    CURLcode res = curl_easy_perform(VAPIX_CURL);
    free(url);

    if (res != CURLE_OK) {
        LOG_WARN("%s: %d: %s", __func__, res, curl_easy_strerror(res));
        free(response);
        return NULL;
    }

    long response_code;
    curl_easy_getinfo(VAPIX_CURL, CURLINFO_RESPONSE_CODE, &response_code);
    if (response_code >= 300) {
        LOG_WARN("%s: Code %ld %s\n", __func__, response_code, response ? response : "No response");
        free(response);
        return NULL;
    }

    LOG_TRACE("%s: %s\n", __func__, response ? response : "No response");
    return response; // Caller must free this buffer
}

char* ACAP_VAPIX_Post(const char* endpoint, const char* request) {
    if (!VAPIX_Credentials || !VAPIX_CURL || !endpoint || !request) {
        LOG_WARN("%s: Invalid input\n", __func__);
        return NULL;
    }

	LOG_TRACE("%s: %s %s\n",__func__,endpoint,request);

    char* response = NULL; // Initialize response buffer
    char* url = malloc(strlen("http://127.0.0.12/axis-cgi/") + strlen(endpoint) + 1);
    if (!url) {
        LOG_WARN("%s: Memory allocation failed", __func__);
        return NULL;
    }
    sprintf(url, "http://127.0.0.12/axis-cgi/%s", endpoint);

    curl_easy_setopt(VAPIX_CURL, CURLOPT_URL, url);
    curl_easy_setopt(VAPIX_CURL, CURLOPT_USERPWD, VAPIX_Credentials);
    curl_easy_setopt(VAPIX_CURL, CURLOPT_HTTPAUTH, CURLAUTH_BASIC);
    curl_easy_setopt(VAPIX_CURL, CURLOPT_POSTFIELDS, request);
    curl_easy_setopt(VAPIX_CURL, CURLOPT_WRITEFUNCTION, append_to_buffer_callback);
    curl_easy_setopt(VAPIX_CURL, CURLOPT_WRITEDATA, &response);

    CURLcode res = curl_easy_perform(VAPIX_CURL);
    free(url);

    if (res != CURLE_OK) {
        LOG_WARN("%s: %d: %s", __func__, res, curl_easy_strerror(res));
        free(response);
        return NULL;
    }

    long response_code;
    curl_easy_getinfo(VAPIX_CURL, CURLINFO_RESPONSE_CODE, &response_code);
    if (response_code >= 300) {
        LOG_WARN("%s: Code %ld %s\n", __func__, response_code, response ? response : "No response");
        free(response);
        return NULL;
    }

    LOG_TRACE("%s: %s\n", __func__, response ? response : "No response");
    return response; // Caller must free this buffer
}

static char* parse_credentials(GVariant* result) {
    char* credentials_string = NULL;
    char* id                 = NULL;
    char* password           = NULL;

    g_variant_get(result, "(&s)", &credentials_string);
    if (sscanf(credentials_string, "%m[^:]:%ms", &id, &password) != 2)
        LOG_WARN("%s: Error parsing credential string %s", __func__, credentials_string);
    char* credentials = g_strdup_printf("%s:%s", id, password);

    free(id);
    free(password);
	LOG_TRACE("%s: %s\n",__func__, credentials);
    return credentials;
}

static char* retrieve_vapix_credentials(const char* username) {
    GError* error               = NULL;
    GDBusConnection* connection = g_bus_get_sync(G_BUS_TYPE_SYSTEM, NULL, &error);
    if (!connection) {
        LOG_TRACE("%s: Error connecting to D-Bus: %s",__func__, error->message);
		return 0;
	}

    const char* bus_name       = "com.axis.HTTPConf1";
    const char* object_path    = "/com/axis/HTTPConf1/VAPIXServiceAccounts1";
    const char* interface_name = "com.axis.HTTPConf1.VAPIXServiceAccounts1";
    const char* method_name    = "GetCredentials";

    GVariant* result = g_dbus_connection_call_sync(connection,
                                                   bus_name,
                                                   object_path,
                                                   interface_name,
                                                   method_name,
                                                   g_variant_new("(s)", username),
                                                   NULL,
                                                   G_DBUS_CALL_FLAGS_NONE,
                                                   -1,
                                                   NULL,
                                                   &error);
    if (!result) {
        LOG_WARN("%s: Error invoking D-Bus method: %s",__func__, error->message);
		g_object_unref(connection);
		return 0;
	}

    char* credentials = parse_credentials(result);

    g_variant_unref(result);
    g_object_unref(connection);
    return credentials;
}

void
ACAP_VAPIX_Init() {
	LOG_TRACE("%s:\n",__func__);
	VAPIX_CURL = curl_easy_init();
	VAPIX_Credentials = retrieve_vapix_credentials("acap");
}

/*------------------------------------------------------------------
 * Cleanup Implementation
 *------------------------------------------------------------------*/

void ACAP_Cleanup(void) {
    // Clean up other resources
    ACAP_HTTP_Cleanup();
	
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

cJSON* SplitString(const char* input, const char* delimiter) {
    if (!input || !delimiter) {
        return NULL; // Invalid input
    }

    // Create a cJSON array to hold the split strings
    cJSON* json_array = cJSON_CreateArray();
    if (!json_array) {
        return NULL; // Failed to create JSON array
    }

    // Make a copy of the input string because strtok modifies it
    char* input_copy = strdup(input);
    if (!input_copy) {
        cJSON_Delete(json_array);
        return NULL; // Memory allocation failure
    }

    // Remove trailing newline character, if present
    size_t len = strlen(input_copy);
    if (len > 0 && input_copy[len - 1] == '\n') {
        input_copy[len - 1] = '\0';
    }

    // Use strtok to split the string by the delimiter
    char* token = strtok(input_copy, delimiter);
    while (token != NULL) {
        // Add each token to the JSON array
        cJSON* json_token = cJSON_CreateString(token);
        if (!json_token) {
            free(input_copy);
            cJSON_Delete(json_array);
            return NULL; // Memory allocation failure for JSON string
        }
        cJSON_AddItemToArray(json_array, json_token);

        // Get the next token
        token = strtok(NULL, delimiter);
    }

    free(input_copy); // Free the temporary copy of the input string
    return json_array;
}

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

