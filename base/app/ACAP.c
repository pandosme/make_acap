/*
 * ACAP SDK wrapper for ACAP SDK 12.x
 * Copyright (c) 2025 Fred Juhlin
 * MIT License - See LICENSE file for details
 * Version 4.0
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
#include <curl/curl.h>
#include <gio/gio.h>
#include <axsdk/axevent.h>
#include <pthread.h>
#include "fcgi_stdio.h"
#include "ACAP.h"

/*-----------------------------------------------------
 * Logging macros
 *-----------------------------------------------------*/
#define LOG(fmt, args...)       { syslog(LOG_INFO, fmt, ## args); printf(fmt, ## args); }
#define LOG_WARN(fmt, args...)  { syslog(LOG_WARNING, fmt, ## args); printf(fmt, ## args); }
//#define LOG_TRACE(fmt, args...) { syslog(LOG_INFO, fmt, ## args); printf(fmt, ## args); }
#define LOG_TRACE(fmt, args...) {}

/*-----------------------------------------------------
 * Opaque HTTP type definitions (internal)
 *-----------------------------------------------------*/
struct ACAP_HTTP_Request_T {
    FCGX_Request*   fcgi;
    char*           postData;
    size_t          postDataLength;
    const char*     method;
    const char*     contentType;
    const char*     queryString;
};

struct ACAP_HTTP_Response_T {
    FCGX_Request*   fcgi;
};

/*-----------------------------------------------------
 * Global variables
 *-----------------------------------------------------*/
static cJSON* app = NULL;
static cJSON* status_container = NULL;

/*-----------------------------------------------------
 * Internal forward declarations
 *-----------------------------------------------------*/
cJSON*  ACAP_STATUS(void);
int     ACAP_HTTP(void);
void    ACAP_HTTP_Process(void);
void    ACAP_HTTP_Cleanup(void);
cJSON*  ACAP_EVENTS(void);
int     ACAP_FILE_Init(void);
cJSON*  ACAP_DEVICE(void);
void    ACAP_VAPIX_Init(void);

static void ACAP_ENDPOINT_settings(const ACAP_HTTP_Response response, const ACAP_HTTP_Request request);
static void ACAP_ENDPOINT_app(const ACAP_HTTP_Response response, const ACAP_HTTP_Request request);
static char ACAP_package_name[ACAP_MAX_PACKAGE_NAME];
static ACAP_Config_Update ACAP_UpdateCallback = NULL;
cJSON* SplitString(const char* input, const char* delimiter);

/*=====================================================
 * Core Functions
 *=====================================================*/

const char* ACAP_Version(void) {
    return ACAP_VERSION;
}

cJSON* ACAP_Init(const char* package, ACAP_Config_Update callback) {
    if (!package) {
        LOG_WARN("Invalid package name\n");
        return NULL;
    }

    LOG_TRACE("%s: Initializing ACAP for package %s\n", __func__, package);

    strncpy(ACAP_package_name, package, ACAP_MAX_PACKAGE_NAME - 1);
    ACAP_package_name[ACAP_MAX_PACKAGE_NAME - 1] = '\0';

    if (!ACAP_FILE_Init()) {
        LOG_WARN("Failed to initialize file system\n");
        return NULL;
    }

    ACAP_UpdateCallback = callback;

    app = cJSON_CreateObject();
    if (!app) {
        LOG_WARN("Failed to create app object\n");
        return NULL;
    }

    /* Load manifest */
    cJSON* manifest = ACAP_FILE_Read("manifest.json");
    if (manifest) {
        cJSON_AddItemToObject(app, "manifest", manifest);
    }

    /* Load and merge settings */
    cJSON* settings = ACAP_FILE_Read("settings/settings.json");
    if (!settings) {
        settings = cJSON_CreateObject();
    }

    cJSON* savedSettings = ACAP_FILE_Read("localdata/settings.json");
    if (savedSettings) {
        cJSON* prop = savedSettings->child;
        while (prop) {
            if (cJSON_GetObjectItem(settings, prop->string)) {
                if (prop->type == cJSON_Object) {
                    cJSON* settingsProp = cJSON_GetObjectItem(settings, prop->string);
                    if (!cJSON_IsObject(settingsProp)) {
                        /* Default is null or non-object — replace entirely */
                        cJSON_ReplaceItemInObject(settings, prop->string, cJSON_Duplicate(prop, 1));
                    } else {
                        /* Default is also an object — merge sub-keys */
                        cJSON* subprop = prop->child;
                        while (subprop) {
                            if (cJSON_GetObjectItem(settingsProp, subprop->string))
                                cJSON_ReplaceItemInObject(settingsProp, subprop->string, cJSON_Duplicate(subprop, 1));
                            subprop = subprop->next;
                        }
                    }
                } else {
                    cJSON_ReplaceItemInObject(settings, prop->string, cJSON_Duplicate(prop, 1));
                }
            }
            prop = prop->next;
        }
        cJSON_Delete(savedSettings);
    }

    cJSON_AddItemToObject(app, "settings", settings);

    /* Initialize subsystems */
    ACAP_VAPIX_Init();
    cJSON* events = ACAP_EVENTS();
    if (events) {
        cJSON_Delete(events);
    }
    ACAP_HTTP();

    ACAP_Set_Config("status", ACAP_STATUS());
    ACAP_Set_Config("device", ACAP_DEVICE());

    ACAP_HTTP_Node("app", ACAP_ENDPOINT_app);
    ACAP_HTTP_Node("settings", ACAP_ENDPOINT_settings);

    /* Notify about initial settings */
    if (ACAP_UpdateCallback) {
        cJSON* setting = settings->child;
        while (setting) {
            ACAP_UpdateCallback(setting->string, setting);
            setting = setting->next;
        }
    }

    LOG_TRACE("%s: Initialization complete\n", __func__);
    return settings;
}

static void
ACAP_ENDPOINT_app(const ACAP_HTTP_Response response, const ACAP_HTTP_Request request) {
    const char* method = ACAP_HTTP_Get_Method(request);
    if (method && strcmp(method, "POST") == 0) {
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

    if (strcmp(method, "GET") == 0) {
        ACAP_HTTP_Respond_JSON(response, cJSON_GetObjectItem(app, "settings"));
        return;
    }

    if (strcmp(method, "POST") == 0) {
        const char* contentType = ACAP_HTTP_Get_Content_Type(request);
        if (!contentType || strcmp(contentType, "application/json") != 0) {
            ACAP_HTTP_Respond_Error(response, 415, "Unsupported Media Type - Use application/json");
            return;
        }

        const char* body = ACAP_HTTP_Get_Body(request);
        size_t bodyLen = ACAP_HTTP_Get_Body_Length(request);
        if (!body || bodyLen == 0) {
            ACAP_HTTP_Respond_Error(response, 400, "Missing POST data");
            return;
        }

        cJSON* params = cJSON_Parse(body);
        if (!params) {
            ACAP_HTTP_Respond_Error(response, 400, "Invalid JSON data");
            return;
        }

        LOG_TRACE("%s: %s\n", __func__, body);

        cJSON* settings = cJSON_GetObjectItem(app, "settings");
        cJSON* param = params->child;
        while (param) {
            if (cJSON_GetObjectItem(settings, param->string)) {
                cJSON_ReplaceItemInObject(settings, param->string, cJSON_Duplicate(param, 1));
                if (ACAP_UpdateCallback)
                    ACAP_UpdateCallback(param->string, cJSON_GetObjectItem(settings, param->string));
            }
            param = param->next;
        }

        cJSON_Delete(params);
        ACAP_FILE_Write("localdata/settings.json", settings);
        ACAP_HTTP_Respond_Text(response, "Settings updated successfully");
        return;
    }

    ACAP_HTTP_Respond_Error(response, 405, "Method Not Allowed - Use GET or POST");
}

const char* ACAP_Name(void) {
    return ACAP_package_name;
}

int ACAP_Set_Config(const char* service, cJSON* serviceSettings) {
    LOG_TRACE("%s: %s\n", __func__, service);
    if (cJSON_GetObjectItem(app, service)) {
        LOG_TRACE("%s: %s already registered\n", __func__, service);
        return 1;
    }
    cJSON_AddItemToObject(app, service, serviceSettings);
    return 1;
}

cJSON* ACAP_Get_Config(const char* service) {
    cJSON* requestedService = cJSON_GetObjectItem(app, service);
    if (!requestedService) {
        LOG_WARN("%s: %s is undefined\n", __func__, service);
        return NULL;
    }
    return requestedService;
}

/*=====================================================
 * HTTP Server Implementation
 *=====================================================*/

static pthread_t http_thread;
static int http_thread_running = 0;
static pthread_mutex_t http_nodes_mutex = PTHREAD_MUTEX_INITIALIZER;

typedef struct {
    char path[ACAP_MAX_PATH_LENGTH];
    ACAP_HTTP_Callback callback;
} HTTPNode;

static int initialized = 0;
static int fcgi_sock = -1;
static HTTPNode http_nodes[ACAP_MAX_HTTP_NODES];
static int http_node_count = 0;

void* fastcgi_thread_func(void* arg) {
    while (http_thread_running) {
        ACAP_HTTP_Process();
    }
    LOG_TRACE("%s: Exit\n", __func__);
    return NULL;
}

static const char* get_path_without_query(const char* uri, char* path, size_t path_size) {
    const char* query = strchr(uri, '?');
    if (query) {
        size_t path_length = query - uri;
        if (path_length >= path_size)
            path_length = path_size - 1;
        strncpy(path, uri, path_length);
        path[path_length] = '\0';
        return path;
    }
    return uri;
}

int ACAP_HTTP(void) {
    LOG_TRACE("%s:\n", __func__);
    if (!initialized) {
        if (FCGX_Init() != 0) {
            LOG_WARN("Failed to initialize FCGI\n");
            return 0;
        }
        initialized = 1;

        http_thread_running = 1;
        if (pthread_create(&http_thread, NULL, fastcgi_thread_func, NULL) != 0) {
            LOG_WARN("Failed to create FastCGI thread\n");
            initialized = 0;
            return 0;
        }
    }
    return 1;
}

void ACAP_HTTP_Cleanup(void) {
    LOG_TRACE("%s:", __func__);
    if (!initialized)
        return;

    if (fcgi_sock != -1) {
        close(fcgi_sock);
        fcgi_sock = -1;
    }

    if (http_thread_running) {
        http_thread_running = 0;
        pthread_cancel(http_thread);
        pthread_join(http_thread, NULL);
    }
    initialized = 0;
}

int ACAP_HTTP_Node(const char* nodename, ACAP_HTTP_Callback callback) {
    pthread_mutex_lock(&http_nodes_mutex);

    if (http_node_count >= ACAP_MAX_HTTP_NODES) {
        LOG_WARN("Maximum HTTP nodes reached");
        pthread_mutex_unlock(&http_nodes_mutex);
        return 0;
    }

    char full_path[ACAP_MAX_PATH_LENGTH];
    if (nodename[0] == '/') {
        snprintf(full_path, ACAP_MAX_PATH_LENGTH, "/local/%s%s", ACAP_Name(), nodename);
    } else {
        snprintf(full_path, ACAP_MAX_PATH_LENGTH, "/local/%s/%s", ACAP_Name(), nodename);
    }

    for (int i = 0; i < http_node_count; i++) {
        if (strcmp(http_nodes[i].path, full_path) == 0) {
            LOG_WARN("Duplicate HTTP node path: %s", full_path);
            pthread_mutex_unlock(&http_nodes_mutex);
            return 0;
        }
    }

    snprintf(http_nodes[http_node_count].path, ACAP_MAX_PATH_LENGTH, "%s", full_path);
    http_nodes[http_node_count].callback = callback;
    http_node_count++;

    pthread_mutex_unlock(&http_nodes_mutex);
    return 1;
}

/*-----------------------------------------------------
 * HTTP Request Accessors
 *-----------------------------------------------------*/

const char* ACAP_HTTP_Get_Method(const ACAP_HTTP_Request request) {
    if (!request || !request->fcgi)
        return NULL;
    return FCGX_GetParam("REQUEST_METHOD", request->fcgi->envp);
}

const char* ACAP_HTTP_Get_Content_Type(const ACAP_HTTP_Request request) {
    if (!request || !request->fcgi)
        return NULL;
    return FCGX_GetParam("CONTENT_TYPE", request->fcgi->envp);
}

size_t ACAP_HTTP_Get_Content_Length(const ACAP_HTTP_Request request) {
    if (!request || !request->fcgi)
        return 0;
    const char* contentLength = FCGX_GetParam("CONTENT_LENGTH", request->fcgi->envp);
    return contentLength ? (size_t)atoll(contentLength) : 0;
}

const char* ACAP_HTTP_Get_Body(const ACAP_HTTP_Request request) {
    if (!request)
        return NULL;
    return request->postData;
}

size_t ACAP_HTTP_Get_Body_Length(const ACAP_HTTP_Request request) {
    if (!request)
        return 0;
    return request->postDataLength;
}

/*-----------------------------------------------------
 * HTTP Request Processing
 *-----------------------------------------------------*/

void ACAP_HTTP_Process(void) {
    FCGX_Request fcgi_request;
    struct ACAP_HTTP_Request_T  requestData  = {0};
    struct ACAP_HTTP_Response_T responseData  = {0};
    char* socket_path = NULL;

    if (!initialized)
        return;

    socket_path = getenv("FCGI_SOCKET_NAME");
    if (!socket_path) {
        LOG_WARN("Failed to get FCGI_SOCKET_NAME\n");
        return;
    }

    if (fcgi_sock == -1) {
        fcgi_sock = FCGX_OpenSocket(socket_path, 5);
        if (fcgi_sock < 0) {
            LOG_WARN("Failed to open FCGI socket\n");
            return;
        }
        chmod(socket_path, 0777);
    }

    if (FCGX_InitRequest(&fcgi_request, fcgi_sock, 0) != 0) {
        LOG_WARN("FCGX_InitRequest failed\n");
        return;
    }

    if (FCGX_Accept_r(&fcgi_request) != 0) {
        FCGX_Free(&fcgi_request, 1);
        return;
    }

    /* Wire up opaque types to the FCGI request */
    requestData.fcgi = &fcgi_request;
    requestData.method = FCGX_GetParam("REQUEST_METHOD", fcgi_request.envp);
    requestData.contentType = FCGX_GetParam("CONTENT_TYPE", fcgi_request.envp);
    responseData.fcgi = &fcgi_request;

    /* Read POST body */
    if (requestData.method && strcmp(requestData.method, "POST") == 0) {
        size_t contentLength = ACAP_HTTP_Get_Content_Length(&requestData);
        if (contentLength > 0 && contentLength < ACAP_MAX_BUFFER_SIZE) {
            char* postData = malloc(contentLength + 1);
            if (postData) {
                size_t bytesRead = FCGX_GetStr(postData, contentLength, fcgi_request.in);
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

    /* Route to handler */
    const char* uriString = FCGX_GetParam("REQUEST_URI", fcgi_request.envp);
    if (!uriString) {
        ACAP_HTTP_Respond_Error(&responseData, 400, "Invalid URI");
        goto cleanup;
    }

    char pathBuffer[ACAP_MAX_PATH_LENGTH];
    const char* pathOnly = get_path_without_query(uriString, pathBuffer, sizeof(pathBuffer));
    ACAP_HTTP_Callback matching_callback = NULL;

    for (int i = 0; i < http_node_count; i++) {
        if (strcmp(http_nodes[i].path, pathOnly) == 0) {
            matching_callback = http_nodes[i].callback;
            break;
        }
    }

    if (matching_callback) {
        matching_callback(&responseData, &requestData);
    } else {
        ACAP_HTTP_Respond_Error(&responseData, 404, "Not Found");
    }

cleanup:
    if (requestData.postData) {
        free(requestData.postData);
    }
    FCGX_Finish_r(&fcgi_request);
}

/*-----------------------------------------------------
 * HTTP Request Parameter Handling
 *-----------------------------------------------------*/

static char* url_decode(const char* src) {
    size_t src_len = strlen(src);
    char* decoded = malloc(src_len + 1);
    if (!decoded) return NULL;

    size_t i, j = 0;
    for (i = 0; i < src_len; i++) {
        if (src[i] == '%' && i + 2 < src_len) {
            int high = tolower(src[i + 1]);
            int low  = tolower(src[i + 2]);
            if (isxdigit(high) && isxdigit(low)) {
                decoded[j++] = (high >= 'a' ? high - 'a' + 10 : high - '0') * 16 +
                               (low  >= 'a' ? low  - 'a' + 10 : low  - '0');
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

char* ACAP_HTTP_Request_Param(const ACAP_HTTP_Request request, const char* name) {
    if (!request || !request->fcgi || !name) {
        LOG_WARN("Invalid request parameters\n");
        return NULL;
    }

    /* Check POST form data first */
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
                return decoded;
            }
        }
    }

    /* Then check query string */
    const char* query_string = FCGX_GetParam("QUERY_STRING", request->fcgi->envp);
    if (query_string) {
        char search_param[512];
        snprintf(search_param, sizeof(search_param), "%s=", name);

        const char* start = query_string;
        while ((start = strstr(start, search_param)) != NULL) {
            if (start != query_string && *(start - 1) != '&') {
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
            return decoded;
        }
    }

    return NULL;
}

cJSON* ACAP_HTTP_Request_JSON(const ACAP_HTTP_Request request, const char* param) {
    if (!request)
        return NULL;

    if (param) {
        char* value = ACAP_HTTP_Request_Param(request, param);
        if (!value)
            return NULL;
        cJSON* json = cJSON_Parse(value);
        free(value);
        return json;
    }

    /* Parse POST body as JSON */
    const char* body = ACAP_HTTP_Get_Body(request);
    if (!body || ACAP_HTTP_Get_Body_Length(request) == 0)
        return NULL;

    const char* contentType = ACAP_HTTP_Get_Content_Type(request);
    if (!contentType || strcmp(contentType, "application/json") != 0)
        return NULL;

    return cJSON_Parse(body);
}

/*-----------------------------------------------------
 * HTTP Response Functions
 *-----------------------------------------------------*/

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
    if (!response || !filename || !contenttype)
        return 0;

    return ACAP_HTTP_Respond_String(response,
        "Content-Type: %s\r\n"
        "Content-Disposition: attachment; filename=%s\r\n"
        "Content-Transfer-Encoding: binary\r\n"
        "Content-Length: %u\r\n"
        "\r\n",
        contenttype, filename, filelength);
}

int ACAP_HTTP_Respond_String(ACAP_HTTP_Response response, const char* fmt, ...) {
    if (!response || !response->fcgi || !response->fcgi->out || !fmt)
        return 0;

    char buffer[8192];
    va_list args;
    va_start(args, fmt);
    int written = vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);

    if (written < 0 || written >= (int)sizeof(buffer)) {
        LOG_WARN("%s: Response failed\n", __func__);
        return 0;
    }

    return FCGX_PutStr(buffer, written, response->fcgi->out) == written;
}

int ACAP_HTTP_Respond_JSON(ACAP_HTTP_Response response, cJSON* object) {
    if (!response || !object)
        return 0;

    char* jsonString = cJSON_PrintUnformatted(object);
    if (!jsonString)
        return 0;

    ACAP_HTTP_Header_JSON(response);

    size_t json_len = strlen(jsonString);
    int result = FCGX_PutStr(jsonString, json_len, response->fcgi->out) == (int)json_len;

    free(jsonString);
    return result;
}

int ACAP_HTTP_Respond_Data(ACAP_HTTP_Response response, size_t count, const void* data) {
    if (!response || !response->fcgi || !response->fcgi->out || !data || count == 0) {
        LOG_WARN("Invalid response parameters\n");
        return 0;
    }
    return FCGX_PutStr(data, count, response->fcgi->out) == (int)count;
}

int ACAP_HTTP_Respond_Error(ACAP_HTTP_Response response, int code, const char* message) {
    if (!response || !message)
        return 0;

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
    if (!response || !message)
        return 0;
    return ACAP_HTTP_Header_TEXT(response) &&
           ACAP_HTTP_Respond_String(response, "%s", message);
}

/*=====================================================
 * Status Management
 *=====================================================*/

static pthread_mutex_t status_mutex = PTHREAD_MUTEX_INITIALIZER;

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
        ACAP_HTTP_Node("status", ACAP_ENDPOINT_status);
    }
    return status_container;
}

cJSON* ACAP_STATUS_Group(const char* name) {
    if (!name || !status_container)
        return NULL;

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

/* Internal helper: set a value in a status group (thread-safe) */
static void status_set_item(const char* group, const char* name, cJSON* value) {
    if (!group || !name || !value) {
        cJSON_Delete(value);
        return;
    }
    cJSON* groupObj = ACAP_STATUS_Group(group);
    if (!groupObj) {
        cJSON_Delete(value);
        return;
    }
    pthread_mutex_lock(&status_mutex);
    cJSON_DeleteItemFromObject(groupObj, name);
    cJSON_AddItemToObject(groupObj, name, value);
    pthread_mutex_unlock(&status_mutex);
}

void ACAP_STATUS_SetBool(const char* group, const char* name, int state) {
    status_set_item(group, name, cJSON_CreateBool(state));
}

void ACAP_STATUS_SetNumber(const char* group, const char* name, double value) {
    status_set_item(group, name, cJSON_CreateNumber(value));
}

void ACAP_STATUS_SetString(const char* group, const char* name, const char* string) {
    if (!string) { LOG_WARN("Invalid parameters\n"); return; }
    status_set_item(group, name, cJSON_CreateString(string));
}

void ACAP_STATUS_SetObject(const char* group, const char* name, cJSON* data) {
    if (!data) { LOG_WARN("Invalid parameters\n"); return; }
    status_set_item(group, name, cJSON_Duplicate(data, 1));
}

void ACAP_STATUS_SetNull(const char* group, const char* name) {
    status_set_item(group, name, cJSON_CreateNull());
}

/*-----------------------------------------------------
 * Status Getters
 *-----------------------------------------------------*/

int ACAP_STATUS_Bool(const char* group, const char* name) {
    cJSON* groupObj = ACAP_STATUS_Group(group);
    if (!groupObj) return 0;
    cJSON* item = cJSON_GetObjectItem(groupObj, name);
    return (item && item->type == cJSON_True) ? 1 : 0;
}

int ACAP_STATUS_Int(const char* group, const char* name) {
    cJSON* groupObj = ACAP_STATUS_Group(group);
    if (!groupObj) return 0;
    cJSON* item = cJSON_GetObjectItem(groupObj, name);
    return (item && cJSON_IsNumber(item)) ? item->valueint : 0;
}

double ACAP_STATUS_Double(const char* group, const char* name) {
    cJSON* groupObj = ACAP_STATUS_Group(group);
    if (!groupObj) return 0.0;
    cJSON* item = cJSON_GetObjectItem(groupObj, name);
    return (item && cJSON_IsNumber(item)) ? item->valuedouble : 0.0;
}

char* ACAP_STATUS_String(const char* group, const char* name) {
    cJSON* groupObj = ACAP_STATUS_Group(group);
    if (!groupObj) return NULL;
    cJSON* item = cJSON_GetObjectItem(groupObj, name);
    return (item && cJSON_IsString(item)) ? item->valuestring : NULL;
}

cJSON* ACAP_STATUS_Object(const char* group, const char* name) {
    cJSON* groupObj = ACAP_STATUS_Group(group);
    if (!groupObj) return NULL;
    return cJSON_GetObjectItem(groupObj, name);
}

/*=====================================================
 * Device Information
 *=====================================================*/

static cJSON* ACAP_DEVICE_Container = NULL;

static double previousTransmitted = 0;
static double previousNetworkTimestamp = 0;
static double previousNetworkAverage = 0;

double ACAP_DEVICE_Network_Average(void) {
    FILE* fd = fopen("/proc/net/dev", "r");
    if (!fd) {
        LOG_WARN("Error opening /proc/net/dev");
        return 0;
    }

    char line[500];
    char eth_line[500] = "";
    while (fgets(line, sizeof(line), fd)) {
        if (strstr(line, "eth0"))
            snprintf(eth_line, sizeof(eth_line), "%s", line);
    }
    fclose(fd);

    if (strlen(eth_line) < 20) {
        LOG_WARN("ACAP_DEVICE_Network_Average: eth0 not found\n");
        return 0;
    }

    unsigned long rx_bytes = 0, tx_bytes = 0;
    if (sscanf(eth_line, "%*s %lu %*u %*u %*u %*u %*u %*u %*u %lu",
               &rx_bytes, &tx_bytes) != 2) {
        LOG_WARN("ACAP_DEVICE_Network_Average: parse error\n");
        return 0;
    }
    (void)rx_bytes;

    double transmitted = (double)tx_bytes;
    double diff = transmitted - previousTransmitted;
    previousTransmitted = transmitted;
    if (diff < 0)
        return previousNetworkAverage;

    double timestamp = ACAP_DEVICE_Timestamp();
    double timeDiff = (timestamp - previousNetworkTimestamp) / 1000.0;
    previousNetworkTimestamp = timestamp;

    double kbps = (diff * 8.0) / (1024.0 * timeDiff);
    previousNetworkAverage = (kbps < 0.001) ? 0 : kbps;
    return previousNetworkAverage;
}

double ACAP_DEVICE_CPU_Average(void) {
    struct sysinfo info;
    sysinfo(&info);
    return (double)info.loads[2] / 65536.0;
}

double ACAP_DEVICE_Uptime(void) {
    struct sysinfo info;
    sysinfo(&info);
    return (double)info.uptime;
}

const char* ACAP_DEVICE_Prop(const char* attribute) {
    if (!ACAP_DEVICE_Container) return NULL;
    cJSON* item = cJSON_GetObjectItem(ACAP_DEVICE_Container, attribute);
    return (item && cJSON_IsString(item)) ? item->valuestring : NULL;
}

int ACAP_DEVICE_Prop_Int(const char* attribute) {
    if (!ACAP_DEVICE_Container) return 0;
    cJSON* item = cJSON_GetObjectItem(ACAP_DEVICE_Container, attribute);
    return (item && cJSON_IsNumber(item)) ? item->valueint : 0;
}

cJSON* ACAP_DEVICE_JSON(const char* attribute) {
    if (!ACAP_DEVICE_Container) return NULL;
    return cJSON_GetObjectItem(ACAP_DEVICE_Container, attribute);
}

/*-----------------------------------------------------
 * Location helpers
 *-----------------------------------------------------*/

static char* ExtractValue(const char* xml, const char* tag) {
    char startTag[64], endTag[64];
    snprintf(startTag, sizeof(startTag), "<%s>", tag);
    snprintf(endTag, sizeof(endTag), "</%s>", tag);

    char* start = strstr(xml, startTag);
    if (!start) return NULL;
    start += strlen(startTag);

    char* end = strstr(start, endTag);
    if (!end) return NULL;

    size_t valueLength = end - start;
    char* value = malloc(valueLength + 1);
    if (!value) return NULL;
    strncpy(value, start, valueLength);
    value[valueLength] = '\0';
    return value;
}

static cJSON* GetLocationData(void) {
    char* xmlResponse = ACAP_VAPIX_Get("geolocation/get.cgi");
    cJSON* locationData = cJSON_CreateObject();

    if (xmlResponse) {
        char* latStr     = ExtractValue(xmlResponse, "Lat");
        char* lonStr     = ExtractValue(xmlResponse, "Lng");
        char* headingStr = ExtractValue(xmlResponse, "Heading");
        char* text       = ExtractValue(xmlResponse, "Text");

        cJSON_AddNumberToObject(locationData, "lat",     latStr ? atof(latStr) : 0.0);
        cJSON_AddNumberToObject(locationData, "lon",     lonStr ? atof(lonStr) : 0.0);
        cJSON_AddNumberToObject(locationData, "heading", headingStr ? atoi(headingStr) : 0);
        cJSON_AddStringToObject(locationData, "text",    text ? text : "");

        free(latStr);
        free(lonStr);
        free(headingStr);
        free(text);
        free(xmlResponse);
    } else {
        cJSON_AddNumberToObject(locationData, "lat", 0.0);
        cJSON_AddNumberToObject(locationData, "lon", 0.0);
        cJSON_AddNumberToObject(locationData, "heading", 0);
        cJSON_AddStringToObject(locationData, "text", "");
    }

    return locationData;
}

static void AppendQueryParameter(char* query, size_t query_size, const char* key, const char* value) {
    size_t current_len = strlen(query);
    size_t needed_len = current_len + strlen(key) + strlen(value) + 2;
    if (current_len > 0) needed_len++;
    if (needed_len >= query_size) {
        LOG_WARN("AppendQueryParameter: Buffer overflow prevented\n");
        return;
    }
    if (current_len > 0) strcat(query, "&");
    strcat(query, key);
    strcat(query, "=");
    strcat(query, value);
}

static void FormatCoordinates(double lat, double lon, char* latBuf, size_t latSize, char* lonBuf, size_t lonSize) {
    snprintf(latBuf, latSize, "%.8f", lat);
    if (lon >= 0 && lon < 100)
        snprintf(lonBuf, lonSize, "0%.8f", lon);
    else if (lon > -100 && lon < 0)
        snprintf(lonBuf, lonSize, "-0%.8f", -lon);
    else
        snprintf(lonBuf, lonSize, "%.8f", lon);
}

static int SetLocationData(cJSON* location) {
    char query[512] = {0};
    char latBuf[24], lonBuf[24], valueBuf[64];

    cJSON* lat = cJSON_GetObjectItem(location, "lat");
    cJSON* lon = cJSON_GetObjectItem(location, "lon");
    if (lat && lon && !cJSON_IsNull(lat) && !cJSON_IsNull(lon)) {
        FormatCoordinates(lat->valuedouble, lon->valuedouble, latBuf, sizeof(latBuf), lonBuf, sizeof(lonBuf));
        AppendQueryParameter(query, sizeof(query), "lat", latBuf);
        AppendQueryParameter(query, sizeof(query), "lng", lonBuf);
    }

    cJSON* heading = cJSON_GetObjectItem(location, "heading");
    if (heading && !cJSON_IsNull(heading)) {
        snprintf(valueBuf, sizeof(valueBuf), "%d", heading->valueint);
        AppendQueryParameter(query, sizeof(query), "heading", valueBuf);
    }

    cJSON* text = cJSON_GetObjectItem(location, "text");
    if (text && !cJSON_IsNull(text))
        AppendQueryParameter(query, sizeof(query), "text", text->valuestring);

    if (strlen(query) == 0)
        return 0;

    char url[1024];
    snprintf(url, sizeof(url), "geolocation/set.cgi?%s", query);
    char* xmlResponse = ACAP_VAPIX_Get(url);
    if (xmlResponse) {
        int success = strstr(xmlResponse, "<GeneralSuccess/>") != NULL;
        free(xmlResponse);
        return success;
    }
    return 0;
}

/*-----------------------------------------------------
 * ACAP_DEVICE() — build device information at startup
 *-----------------------------------------------------*/

static void add_device_prop(cJSON* container, const char* key, cJSON* data,
                            const char* apiKey, const char* fallback) {
    cJSON* item = data ? cJSON_GetObjectItem(data, apiKey) : NULL;
    cJSON_AddStringToObject(container, key,
        (item && item->valuestring) ? item->valuestring : fallback);
}

static void device_load_resolutions(cJSON* container) {
    cJSON* resolutions = cJSON_CreateObject();
    cJSON_AddItemToObject(container, "resolutions", resolutions);

    cJSON* res169  = cJSON_AddArrayToObject(resolutions, "16:9");
    cJSON* res43   = cJSON_AddArrayToObject(resolutions, "4:3");
    cJSON* res11   = cJSON_AddArrayToObject(resolutions, "1:1");
    cJSON* res1610 = cJSON_AddArrayToObject(resolutions, "16:10");

    char* response = ACAP_VAPIX_Get("param.cgi?action=list&group=root.Properties.Image.Resolution");
    if (!response) return;

    cJSON* items = SplitString(response, "=");
    free(response);
    if (!items || cJSON_GetArraySize(items) != 2) {
        if (items) cJSON_Delete(items);
        return;
    }

    cJSON* resList = SplitString(cJSON_GetArrayItem(items, 1)->valuestring, ",");
    cJSON_Delete(items);
    if (!resList) return;

    cJSON* res = resList->child;
    while (res) {
        cJSON* wh = SplitString(res->valuestring, "x");
        if (wh && cJSON_GetArraySize(wh) == 2) {
            int w = atoi(cJSON_GetArrayItem(wh, 0)->valuestring);
            int h = atoi(cJSON_GetArrayItem(wh, 1)->valuestring);
            int aspect = (w * 100) / h;
            if (aspect == 177) cJSON_AddItemToArray(res169,  cJSON_CreateString(res->valuestring));
            if (aspect == 133) cJSON_AddItemToArray(res43,   cJSON_CreateString(res->valuestring));
            if (aspect == 160) cJSON_AddItemToArray(res1610, cJSON_CreateString(res->valuestring));
            if (aspect == 100) cJSON_AddItemToArray(res11,   cJSON_CreateString(res->valuestring));
        }
        if (wh) cJSON_Delete(wh);
        res = res->next;
    }
    cJSON_Delete(resList);
}

cJSON* ACAP_DEVICE(void) {
    ACAP_DEVICE_Container = cJSON_CreateObject();
    cJSON* data = NULL;
    cJSON* apiData = NULL;
    char* response = NULL;

    /* Basic device info */
    const char* basicInfo =
        "{\"apiVersion\":\"1.0\",\"context\":\"ACAP\",\"method\":\"getAllProperties\"}";
    response = ACAP_VAPIX_Post("basicdeviceinfo.cgi", basicInfo);
    if (response) {
        apiData = cJSON_Parse(response);
        if (apiData) {
            data = cJSON_GetObjectItem(apiData, "data");
            if (data) data = cJSON_GetObjectItem(data, "propertyList");
        }
    }

    add_device_prop(ACAP_DEVICE_Container, "serial",   data, "SerialNumber",  "000000000000");
    add_device_prop(ACAP_DEVICE_Container, "model",    data, "ProdNbr",       "Unknown");
    add_device_prop(ACAP_DEVICE_Container, "platform", data, "Soc",           "Unknown");
    add_device_prop(ACAP_DEVICE_Container, "chip",     data, "Architecture",  "Unknown");
    add_device_prop(ACAP_DEVICE_Container, "firmware", data, "Version",       "0.0.0");

    if (apiData) cJSON_Delete(apiData);
    if (response) free(response);

    /* IP address */
    response = ACAP_VAPIX_Get("param.cgi?action=list&group=root.Network.eth0.IPAddress");
    if (response) {
        cJSON* items = SplitString(response, "=");
        if (items && cJSON_GetArraySize(items) == 2)
            cJSON_AddStringToObject(ACAP_DEVICE_Container, "IPv4", cJSON_GetArrayItem(items, 1)->valuestring);
        else
            cJSON_AddStringToObject(ACAP_DEVICE_Container, "IPv4", "");
        if (items) cJSON_Delete(items);
        free(response);
    } else {
        cJSON_AddStringToObject(ACAP_DEVICE_Container, "IPv4", "");
    }

    /* Location */
    cJSON_AddItemToObject(ACAP_DEVICE_Container, "location", GetLocationData());

    /* Aspect ratio */
    response = ACAP_VAPIX_Get("param.cgi?action=list&group=root.ImageSource.I0.Sensor.AspectRatio");
    if (response) {
        cJSON* items = SplitString(response, "=");
        if (items && cJSON_GetArraySize(items) == 2)
            cJSON_AddStringToObject(ACAP_DEVICE_Container, "aspect", cJSON_GetArrayItem(items, 1)->valuestring);
        else
            cJSON_AddStringToObject(ACAP_DEVICE_Container, "aspect", "16:9");
        if (items) cJSON_Delete(items);
        free(response);
    } else {
        cJSON_AddStringToObject(ACAP_DEVICE_Container, "aspect", "16:9");
    }

    /* Resolutions */
    device_load_resolutions(ACAP_DEVICE_Container);

    return ACAP_DEVICE_Container;
}

/*-----------------------------------------------------
 * Time and System Information
 *-----------------------------------------------------*/

double ACAP_DEVICE_Longitude(void) {
    if (!ACAP_DEVICE_Container) return 0;
    cJSON* location = cJSON_GetObjectItem(ACAP_DEVICE_Container, "location");
    if (!location) return 0;
    cJSON* lon = cJSON_GetObjectItem(location, "lon");
    return lon ? lon->valuedouble : 0;
}

double ACAP_DEVICE_Latitude(void) {
    if (!ACAP_DEVICE_Container) return 0;
    cJSON* location = cJSON_GetObjectItem(ACAP_DEVICE_Container, "location");
    if (!location) return 0;
    cJSON* lat = cJSON_GetObjectItem(location, "lat");
    return lat ? lat->valuedouble : 0;
}

int ACAP_DEVICE_Set_Location(double lat, double lon) {
    LOG_TRACE("%s: %f %f\n", __func__, lat, lon);
    if (!ACAP_DEVICE_Container) return 0;
    cJSON* location = cJSON_GetObjectItem(ACAP_DEVICE_Container, "location");
    if (!location) {
        LOG_WARN("%s: Missing location data\n", __func__);
        return 0;
    }
    cJSON_ReplaceItemInObject(location, "lat", cJSON_CreateNumber(lat));
    cJSON_ReplaceItemInObject(location, "lon", cJSON_CreateNumber(lon));
    return SetLocationData(location);
}

int ACAP_DEVICE_Seconds_Since_Midnight(void) {
    time_t t = time(NULL);
    struct tm tm;
    localtime_r(&t, &tm);
    return tm.tm_hour * 3600 + tm.tm_min * 60 + tm.tm_sec;
}

static char ACAP_DEVICE_timestring[128] = "2020-01-01 00:00:00";
static char ACAP_DEVICE_date[128] = "2023-01-01";
static char ACAP_DEVICE_time_buf[128] = "00:00:00";
static char ACAP_DEVICE_isostring[128] = "2020-01-01T00:00:00+0000";

const char* ACAP_DEVICE_Date(void) {
    time_t t = time(NULL);
    struct tm tm;
    localtime_r(&t, &tm);
    snprintf(ACAP_DEVICE_date, sizeof(ACAP_DEVICE_date),
             "%d-%02d-%02d", tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday);
    return ACAP_DEVICE_date;
}

const char* ACAP_DEVICE_Time(void) {
    time_t t = time(NULL);
    struct tm tm;
    localtime_r(&t, &tm);
    snprintf(ACAP_DEVICE_time_buf, sizeof(ACAP_DEVICE_time_buf),
             "%02d:%02d:%02d", tm.tm_hour, tm.tm_min, tm.tm_sec);
    return ACAP_DEVICE_time_buf;
}

const char* ACAP_DEVICE_Local_Time(void) {
    time_t t = time(NULL);
    struct tm tm;
    localtime_r(&t, &tm);
    snprintf(ACAP_DEVICE_timestring, sizeof(ACAP_DEVICE_timestring),
             "%d-%02d-%02d %02d:%02d:%02d",
             tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
             tm.tm_hour, tm.tm_min, tm.tm_sec);
    return ACAP_DEVICE_timestring;
}

const char* ACAP_DEVICE_ISOTime(void) {
    time_t t = time(NULL);
    struct tm tm;
    localtime_r(&t, &tm);
    strftime(ACAP_DEVICE_isostring, sizeof(ACAP_DEVICE_isostring), "%Y-%m-%dT%T%z", &tm);
    return ACAP_DEVICE_isostring;
}

double ACAP_DEVICE_Timestamp(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec * 1000.0 + tv.tv_usec / 1000.0;
}

/*=====================================================
 * File Operations
 *=====================================================*/

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
    if (!filepath) {
        LOG_WARN("Invalid parameters for file operation\n");
        return NULL;
    }
    LOG_TRACE("%s: %s\n", __func__, filepath);

    char fullpath[ACAP_MAX_PATH_LENGTH];
    if (snprintf(fullpath, sizeof(fullpath), "%s%s", ACAP_FILE_Path, filepath) >= (int)sizeof(fullpath)) {
        LOG_WARN("Path too long\n");
        return NULL;
    }

    FILE* file = fopen(fullpath, mode);
    if (!file)
        LOG_TRACE("%s: Opening file %s failed: %s\n", __func__, fullpath, strerror(errno));
    return file;
}

int ACAP_FILE_Delete(const char* filepath) {
    if (!filepath) return 0;

    char fullpath[ACAP_MAX_PATH_LENGTH];
    snprintf(fullpath, sizeof(fullpath), "%s%s", ACAP_FILE_Path, filepath);

    if (remove(fullpath) != 0) {
        LOG_WARN("Delete %s failed\n", fullpath);
        return 0;
    }
    return 1;
}

cJSON* ACAP_FILE_Read(const char* filepath) {
    LOG_TRACE("%s: %s\n", __func__, filepath);
    if (!filepath) {
        LOG_WARN("Invalid filepath\n");
        return NULL;
    }

    FILE* file = ACAP_FILE_Open(filepath, "r");
    if (!file) {
        LOG_TRACE("%s: File open error %s\n", __func__, filepath);
        return NULL;
    }

    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    fseek(file, 0, SEEK_SET);

    if (size < 2) {
        LOG_WARN("%s: File size error in %s\n", __func__, filepath);
        fclose(file);
        return NULL;
    }

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

int ACAP_FILE_WriteData(const char* filepath, const char* data) {
    if (!filepath || !data) {
        LOG_WARN("Invalid parameters for file write data\n");
        return 0;
    }

    FILE* file = ACAP_FILE_Open(filepath, "w");
    if (!file) {
        LOG_WARN("Error opening %s for writing\n", filepath);
        return 0;
    }

    int result = fputs(data, file);
    fclose(file);

    if (result < 0) {
        LOG_WARN("Write error in %s\n", filepath);
        return 0;
    }
    return 1;
}

int ACAP_FILE_Exists(const char* filepath) {
    if (!filepath) return 0;
    char fullpath[ACAP_MAX_PATH_LENGTH];
    if (snprintf(fullpath, sizeof(fullpath), "%s%s", ACAP_FILE_Path, filepath) >= (int)sizeof(fullpath))
        return 0;
    return access(fullpath, F_OK) == 0;
}

/*=====================================================
 * Events System
 *=====================================================*/

static ACAP_EVENTS_Callback EVENT_USER_CALLBACK = NULL;
static void ACAP_EVENTS_Main_Callback(guint subscription, AXEvent* event, gpointer user_data);
static cJSON* ACAP_EVENTS_SUBSCRIPTIONS = NULL;
static cJSON* ACAP_EVENTS_DECLARATIONS = NULL;
static AXEventHandler* ACAP_EVENTS_HANDLER = NULL;

static char ACAP_EVENTS_PACKAGE[64];
static char ACAP_EVENTS_APPNAME[64];

/* Internal SDK struct access for event parsing */
typedef struct S_AXEventKeyValueSet {
    GHashTable* key_values;
} T_ValueSet;

typedef struct S_NamespaceKeyPair {
    gchar* name_space;
    gchar* key;
} T_KeyPair;

typedef struct S_ValueElement {
    gint int_value;
    gboolean bool_value;
    gdouble double_value;
    gchar* str_value;
    AXEventElementItem* elem_value;
    gchar* elem_str_value;
    GList* tags;
    gchar* key_nice_name;
    gchar* value_nice_name;
    gboolean defined;
    gboolean onvif_data;
    AXEventValueType value_type;
} T_ValueElement;

cJSON* ACAP_EVENTS(void) {
    LOG_TRACE("%s:\n", __func__);

    cJSON* manifest = ACAP_Get_Config("manifest");
    if (!manifest) return cJSON_CreateNull();
    cJSON* acapPackageConf = cJSON_GetObjectItem(manifest, "acapPackageConf");
    if (!acapPackageConf) return cJSON_CreateNull();
    cJSON* setup = cJSON_GetObjectItem(acapPackageConf, "setup");
    if (!setup) return cJSON_CreateNull();
    cJSON* appName = cJSON_GetObjectItem(setup, "appName");
    cJSON* friendlyName = cJSON_GetObjectItem(setup, "friendlyName");
    if (!appName || !appName->valuestring || !friendlyName || !friendlyName->valuestring) {
        LOG_WARN("Missing appName or friendlyName in manifest setup");
        return cJSON_CreateNull();
    }

    snprintf(ACAP_EVENTS_PACKAGE, sizeof(ACAP_EVENTS_PACKAGE), "%s", appName->valuestring);
    snprintf(ACAP_EVENTS_APPNAME, sizeof(ACAP_EVENTS_APPNAME), "%s", friendlyName->valuestring);
    LOG_TRACE("%s: %s %s\n", __func__, ACAP_EVENTS_PACKAGE, ACAP_EVENTS_APPNAME);

    ACAP_EVENTS_SUBSCRIPTIONS = cJSON_CreateArray();
    ACAP_EVENTS_DECLARATIONS = cJSON_CreateObject();
    ACAP_EVENTS_HANDLER = ax_event_handler_new();

    cJSON* events = ACAP_FILE_Read("settings/events.json");
    if (!events)
        LOG_WARN("Cannot load event list\n");
    cJSON* event = events ? events->child : NULL;
    while (event) {
        ACAP_EVENTS_Add_Event_JSON(event);
        event = event->next;
    }
    return events;
}

int ACAP_EVENTS_SetCallback(ACAP_EVENTS_Callback callback) {
    LOG_TRACE("%s: Entry\n", __func__);
    EVENT_USER_CALLBACK = callback;
    return 1;
}

cJSON* ACAP_EVENTS_Parse(AXEvent* axEvent) {
    LOG_TRACE("%s: Entry\n", __func__);

    const T_ValueSet* set = (T_ValueSet*)ax_event_get_key_value_set(axEvent);
    cJSON* object = cJSON_CreateObject();

    char topics[6][32];
    for (int i = 0; i < 6; i++)
        topics[i][0] = '\0';

    GHashTableIter iter;
    T_KeyPair* nskp;
    T_ValueElement* value_element;
    g_hash_table_iter_init(&iter, set->key_values);

    while (g_hash_table_iter_next(&iter, (gpointer*)&nskp, (gpointer*)&value_element)) {
        /* Check if this is a topic key (topic0..topic5) */
        int isTopic = 0;
        for (int i = 0; i < 6; i++) {
            char key[8];
            snprintf(key, sizeof(key), "topic%d", i);
            if (strcmp(nskp->key, key) == 0) {
                if (i == 0 && strcmp((char*)value_element->str_value, "CameraApplicationPlatform") == 0)
                    snprintf(topics[0], sizeof(topics[0]), "acap");
                else
                    snprintf(topics[i], sizeof(topics[i]), "%s", value_element->str_value);
                isTopic = 1;
                break;
            }
        }

        if (!isTopic && value_element->defined) {
            switch (value_element->value_type) {
                case AX_VALUE_TYPE_INT:
                    cJSON_AddNumberToObject(object, nskp->key, (double)value_element->int_value);
                    break;
                case AX_VALUE_TYPE_BOOL:
                    cJSON_AddBoolToObject(object, nskp->key, value_element->bool_value);
                    break;
                case AX_VALUE_TYPE_DOUBLE:
                    cJSON_AddNumberToObject(object, nskp->key, value_element->double_value);
                    break;
                case AX_VALUE_TYPE_STRING:
                    cJSON_AddStringToObject(object, nskp->key, value_element->str_value);
                    break;
                case AX_VALUE_TYPE_ELEMENT:
                    cJSON_AddStringToObject(object, nskp->key, value_element->elem_str_value);
                    break;
                default:
                    cJSON_AddNullToObject(object, nskp->key);
                    break;
            }
        }
    }

    /* Build topic path */
    char topic[200];
    strncpy(topic, topics[0], sizeof(topic) - 1);
    topic[sizeof(topic) - 1] = '\0';
    for (int i = 1; i < 6; i++) {
        if (strlen(topics[i]) > 0) {
            size_t remaining = sizeof(topic) - strlen(topic) - 1;
            if (remaining > 1) {
                strncat(topic, "/", remaining);
                remaining = sizeof(topic) - strlen(topic) - 1;
                strncat(topic, topics[i], remaining);
            }
        }
    }

    cJSON_AddStringToObject(object, "event", topic);
    return object;
}

static void
ACAP_EVENTS_Main_Callback(guint subscription, AXEvent* axEvent, gpointer user_data) {
    LOG_TRACE("%s:\n", __func__);

    cJSON* eventData = ACAP_EVENTS_Parse(axEvent);
    if (!eventData) {
        ax_event_free(axEvent);
        return;
    }

    if (user_data)
        cJSON_AddItemReferenceToObject(eventData, "source", (cJSON*)user_data);
    if (EVENT_USER_CALLBACK)
        EVENT_USER_CALLBACK(eventData, user_data);
    cJSON_Delete(eventData);
    ax_event_free(axEvent);
}

/*-----------------------------------------------------
 * Event subscription helper — adds one topic level to keyset
 *-----------------------------------------------------*/
static int add_subscribe_topic(AXEventKeyValueSet* keyset, cJSON* event, const char* topicKey) {
    cJSON* topic = cJSON_GetObjectItem(event, topicKey);
    if (!topic)
        return 1;  /* Optional topic, skip */
    if (!topic->child) {
        LOG_WARN("ACAP_EVENTS_Subscribe: Invalid %s structure\n", topicKey);
        return 0;
    }
    if (!ax_event_key_value_set_add_key_value(keyset, topicKey,
            topic->child->string, topic->child->valuestring, AX_VALUE_TYPE_STRING, NULL)) {
        LOG_WARN("ACAP_EVENTS_Subscribe: Failed to add %s\n", topicKey);
        return 0;
    }
    LOG_TRACE("%s: %s %s:%s\n", __func__, topicKey, topic->child->string, topic->child->valuestring);
    return 1;
}

int ACAP_EVENTS_Subscribe(cJSON* event, void* user_data) {
    guint declarationID = 0;

    if (!ACAP_EVENTS_HANDLER) {
        LOG_WARN("%s: Event handler not initialized\n", __func__);
        return 0;
    }
    if (!event) {
        LOG_WARN("%s: Invalid event\n", __func__);
        return 0;
    }

    cJSON* nameItem = cJSON_GetObjectItem(event, "name");
    if (!nameItem || !nameItem->valuestring || !strlen(nameItem->valuestring)) {
        LOG_WARN("%s: Event declaration is missing name\n", __func__);
        return 0;
    }
    if (!cJSON_GetObjectItem(event, "topic0")) {
        LOG_WARN("%s: Event declaration is missing topic0\n", __func__);
        return 0;
    }

    AXEventKeyValueSet* keyset = ax_event_key_value_set_new();
    if (!keyset) {
        LOG_WARN("%s: Unable to create keyset\n", __func__);
        return 0;
    }

    /* Add topic0 (required) then optional topic1..topic3 */
    const char* topicKeys[] = { "topic0", "topic1", "topic2", "topic3" };
    for (int i = 0; i < 4; i++) {
        if (!add_subscribe_topic(keyset, event, topicKeys[i])) {
            ax_event_key_value_set_free(keyset);
            return 0;
        }
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
    if (!ax) {
        LOG_WARN("ACAP_EVENTS_Subscribe: Unable to subscribe to event\n");
        return 0;
    }
    cJSON_AddItemToArray(ACAP_EVENTS_SUBSCRIPTIONS, cJSON_CreateNumber(declarationID));
    return declarationID;
}

int ACAP_EVENTS_Unsubscribe(int id) {
    LOG_TRACE("%s: Unsubscribing id=%d\n", __func__, id);

    if (!ACAP_EVENTS_SUBSCRIPTIONS)
        return 0;

    if (id == 0) {
        cJSON* event = ACAP_EVENTS_SUBSCRIPTIONS->child;
        while (event) {
            ax_event_handler_unsubscribe(ACAP_EVENTS_HANDLER, (guint)event->valueint, 0);
            event = event->next;
        }
        cJSON_Delete(ACAP_EVENTS_SUBSCRIPTIONS);
        ACAP_EVENTS_SUBSCRIPTIONS = cJSON_CreateArray();
    } else {
        int arraySize = cJSON_GetArraySize(ACAP_EVENTS_SUBSCRIPTIONS);
        for (int i = 0; i < arraySize; i++) {
            cJSON* item = cJSON_GetArrayItem(ACAP_EVENTS_SUBSCRIPTIONS, i);
            if (item && item->valueint == id) {
                ax_event_handler_unsubscribe(ACAP_EVENTS_HANDLER, (guint)id, 0);
                cJSON_DeleteItemFromArray(ACAP_EVENTS_SUBSCRIPTIONS, i);
                break;
            }
        }
    }
    return 1;
}

int ACAP_EVENTS_Add_Event(const char* id, const char* name, int state) {
    AXEventKeyValueSet* set = NULL;
    int dummy_value = 0;
    guint declarationID;

    if (!ACAP_EVENTS_HANDLER || !id || !name || !ACAP_EVENTS_DECLARATIONS) {
        LOG_WARN("ACAP_EVENTS_Add_Event: Invalid input\n");
        return 0;
    }

    set = ax_event_key_value_set_new();
    ax_event_key_value_set_add_key_value(set, "topic0", "tnsaxis", "CameraApplicationPlatform", AX_VALUE_TYPE_STRING, NULL);
    ax_event_key_value_set_add_key_value(set, "topic1", "tnsaxis", ACAP_EVENTS_PACKAGE, AX_VALUE_TYPE_STRING, NULL);
    ax_event_key_value_set_add_nice_names(set, "topic1", "tnsaxis", ACAP_EVENTS_PACKAGE, ACAP_EVENTS_APPNAME, NULL);
    ax_event_key_value_set_add_key_value(set, "topic2", "tnsaxis", id, AX_VALUE_TYPE_STRING, NULL);
    ax_event_key_value_set_add_nice_names(set, "topic2", "tnsaxis", id, name, NULL);

    int ax = 0;
    if (state) {
        ax_event_key_value_set_add_key_value(set, "state", NULL, &dummy_value, AX_VALUE_TYPE_BOOL, NULL);
        ax_event_key_value_set_mark_as_data(set, "state", NULL, NULL);
        ax = ax_event_handler_declare(ACAP_EVENTS_HANDLER, set, 0, &declarationID, NULL, NULL, NULL);
    } else {
        ax_event_key_value_set_add_key_value(set, "value", NULL, &dummy_value, AX_VALUE_TYPE_INT, NULL);
        ax_event_key_value_set_mark_as_data(set, "value", NULL, NULL);
        ax = ax_event_handler_declare(ACAP_EVENTS_HANDLER, set, 1, &declarationID, NULL, NULL, NULL);
    }

    if (!ax) {
        LOG_WARN("Error declaring event\n");
        ax_event_key_value_set_free(set);
        return 0;
    }
    LOG_TRACE("%s: %s %s %s\n", __func__, id, name, state ? "Stateful" : "Stateless");

    cJSON_AddNumberToObject(ACAP_EVENTS_DECLARATIONS, id, declarationID);
    ax_event_key_value_set_free(set);
    return declarationID;
}

int ACAP_EVENTS_Remove_Event(const char* id) {
    LOG_TRACE("%s: %s", __func__, id);
    if (!ACAP_EVENTS_HANDLER || !id || !ACAP_EVENTS_DECLARATIONS) {
        LOG_TRACE("ACAP_EVENTS_Remove_Event: Invalid input\n");
        return 0;
    }

    cJSON* event = cJSON_DetachItemFromObject(ACAP_EVENTS_DECLARATIONS, id);
    if (!event) {
        LOG_WARN("Error removing event %s. Event not found\n", id);
        return 0;
    }

    ax_event_handler_undeclare(ACAP_EVENTS_HANDLER, event->valueint, NULL);
    cJSON_Delete(event);
    return 1;
}

int ACAP_EVENTS_Fire(const char* id) {
    GError* error = NULL;
    LOG_TRACE("%s: %s\n", __func__, id);

    if (!ACAP_EVENTS_DECLARATIONS || !id) {
        LOG_WARN("ACAP_EVENTS_Fire: Error send event\n");
        return 0;
    }

    cJSON* event = cJSON_GetObjectItem(ACAP_EVENTS_DECLARATIONS, id);
    if (!event) {
        LOG_WARN("%s: Event %s not found\n", __func__, id);
        return 0;
    }

    AXEventKeyValueSet* set = ax_event_key_value_set_new();
    guint value = 1;
    if (!ax_event_key_value_set_add_key_value(set, "value", NULL, &value, AX_VALUE_TYPE_INT, &error)) {
        ax_event_key_value_set_free(set);
        LOG_WARN("%s: %s error %s\n", __func__, id, error->message);
        g_error_free(error);
        return 0;
    }

    AXEvent* axEvent = ax_event_new2(set, NULL);
    ax_event_key_value_set_free(set);

    if (!ax_event_handler_send_event(ACAP_EVENTS_HANDLER, event->valueint, axEvent, &error)) {
        LOG_WARN("%s: Could not send event %s %s\n", __func__, id, error->message);
        ax_event_free(axEvent);
        g_error_free(error);
        return 0;
    }
    ax_event_free(axEvent);
    LOG_TRACE("%s: %s fired\n", __func__, id);
    return 1;
}

int ACAP_EVENTS_Fire_State(const char* id, int value) {
    if (value && ACAP_STATUS_Bool("events", id))
        return 1;
    if (!value && !ACAP_STATUS_Bool("events", id))
        return 1;

    LOG_TRACE("%s: %s %d\n", __func__, id, value);

    if (!ACAP_EVENTS_DECLARATIONS || !id) {
        LOG_WARN("ACAP_EVENTS_Fire_State: Error send event\n");
        return 0;
    }

    cJSON* event = cJSON_GetObjectItem(ACAP_EVENTS_DECLARATIONS, id);
    if (!event) {
        LOG_WARN("Error sending event %s. Event not found\n", id);
        return 0;
    }

    AXEventKeyValueSet* set = ax_event_key_value_set_new();
    if (!ax_event_key_value_set_add_key_value(set, "state", NULL, &value, AX_VALUE_TYPE_BOOL, NULL)) {
        ax_event_key_value_set_free(set);
        LOG_WARN("ACAP_EVENTS_Fire_State: Could not send event %s\n", id);
        return 0;
    }

    AXEvent* axEvent = ax_event_new2(set, NULL);
    ax_event_key_value_set_free(set);

    if (!ax_event_handler_send_event(ACAP_EVENTS_HANDLER, event->valueint, axEvent, NULL)) {
        LOG_WARN("ACAP_EVENTS_Fire_State: Could not send event %s\n", id);
        ax_event_free(axEvent);
        return 0;
    }
    ax_event_free(axEvent);
    ACAP_STATUS_SetBool("events", id, value);
    LOG_TRACE("ACAP_EVENTS_Fire_State: %s %d fired\n", id, value);
    return 1;
}

int ACAP_EVENTS_Fire_JSON(const char* Id, cJSON* data) {
    AXEventKeyValueSet* set = NULL;
    int boolValue;
    GError* error = NULL;

    if (!data) {
        LOG_WARN("%s: Invalid data", __func__);
        return 0;
    }

    cJSON* event = cJSON_GetObjectItem(ACAP_EVENTS_DECLARATIONS, Id);
    if (!event) {
        LOG_WARN("%s: Error sending event %s. Event not found\n", __func__, Id);
        return 0;
    }

    set = ax_event_key_value_set_new();
    int success = 0;
    cJSON* property = data->child;
    while (property) {
        if (property->type == cJSON_True) {
            boolValue = 1;
            success = ax_event_key_value_set_add_key_value(set, property->string, NULL, &boolValue, AX_VALUE_TYPE_BOOL, NULL);
        }
        if (property->type == cJSON_False) {
            boolValue = 0;
            success = ax_event_key_value_set_add_key_value(set, property->string, NULL, &boolValue, AX_VALUE_TYPE_BOOL, NULL);
        }
        if (property->type == cJSON_String) {
            success = ax_event_key_value_set_add_key_value(set, property->string, NULL, property->valuestring, AX_VALUE_TYPE_STRING, NULL);
        }
        if (property->type == cJSON_Number) {
            success = ax_event_key_value_set_add_key_value(set, property->string, NULL, &property->valuedouble, AX_VALUE_TYPE_DOUBLE, NULL);
        }
        if (!success)
            LOG_WARN("%s: Unable to add property\n", __func__);
        property = property->next;
    }

    AXEvent* axEvent = ax_event_new2(set, NULL);
    success = ax_event_handler_send_event(ACAP_EVENTS_HANDLER, event->valueint, axEvent, &error);
    ax_event_key_value_set_free(set);
    ax_event_free(axEvent);
    if (!success) {
        LOG_WARN("%s: Could not send event %s id = %d %s\n", __func__, Id, event->valueint, error->message);
        g_error_free(error);
        return 0;
    }
    return 1;
}

int ACAP_EVENTS_Add_Event_JSON(cJSON* event) {
    AXEventKeyValueSet* set = NULL;
    GError* error = NULL;
    guint declarationID;
    int success = 0;
    set = ax_event_key_value_set_new();

    char* eventID   = cJSON_GetObjectItem(event, "id")   ? cJSON_GetObjectItem(event, "id")->valuestring   : NULL;
    char* eventName = cJSON_GetObjectItem(event, "name") ? cJSON_GetObjectItem(event, "name")->valuestring : NULL;

    if (!eventID) {
        LOG_WARN("%s: Invalid id\n", __func__);
        ax_event_key_value_set_free(set);
        return 0;
    }

    ax_event_key_value_set_add_key_value(set, "topic0", "tnsaxis", "CameraApplicationPlatform", AX_VALUE_TYPE_STRING, NULL);
    ax_event_key_value_set_add_key_value(set, "topic1", "tnsaxis", ACAP_EVENTS_PACKAGE, AX_VALUE_TYPE_STRING, NULL);
    ax_event_key_value_set_add_nice_names(set, "topic1", "tnsaxis", ACAP_EVENTS_PACKAGE, ACAP_EVENTS_APPNAME, NULL);
    ax_event_key_value_set_add_key_value(set, "topic2", "tnsaxis", eventID, AX_VALUE_TYPE_STRING, NULL);
    if (eventName)
        ax_event_key_value_set_add_nice_names(set, "topic2", "tnsaxis", eventID, eventName, NULL);

    if (cJSON_GetObjectItem(event, "show") && cJSON_GetObjectItem(event, "show")->type == cJSON_False)
        ax_event_key_value_set_mark_as_user_defined(set, eventID, "tnsaxis", "isApplicationData", NULL);

    int defaultInt = 0;
    double defaultDouble = 0;
    char defaultString[] = "";

    cJSON* source = cJSON_GetObjectItem(event, "source") ? cJSON_GetObjectItem(event, "source")->child : NULL;
    while (source) {
        cJSON* property = source->child;
        if (property) {
            if (strcmp(property->valuestring, "string") == 0)
                ax_event_key_value_set_add_key_value(set, property->string, NULL, &defaultString, AX_VALUE_TYPE_STRING, NULL);
            if (strcmp(property->valuestring, "int") == 0)
                ax_event_key_value_set_add_key_value(set, property->string, NULL, &defaultInt, AX_VALUE_TYPE_INT, NULL);
            if (strcmp(property->valuestring, "bool") == 0)
                ax_event_key_value_set_add_key_value(set, property->string, NULL, &defaultInt, AX_VALUE_TYPE_BOOL, NULL);
            ax_event_key_value_set_mark_as_source(set, property->string, NULL, NULL);
            LOG_TRACE("%s: %s Source %s %s\n", __func__, eventID, property->string, property->valuestring);
        }
        source = source->next;
    }

    cJSON* dataItem = cJSON_GetObjectItem(event, "data") ? cJSON_GetObjectItem(event, "data")->child : NULL;
    int propertyCounter = 0;
    while (dataItem) {
        cJSON* property = dataItem->child;
        if (property) {
            propertyCounter++;
            if (strcmp(property->valuestring, "string") == 0) {
                if (!ax_event_key_value_set_add_key_value(set, property->string, NULL, &defaultString, AX_VALUE_TYPE_STRING, &error)) {
                    LOG_WARN("%s: Unable to add string %s %s\n", __func__, property->string, error->message);
                    g_error_free(error);
                    error = NULL;
                }
            }
            if (strcmp(property->valuestring, "int") == 0)
                ax_event_key_value_set_add_key_value(set, property->string, NULL, &defaultInt, AX_VALUE_TYPE_INT, NULL);
            if (strcmp(property->valuestring, "double") == 0)
                ax_event_key_value_set_add_key_value(set, property->string, NULL, &defaultDouble, AX_VALUE_TYPE_DOUBLE, NULL);
            if (strcmp(property->valuestring, "bool") == 0)
                ax_event_key_value_set_add_key_value(set, property->string, NULL, &defaultInt, AX_VALUE_TYPE_BOOL, NULL);
            ax_event_key_value_set_mark_as_data(set, property->string, NULL, NULL);
            LOG_TRACE("%s: %s Data %s %s\n", __func__, eventID, property->string, property->valuestring);
        }
        dataItem = dataItem->next;
    }

    if (propertyCounter == 0)
        ax_event_key_value_set_add_key_value(set, "value", NULL, &defaultInt, AX_VALUE_TYPE_INT, NULL);

    if (cJSON_GetObjectItem(event, "state") && cJSON_GetObjectItem(event, "state")->type == cJSON_True) {
        ax_event_key_value_set_add_key_value(set, "state", NULL, &defaultInt, AX_VALUE_TYPE_BOOL, NULL);
        ax_event_key_value_set_mark_as_data(set, "state", NULL, NULL);
        LOG_TRACE("%s: %s is stateful\n", __func__, eventID);
        success = ax_event_handler_declare(ACAP_EVENTS_HANDLER, set, 0, &declarationID, NULL, NULL, NULL);
    } else {
        success = ax_event_handler_declare(ACAP_EVENTS_HANDLER, set, 1, &declarationID, NULL, NULL, NULL);
    }
    LOG_TRACE("%s: %s ID = %d\n", __func__, eventID, declarationID);
    if (!success) {
        ax_event_key_value_set_free(set);
        LOG_WARN("Unable to register event %s\n", eventID);
        return 0;
    }
    cJSON_AddNumberToObject(ACAP_EVENTS_DECLARATIONS, eventID, declarationID);
    ax_event_key_value_set_free(set);
    return declarationID;
}

/*=====================================================
 * VAPIX API
 *=====================================================*/

static char* VAPIX_Credentials = NULL;
static CURL* VAPIX_CURL = NULL;
static pthread_mutex_t vapix_mutex = PTHREAD_MUTEX_INITIALIZER;

static size_t append_to_buffer_callback(char* ptr, size_t size, size_t nmemb, void* userdata) {
    size_t processed_bytes = size * nmemb;
    char** response_buffer = (char**)userdata;

    size_t current_length = *response_buffer ? strlen(*response_buffer) : 0;
    char* new_buffer = realloc(*response_buffer, current_length + processed_bytes + 1);
    if (!new_buffer) {
        LOG_WARN("%s: Memory allocation failed", __func__);
        return 0;
    }

    memcpy(new_buffer + current_length, ptr, processed_bytes);
    new_buffer[current_length + processed_bytes] = '\0';
    *response_buffer = new_buffer;
    return processed_bytes;
}

char* ACAP_VAPIX_Get(const char* endpoint) {
    if (!VAPIX_Credentials || !VAPIX_CURL || !endpoint) {
        LOG_WARN("%s: Invalid input\n", __func__);
        return NULL;
    }

    char* response = NULL;
    size_t url_size = strlen("http://127.0.0.12/axis-cgi/") + strlen(endpoint) + 1;
    char* url = malloc(url_size);
    if (!url) {
        LOG_WARN("%s: Memory allocation failed", __func__);
        return NULL;
    }
    snprintf(url, url_size, "http://127.0.0.12/axis-cgi/%s", endpoint);

    pthread_mutex_lock(&vapix_mutex);
    curl_easy_setopt(VAPIX_CURL, CURLOPT_URL, url);
    curl_easy_setopt(VAPIX_CURL, CURLOPT_USERPWD, VAPIX_Credentials);
    curl_easy_setopt(VAPIX_CURL, CURLOPT_HTTPAUTH, CURLAUTH_BASIC);
    curl_easy_setopt(VAPIX_CURL, CURLOPT_HTTPGET, 1L);
    curl_easy_setopt(VAPIX_CURL, CURLOPT_WRITEFUNCTION, append_to_buffer_callback);
    curl_easy_setopt(VAPIX_CURL, CURLOPT_WRITEDATA, &response);

    CURLcode res = curl_easy_perform(VAPIX_CURL);
    pthread_mutex_unlock(&vapix_mutex);
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
    return response;
}

char* ACAP_VAPIX_Post(const char* endpoint, const char* request) {
    if (!VAPIX_Credentials || !VAPIX_CURL || !endpoint || !request) {
        LOG_WARN("%s: Invalid input\n", __func__);
        return NULL;
    }

    LOG_TRACE("%s: %s %s\n", __func__, endpoint, request);

    char* response = NULL;
    size_t url_size = strlen("http://127.0.0.12/axis-cgi/") + strlen(endpoint) + 1;
    char* url = malloc(url_size);
    if (!url) {
        LOG_WARN("%s: Memory allocation failed", __func__);
        return NULL;
    }
    snprintf(url, url_size, "http://127.0.0.12/axis-cgi/%s", endpoint);

    pthread_mutex_lock(&vapix_mutex);
    curl_easy_setopt(VAPIX_CURL, CURLOPT_URL, url);
    curl_easy_setopt(VAPIX_CURL, CURLOPT_USERPWD, VAPIX_Credentials);
    curl_easy_setopt(VAPIX_CURL, CURLOPT_HTTPAUTH, CURLAUTH_BASIC);
    curl_easy_setopt(VAPIX_CURL, CURLOPT_POSTFIELDS, request);
    curl_easy_setopt(VAPIX_CURL, CURLOPT_WRITEFUNCTION, append_to_buffer_callback);
    curl_easy_setopt(VAPIX_CURL, CURLOPT_WRITEDATA, &response);

    CURLcode res = curl_easy_perform(VAPIX_CURL);
    pthread_mutex_unlock(&vapix_mutex);
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
    return response;
}

static char* parse_credentials(GVariant* result) {
    char* credentials_string = NULL;
    char* id = NULL;
    char* password = NULL;

    g_variant_get(result, "(&s)", &credentials_string);
    if (sscanf(credentials_string, "%m[^:]:%ms", &id, &password) != 2) {
        LOG_WARN("%s: Error parsing credential string %s", __func__, credentials_string);
        return NULL;
    }
    char* credentials = g_strdup_printf("%s:%s", id, password);
    free(id);
    free(password);
    LOG_TRACE("%s: %s\n", __func__, credentials);
    return credentials;
}

static char* retrieve_vapix_credentials(const char* username) {
    GError* error = NULL;
    GDBusConnection* connection = g_bus_get_sync(G_BUS_TYPE_SYSTEM, NULL, &error);
    if (!connection) {
        LOG_TRACE("%s: Error connecting to D-Bus: %s", __func__, error->message);
        return NULL;
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
        LOG_WARN("%s: Error invoking D-Bus method: %s", __func__, error->message);
        g_object_unref(connection);
        return NULL;
    }

    char* credentials = parse_credentials(result);
    g_variant_unref(result);
    g_object_unref(connection);
    return credentials;
}

void ACAP_VAPIX_Init(void) {
    LOG_TRACE("%s:\n", __func__);
    VAPIX_CURL = curl_easy_init();
    VAPIX_Credentials = retrieve_vapix_credentials("acap");
}

/*=====================================================
 * Cleanup
 *=====================================================*/

void ACAP_Cleanup(void) {
    LOG_TRACE("%s:", __func__);

    ACAP_HTTP_Cleanup();

    if (ACAP_EVENTS_HANDLER) {
        ax_event_handler_free(ACAP_EVENTS_HANDLER);
        ACAP_EVENTS_HANDLER = NULL;
    }
    if (ACAP_EVENTS_SUBSCRIPTIONS) {
        cJSON_Delete(ACAP_EVENTS_SUBSCRIPTIONS);
        ACAP_EVENTS_SUBSCRIPTIONS = NULL;
    }
    if (ACAP_EVENTS_DECLARATIONS) {
        cJSON_Delete(ACAP_EVENTS_DECLARATIONS);
        ACAP_EVENTS_DECLARATIONS = NULL;
    }

    status_container = NULL;
    ACAP_DEVICE_Container = NULL;

    if (app) {
        cJSON_Delete(app);
        app = NULL;
    }

    if (VAPIX_CURL) {
        curl_easy_cleanup(VAPIX_CURL);
        VAPIX_CURL = NULL;
    }
    if (VAPIX_Credentials) {
        g_free(VAPIX_Credentials);
        VAPIX_Credentials = NULL;
    }

    http_node_count = 0;
    ACAP_UpdateCallback = NULL;
}

/*=====================================================
 * Helper Functions
 *=====================================================*/

cJSON* SplitString(const char* input, const char* delimiter) {
    if (!input || !delimiter)
        return NULL;

    cJSON* json_array = cJSON_CreateArray();
    if (!json_array) return NULL;

    char* input_copy = strdup(input);
    if (!input_copy) {
        cJSON_Delete(json_array);
        return NULL;
    }

    /* Remove trailing newline */
    size_t len = strlen(input_copy);
    if (len > 0 && input_copy[len - 1] == '\n')
        input_copy[len - 1] = '\0';

    char* token = strtok(input_copy, delimiter);
    while (token != NULL) {
        cJSON* json_token = cJSON_CreateString(token);
        if (!json_token) {
            free(input_copy);
            cJSON_Delete(json_array);
            return NULL;
        }
        cJSON_AddItemToArray(json_array, json_token);
        token = strtok(NULL, delimiter);
    }

    free(input_copy);
    return json_array;
}
