/*
 * Copyright (c) 2024 Fred Juhlin
 * MIT License - See LICENSE file for details
 */

#ifndef _ACAP_H_
#define _ACAP_H_

#include <glib.h>
#include "fcgi_stdio.h"
#include "cJSON.h"

#ifdef __cplusplus
extern "C" {
#endif

// Constants
#define ACAP_MAX_HTTP_NODES 32
#define ACAP_MAX_PATH_LENGTH 128
#define ACAP_MAX_PACKAGE_NAME 30
#define ACAP_MAX_BUFFER_SIZE 4096


// Return types
typedef enum {
    ACAP_SUCCESS = 0,
    ACAP_ERROR_INIT = -1,
    ACAP_ERROR_PARAM = -2,
    ACAP_ERROR_MEMORY = -3,
    ACAP_ERROR_IO = -4,
    ACAP_ERROR_HTTP = -5
} ACAP_Status;

/*-----------------------------------------------------
 * Core Types and Structures
 *-----------------------------------------------------*/
typedef void (*ACAP_Config_Update)(const char* service, cJSON* event );
typedef void (*ACAP_EVENTS_Callback)(cJSON* event, void* user_data);

// HTTP Request/Response structures
typedef struct {
    FCGX_Request* request;
    const char* postData;    // Will be NULL for GET requests
    size_t postDataLength;
    const char* method;      // Request method (GET, POST, etc.)
    const char* contentType; // Content-Type header
    const char* queryString; // Raw query string
} ACAP_HTTP_Request_DATA;

typedef ACAP_HTTP_Request_DATA* ACAP_HTTP_Request;
typedef FCGX_Request* ACAP_HTTP_Response;
typedef void (*ACAP_HTTP_Callback)(ACAP_HTTP_Response response, const ACAP_HTTP_Request request);

/*-----------------------------------------------------
 * Core Functions
 *-----------------------------------------------------*/
// Initialization and configuration
cJSON*		ACAP(const char* package, ACAP_Config_Update updateCallback);
const char* ACAP_Name(void);
int 		ACAP_Set_Config(const char* service, cJSON* serviceSettings);
cJSON* 		ACAP_Get_Config(const char* service);
void		ACAP_Cleanup(void);

/*-----------------------------------------------------
 * HTTP Functions
 *-----------------------------------------------------*/
int 		ACAP_HTTP_Node(const char* nodename, ACAP_HTTP_Callback callback);

// HTTP Request helpers
const char* ACAP_HTTP_Get_Method(const ACAP_HTTP_Request request);
const char* ACAP_HTTP_Get_Content_Type(const ACAP_HTTP_Request request);
size_t 		ACAP_HTTP_Get_Content_Length(const ACAP_HTTP_Request request);
const char* ACAP_HTTP_Request_Param(const ACAP_HTTP_Request request, const char* param);
cJSON* 		ACAP_HTTP_Request_JSON(const ACAP_HTTP_Request request, const char* param);

// HTTP Response helpers
int 		ACAP_HTTP_Header_XML(ACAP_HTTP_Response response);
int 		ACAP_HTTP_Header_JSON(ACAP_HTTP_Response response);
int 		ACAP_HTTP_Header_TEXT(ACAP_HTTP_Response response);
int 		ACAP_HTTP_Header_FILE(ACAP_HTTP_Response response, const char* filename, 
                         const char* contenttype, unsigned filelength);

// HTTP Response functions
int 		ACAP_HTTP_Respond_String(ACAP_HTTP_Response response, const char* fmt, ...);
int 		ACAP_HTTP_Respond_JSON(ACAP_HTTP_Response response, cJSON* object);
int 		ACAP_HTTP_Respond_Data(ACAP_HTTP_Response response, size_t count, const void* data);
int 		ACAP_HTTP_Respond_Error(ACAP_HTTP_Response response, int code, const char* message);
int 		ACAP_HTTP_Respond_Text(ACAP_HTTP_Response response, const char* message);

/*-----------------------------------------------------
	EVENTS
  -----------------------------------------------------*/

int		ACAP_EVENTS_Add_Event( const char* Id, const char* NiceName, int state );
int		ACAP_EVENTS_Add_Event_JSON( cJSON* event );
int		ACAP_EVENTS_Remove_Event( const char* Id );
int		ACAP_EVENTS_Fire_State( const char* Id, int value );
int		ACAP_EVENTS_Fire( const char* Id );
int		ACAP_EVENTS_Fire_JSON( const char* Id, cJSON* data );
int		ACAP_EVENTS_SetCallback( ACAP_EVENTS_Callback callback );
int		ACAP_EVENTS_Subscribe( cJSON* eventDeclaration, void* user_data );
int		ACAP_EVENTS_Unsubscribe(int id);

/*-----------------------------------------------------
 * File Operations
 *-----------------------------------------------------*/
const char* ACAP_FILE_AppPath(void);
FILE* 		ACAP_FILE_Open(const char* filepath, const char* mode);
int 		ACAP_FILE_Delete(const char* filepath);
cJSON* 		ACAP_FILE_Read(const char* filepath);
int 		ACAP_FILE_Write(const char* filepath, cJSON* object);
int 		ACAP_FILE_WriteData(const char* filepath, const char* data);
int 		ACAP_FILE_Exists(const char* filepath);

/*-----------------------------------------------------
 * Device Information
 *-----------------------------------------------------*/
double		ACAP_DEVICE_Longitude();
double		ACAP_DEVICE_Latitude();

int			ACAP_DEVICE_Set_Location( double lat, double lon);
//Properties: serial, model, platform, chip, firmware, aspect, 
const char* ACAP_DEVICE_Prop(const char* name);
int 		ACAP_DEVICE_Prop_Int(const char* name);
cJSON* 		ACAP_DEVICE_JSON(const char* name); //location, 
int 		ACAP_DEVICE_Seconds_Since_Midnight(void);
double 		ACAP_DEVICE_Timestamp(void);
const char* ACAP_DEVICE_Local_Time(void);
const char* ACAP_DEVICE_ISOTime(void);
const char* ACAP_DEVICE_Date(void);
const char* ACAP_DEVICE_Time(void);
double 		ACAP_DEVICE_Uptime(void);
double 		ACAP_DEVICE_CPU_Average(void);
double 		ACAP_DEVICE_Network_Average(void);

/*-----------------------------------------------------
 * Status Management
 *-----------------------------------------------------*/
 
cJSON* 		ACAP_STATUS_Group(const char* name);
int 		ACAP_STATUS_Bool(const char* group, const char* name);
int 		ACAP_STATUS_Int(const char* group, const char* name);
double 		ACAP_STATUS_Double(const char* group, const char* name);
char* 		ACAP_STATUS_String(const char* group, const char* name);
cJSON* 		ACAP_STATUS_Object(const char* group, const char* name);

// Status setters
void		ACAP_STATUS_SetBool(const char* group, const char* name, int state);
void		ACAP_STATUS_SetNumber(const char* group, const char* name, double value);
void		ACAP_STATUS_SetString(const char* group, const char* name, const char* string);
void		ACAP_STATUS_SetObject(const char* group, const char* name, cJSON* data);
void		ACAP_STATUS_SetNull(const char* group, const char* name);

/*-----------------------------------------------------
 * VAPIX
 *-----------------------------------------------------*/
char*		ACAP_VAPIX_Get(const char *request);
char*		ACAP_VAPIX_Post(const char *request, const char* body );


#ifdef __cplusplus
}
#endif

#endif // _ACAP_H_
