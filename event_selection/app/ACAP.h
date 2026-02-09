/*
 * ACAP SDK wrapper for version 12
 * Copyright (c) 2025 Fred Juhlin
 * MIT License - See LICENSE file for details
 * Version 3.7
 * 
 * This header provides a simplified wrapper around the AXIS ACAP SDK,
 * offering easy-to-use interfaces for HTTP handling, event management,
 * device information, file operations, status management, and VAPIX API access.
 * 
 * MEMORY OWNERSHIP CONVENTIONS:
 * - Functions returning cJSON* from ACAP_Get_Config(), ACAP_STATUS_*() getters,
 *   and ACAP_DEVICE_*() return pointers to internally managed objects.
 *   DO NOT call cJSON_Delete() on these pointers.
 * - Functions like ACAP_FILE_Read() return newly allocated objects that the
 *   caller MUST free with cJSON_Delete().
 * - ACAP_VAPIX_Get/Post return dynamically allocated strings that the caller
 *   MUST free().
 * - ACAP_HTTP_Request_Param returns dynamically allocated strings that the
 *   caller MUST free().
 */

#ifndef _ACAP_H_
#define _ACAP_H_

#include <glib.h>
#include "fcgi_stdio.h"
#include "cJSON.h"

#ifdef __cplusplus
extern "C" {
#endif

/*-----------------------------------------------------
 * Constants
 *-----------------------------------------------------*/
#define ACAP_MAX_HTTP_NODES 32      /**< Maximum number of HTTP endpoint handlers */
#define ACAP_MAX_PATH_LENGTH 128    /**< Maximum file path length */
#define ACAP_MAX_PACKAGE_NAME 30    /**< Maximum ACAP package name length */
#define ACAP_MAX_BUFFER_SIZE 4096   /**< Maximum buffer size for HTTP POST data */

/*-----------------------------------------------------
 * Return Types
 *-----------------------------------------------------*/
/**
 * @brief ACAP operation status codes
 */
typedef enum {
    ACAP_SUCCESS = 0,        /**< Operation completed successfully */
    ACAP_ERROR_INIT = -1,    /**< Initialization error */
    ACAP_ERROR_PARAM = -2,   /**< Invalid parameter */
    ACAP_ERROR_MEMORY = -3,  /**< Memory allocation error */
    ACAP_ERROR_IO = -4,      /**< I/O error */
    ACAP_ERROR_HTTP = -5     /**< HTTP error */
} ACAP_Status;

/*-----------------------------------------------------
 * Core Types and Structures
 *-----------------------------------------------------*/

/**
 * @brief Callback function type for configuration updates.
 * @param service The name of the service/setting that was updated (e.g., "publish", "scene")
 * @param data The updated configuration data (internally managed, do not delete)
 * 
 * Called when settings are modified via HTTP POST to /local/<package>/settings
 * or during initialization for each setting in settings.json.
 */
typedef void (*ACAP_Config_Update)(const char* service, cJSON* data);

/**
 * @brief Callback function type for subscribed events.
 * @param event The event data (caller must NOT delete - handled internally)
 * @param user_data User-provided context from ACAP_EVENTS_Subscribe
 */
typedef void (*ACAP_EVENTS_Callback)(cJSON* event, void* user_data);

/**
 * @brief HTTP Request data structure containing parsed request information.
 */
typedef struct {
    FCGX_Request* request;      /**< Underlying FastCGI request handle */
    const char* postData;       /**< POST body data (NULL for GET requests) */
    size_t postDataLength;      /**< Length of POST data in bytes */
    const char* method;         /**< HTTP method: "GET", "POST", etc. */
    const char* contentType;    /**< Content-Type header value */
    const char* queryString;    /**< Raw query string from URL */
} ACAP_HTTP_Request_DATA;

typedef ACAP_HTTP_Request_DATA* ACAP_HTTP_Request;
typedef FCGX_Request* ACAP_HTTP_Response;

/**
 * @brief HTTP endpoint callback function type.
 * @param response The response object to write data to
 * @param request The request object containing method, params, and POST data
 */
typedef void (*ACAP_HTTP_Callback)(ACAP_HTTP_Response response, const ACAP_HTTP_Request request);

/*=====================================================
 * CORE FUNCTIONS
 *=====================================================*/

/**
 * @brief Initialize the ACAP framework.
 * 
 * This function MUST be called first before any other ACAP functions.
 * It initializes file system paths, loads manifest.json and settings,
 * starts the HTTP server thread, and sets up event handling.
 * 
 * @param package The ACAP package name (must match manifest.json appName)
 * @param updateCallback Optional callback invoked when settings change (can be NULL)
 * @return Pointer to the settings cJSON object (internally managed, do NOT delete).
 *         Returns NULL on initialization failure.
 * 
 * @note The callback is invoked for each setting during initialization and
 *       whenever settings are updated via HTTP POST.
 * 
 * Example:
 * @code
 * void onSettingsUpdate(const char* service, cJSON* data) {
 *     if (strcmp(service, "publish") == 0) {
 *         // Handle publish settings change
 *     }
 * }
 * 
 * int main(void) {
 *     cJSON* settings = ACAP("MyApp", onSettingsUpdate);
 *     if (!settings) return 1;
 *     // ... application code ...
 *     ACAP_Cleanup();
 *     return 0;
 * }
 * @endcode
 */
cJSON* ACAP(const char* package, ACAP_Config_Update updateCallback);

/**
 * @brief Get the ACAP package name.
 * @return The package name string (internally managed, do NOT free)
 */
const char* ACAP_Name(void);

/**
 * @brief Register a configuration object for a service.
 * 
 * Adds a cJSON object to the global app configuration, making it
 * accessible via ACAP_Get_Config() and the /local/<package>/app HTTP endpoint.
 * 
 * @param service The service name (e.g., "mqtt", "status")
 * @param serviceSettings The cJSON object to register (ownership transfers to ACAP)
 * @return 1 on success, 1 if already registered (no-op)
 * 
 * @note Once registered, the object should NOT be deleted by the caller.
 */
int ACAP_Set_Config(const char* service, cJSON* serviceSettings);

/**
 * @brief Retrieve a registered configuration object.
 * 
 * @param service The service name to retrieve (e.g., "settings", "mqtt", "device")
 * @return Pointer to the cJSON object, or NULL if not found.
 * 
 * @warning DO NOT call cJSON_Delete() on the returned pointer.
 *          The object is managed internally and persists for the application lifetime.
 *          If you need to modify a copy, use cJSON_Duplicate() first.
 * 
 * Common service names:
 * - "settings" - Application settings from settings/settings.json
 * - "manifest" - Package manifest.json
 * - "device"   - Device information (serial, model, firmware, etc.)
 * - "status"   - Runtime status information
 * - "mqtt"     - MQTT configuration (if registered)
 */
cJSON* ACAP_Get_Config(const char* service);

/**
 * @brief Clean up all ACAP resources and stop background threads.
 * 
 * Call this before application exit to properly release resources:
 * - Stops HTTP server thread
 * - Frees all cJSON configuration objects
 * - Cleans up event handlers
 * 
 * After calling this function, no other ACAP functions should be called.
 */
void ACAP_Cleanup(void);

/*=====================================================
 * HTTP FUNCTIONS
 *=====================================================*/

/**
 * @brief Register an HTTP endpoint handler.
 * 
 * Creates an HTTP endpoint at /local/<package>/<nodename> that will
 * invoke the callback when accessed.
 * 
 * @param nodename The endpoint path (without /local/<package>/ prefix)
 * @param callback The function to handle requests to this endpoint
 * @return 1 on success, 0 on failure (max nodes reached or duplicate path)
 * 
 * Example:
 * @code
 * void my_handler(ACAP_HTTP_Response response, ACAP_HTTP_Request request) {
 *     const char* method = ACAP_HTTP_Get_Method(request);
 *     if (strcmp(method, "GET") == 0) {
 *         cJSON* data = cJSON_CreateObject();
 *         cJSON_AddStringToObject(data, "status", "ok");
 *         ACAP_HTTP_Respond_JSON(response, data);
 *         cJSON_Delete(data);
 *     }
 * }
 * 
 * ACAP_HTTP_Node("myendpoint", my_handler);
 * // Accessible at: http://camera/local/MyApp/myendpoint
 * @endcode
 */
int ACAP_HTTP_Node(const char* nodename, ACAP_HTTP_Callback callback);

/**
 * @brief Get the HTTP request method.
 * @param request The HTTP request object
 * @return "GET", "POST", "PUT", "DELETE", etc., or NULL on error
 */
const char* ACAP_HTTP_Get_Method(const ACAP_HTTP_Request request);

/**
 * @brief Get the Content-Type header from the request.
 * @param request The HTTP request object
 * @return Content-Type string (e.g., "application/json") or NULL
 */
const char* ACAP_HTTP_Get_Content_Type(const ACAP_HTTP_Request request);

/**
 * @brief Get the Content-Length of the request body.
 * @param request The HTTP request object
 * @return Content length in bytes, or 0 if not available
 */
size_t ACAP_HTTP_Get_Content_Length(const ACAP_HTTP_Request request);

/**
 * @brief Get a query string or form parameter value.
 * 
 * Searches for the parameter in POST form data first (for application/x-www-form-urlencoded),
 * then falls back to URL query string.
 * 
 * @param request The HTTP request object
 * @param param The parameter name to retrieve
 * @return The URL-decoded parameter value, or NULL if not found.
 * 
 * @warning The returned string is dynamically allocated. Caller MUST free() it.
 * 
 * Example:
 * @code
 * char* value = (char*)ACAP_HTTP_Request_Param(request, "id");
 * if (value) {
 *     // Use value...
 *     free(value);
 * }
 * @endcode
 */
const char* ACAP_HTTP_Request_Param(const ACAP_HTTP_Request request, const char* param);

/**
 * @brief Get a query parameter and parse it as JSON.
 * @param request The HTTP request object
 * @param param The parameter name
 * @return Parsed cJSON object (caller must cJSON_Delete), or NULL
 */
cJSON* ACAP_HTTP_Request_JSON(const ACAP_HTTP_Request request, const char* param);

/* HTTP Response Header Functions */

/**
 * @brief Write XML content-type header.
 * @param response The HTTP response object
 * @return 1 on success, 0 on failure
 */
int ACAP_HTTP_Header_XML(ACAP_HTTP_Response response);

/**
 * @brief Write JSON content-type header.
 * @param response The HTTP response object
 * @return 1 on success, 0 on failure
 */
int ACAP_HTTP_Header_JSON(ACAP_HTTP_Response response);

/**
 * @brief Write plain text content-type header.
 * @param response The HTTP response object
 * @return 1 on success, 0 on failure
 */
int ACAP_HTTP_Header_TEXT(ACAP_HTTP_Response response);

/**
 * @brief Write file download headers.
 * @param response The HTTP response object
 * @param filename The filename for Content-Disposition header
 * @param contenttype The MIME type (e.g., "application/octet-stream")
 * @param filelength The file size in bytes
 * @return 1 on success, 0 on failure
 */
int ACAP_HTTP_Header_FILE(ACAP_HTTP_Response response, const char* filename,
                          const char* contenttype, unsigned filelength);

/* HTTP Response Body Functions */

/**
 * @brief Write a formatted string to the response.
 * @param response The HTTP response object
 * @param fmt Printf-style format string
 * @param ... Format arguments
 * @return 1 on success, 0 on failure
 */
int ACAP_HTTP_Respond_String(ACAP_HTTP_Response response, const char* fmt, ...);

/**
 * @brief Send a JSON object as the response body.
 * 
 * Automatically sets Content-Type: application/json header.
 * 
 * @param response The HTTP response object
 * @param object The cJSON object to serialize and send (not consumed, caller still owns it)
 * @return 1 on success, 0 on failure
 */
int ACAP_HTTP_Respond_JSON(ACAP_HTTP_Response response, cJSON* object);

/**
 * @brief Write raw binary data to the response.
 * @param response The HTTP response object
 * @param count Number of bytes to write
 * @param data Pointer to the data buffer
 * @return 1 on success, 0 on failure
 */
int ACAP_HTTP_Respond_Data(ACAP_HTTP_Response response, size_t count, const void* data);

/**
 * @brief Send an HTTP error response.
 * @param response The HTTP response object
 * @param code HTTP status code (e.g., 400, 404, 500)
 * @param message Error message to include in the response body
 * @return 1 on success, 0 on failure
 */
int ACAP_HTTP_Respond_Error(ACAP_HTTP_Response response, int code, const char* message);

/**
 * @brief Send a plain text response.
 * 
 * Automatically sets Content-Type: text/plain header.
 * 
 * @param response The HTTP response object
 * @param message The text message to send
 * @return 1 on success, 0 on failure
 */
int ACAP_HTTP_Respond_Text(ACAP_HTTP_Response response, const char* message);

/*=====================================================
 * EVENT FUNCTIONS
 *=====================================================*/

/**
 * @brief Declare a simple stateful or stateless event.
 * 
 * Creates an AXIS event under CameraApplicationPlatform/<AppName>/<Id>
 * 
 * @param Id Event identifier (used in topic)
 * @param NiceName Human-readable event name shown in camera UI
 * @param state 1 for stateful event (has state property), 0 for stateless (has value property)
 * @return Declaration ID on success, 0 on failure
 * 
 * Example:
 * @code
 * // Stateful event (can be high/low)
 * ACAP_EVENTS_Add_Event("motion", "Motion Detected", 1);
 * ACAP_EVENTS_Fire_State("motion", 1);  // Set high
 * ACAP_EVENTS_Fire_State("motion", 0);  // Set low
 * 
 * // Stateless event (pulse)
 * ACAP_EVENTS_Add_Event("trigger", "Trigger Pulse", 0);
 * ACAP_EVENTS_Fire("trigger");
 * @endcode
 */
int ACAP_EVENTS_Add_Event(const char* Id, const char* NiceName, int state);

/**
 * @brief Declare an event from a JSON definition.
 * 
 * Allows declaring events with custom source and data properties.
 * 
 * @param event JSON object with event definition containing:
 *        - "id": Event identifier (required)
 *        - "name": Human-readable name
 *        - "state": true for stateful events
 *        - "source": Array of source properties
 *        - "data": Array of data properties
 * @return Declaration ID on success, 0 on failure
 */
int ACAP_EVENTS_Add_Event_JSON(cJSON* event);

/**
 * @brief Remove a declared event.
 * @param Id The event identifier to remove
 * @return 1 on success, 0 on failure
 */
int ACAP_EVENTS_Remove_Event(const char* Id);

/**
 * @brief Fire a stateful event (set state high or low).
 * 
 * Only fires if the state actually changes (debounced).
 * 
 * @param Id The event identifier
 * @param value 1 for high/active, 0 for low/inactive
 * @return 1 on success, 0 on failure
 */
int ACAP_EVENTS_Fire_State(const char* Id, int value);

/**
 * @brief Fire a stateless event (pulse).
 * @param Id The event identifier
 * @return 1 on success, 0 on failure
 */
int ACAP_EVENTS_Fire(const char* Id);

/**
 * @brief Fire an event with custom JSON data payload.
 * @param Id The event identifier
 * @param data JSON object containing the event data properties
 * @return 1 on success, 0 on failure
 */
int ACAP_EVENTS_Fire_JSON(const char* Id, cJSON* data);

/**
 * @brief Set the global event callback for subscribed events.
 * @param callback Function to call when subscribed events occur
 * @return 1 on success
 */
int ACAP_EVENTS_SetCallback(ACAP_EVENTS_Callback callback);

/**
 * @brief Subscribe to an event.
 * 
 * @param eventDeclaration JSON object defining the event to subscribe to:
 *        - "name": Description (required)
 *        - "topic0": {"namespace": "value"} (required)
 *        - "topic1", "topic2", "topic3": Optional additional topic levels
 * @param user_data User context passed to callback (can be NULL)
 * @return Subscription ID on success, 0 on failure
 * 
 * Example subscription declaration:
 * @code
 * {
 *   "name": "All Motion Events",
 *   "topic0": {"tnsaxis": "VideoAnalytics"},
 *   "topic1": {"tnsaxis": "MotionDetection"}
 * }
 * @endcode
 */
int ACAP_EVENTS_Subscribe(cJSON* eventDeclaration, void* user_data);

/**
 * @brief Unsubscribe from an event.
 * @param id Subscription ID from ACAP_EVENTS_Subscribe, or 0 to unsubscribe all
 * @return 1 on success
 */
int ACAP_EVENTS_Unsubscribe(int id);

/*=====================================================
 * FILE OPERATIONS
 *=====================================================*/

/**
 * @brief Get the application's installation directory path.
 * @return Path string ending with "/" (e.g., "/usr/local/packages/MyApp/")
 *         Internally managed, do NOT free.
 */
const char* ACAP_FILE_AppPath(void);

/**
 * @brief Open a file relative to the application directory.
 * @param filepath Relative path within the app directory
 * @param mode fopen mode string ("r", "w", "rb", etc.)
 * @return FILE pointer on success, NULL on failure
 * 
 * @note Caller must fclose() the returned file handle.
 */
FILE* ACAP_FILE_Open(const char* filepath, const char* mode);

/**
 * @brief Delete a file in the application directory.
 * @param filepath Relative path to the file
 * @return 1 on success, 0 on failure
 */
int ACAP_FILE_Delete(const char* filepath);

/**
 * @brief Read and parse a JSON file.
 * @param filepath Relative path to the JSON file
 * @return Parsed cJSON object on success, NULL on failure
 * 
 * @warning Caller MUST call cJSON_Delete() on the returned object.
 */
cJSON* ACAP_FILE_Read(const char* filepath);

/**
 * @brief Write a cJSON object to a file.
 * @param filepath Relative path for the output file
 * @param object The cJSON object to serialize
 * @return 1 on success, 0 on failure
 */
int ACAP_FILE_Write(const char* filepath, cJSON* object);

/**
 * @brief Write raw string data to a file.
 * @param filepath Relative path for the output file
 * @param data String data to write
 * @return 1 on success, 0 on failure
 */
int ACAP_FILE_WriteData(const char* filepath, const char* data);

/**
 * @brief Check if a file exists.
 * @param filepath Relative path to check
 * @return 1 if exists, 0 if not
 */
int ACAP_FILE_Exists(const char* filepath);

/*=====================================================
 * DEVICE INFORMATION
 *=====================================================*/

/**
 * @brief Get the device's configured longitude.
 * @return Longitude in decimal degrees, or 0 if not set
 */
double ACAP_DEVICE_Longitude(void);

/**
 * @brief Get the device's configured latitude.
 * @return Latitude in decimal degrees, or 0 if not set
 */
double ACAP_DEVICE_Latitude(void);

/**
 * @brief Set the device's geographic location.
 * @param lat Latitude in decimal degrees
 * @param lon Longitude in decimal degrees
 * @return 1 on success, 0 on failure
 */
int ACAP_DEVICE_Set_Location(double lat, double lon);

/**
 * @brief Get a device property as a string.
 * 
 * @param name Property name. Available properties:
 *        - "serial"   - Device serial number
 *        - "model"    - Product model number
 *        - "platform" - SoC platform name
 *        - "chip"     - CPU architecture
 *        - "firmware" - Firmware version
 *        - "aspect"   - Camera aspect ratio (e.g., "16:9")
 *        - "IPv4"     - Device IP address
 * @return Property value string (internally managed, do NOT free), or NULL
 */
const char* ACAP_DEVICE_Prop(const char* name);

/**
 * @brief Get a device property as an integer.
 * @param name Property name
 * @return Property value as integer, or 0 if not found/not numeric
 */
int ACAP_DEVICE_Prop_Int(const char* name);

/**
 * @brief Get a device property as a cJSON object.
 * @param name Property name (e.g., "location", "resolutions")
 * @return cJSON object (internally managed, do NOT delete), or NULL
 */
cJSON* ACAP_DEVICE_JSON(const char* name);

/**
 * @brief Get seconds elapsed since midnight (local time).
 * @return Seconds since midnight (0-86399)
 */
int ACAP_DEVICE_Seconds_Since_Midnight(void);

/**
 * @brief Get current timestamp in milliseconds since epoch.
 * @return Milliseconds since Unix epoch
 */
double ACAP_DEVICE_Timestamp(void);

/**
 * @brief Get current local time as formatted string.
 * @return Time string in "YYYY-MM-DD HH:MM:SS" format (static buffer, do NOT free)
 */
const char* ACAP_DEVICE_Local_Time(void);

/**
 * @brief Get current time in ISO 8601 format with timezone.
 * @return Time string in "YYYY-MM-DDTHH:MM:SS+ZZZZ" format (static buffer)
 */
const char* ACAP_DEVICE_ISOTime(void);

/**
 * @brief Get current date.
 * @return Date string in "YYYY-MM-DD" format (static buffer)
 */
const char* ACAP_DEVICE_Date(void);

/**
 * @brief Get current time.
 * @return Time string in "HH:MM:SS" format (static buffer)
 */
const char* ACAP_DEVICE_Time(void);

/**
 * @brief Get system uptime in seconds.
 * @return Uptime in seconds since boot
 */
double ACAP_DEVICE_Uptime(void);

/**
 * @brief Get 15-minute CPU load average.
 * @return CPU load as fraction (0.0 to 1.0+, where 1.0 = 100% of one core)
 */
double ACAP_DEVICE_CPU_Average(void);

/**
 * @brief Get network transmit rate in Kbps.
 * 
 * Calculates the average outbound network throughput since last call.
 * 
 * @return Transmit rate in kilobits per second
 */
double ACAP_DEVICE_Network_Average(void);

/*=====================================================
 * STATUS MANAGEMENT
 * 
 * Runtime status values organized in groups. Status data is exposed
 * via the /local/<package>/status HTTP endpoint and can be used
 * for monitoring application state.
 *=====================================================*/

/**
 * @brief Get or create a status group.
 * @param name Group name
 * @return cJSON object for the group (internally managed, do NOT delete)
 */
cJSON* ACAP_STATUS_Group(const char* name);

/**
 * @brief Get a boolean status value.
 * @param group Group name
 * @param name Property name
 * @return 1 if true, 0 if false or not found
 */
int ACAP_STATUS_Bool(const char* group, const char* name);

/**
 * @brief Get an integer status value.
 * @param group Group name
 * @param name Property name
 * @return Integer value, or 0 if not found
 */
int ACAP_STATUS_Int(const char* group, const char* name);

/**
 * @brief Get a double status value.
 * @param group Group name
 * @param name Property name
 * @return Double value, or 0.0 if not found
 */
double ACAP_STATUS_Double(const char* group, const char* name);

/**
 * @brief Get a string status value.
 * @param group Group name
 * @param name Property name
 * @return String value (internally managed, do NOT free), or NULL
 */
char* ACAP_STATUS_String(const char* group, const char* name);

/**
 * @brief Get a cJSON object status value.
 * @param group Group name
 * @param name Property name
 * @return cJSON object (internally managed, do NOT delete), or NULL
 */
cJSON* ACAP_STATUS_Object(const char* group, const char* name);

/* Status Setters - Thread-safe */

/**
 * @brief Set a boolean status value.
 * @param group Group name (created if doesn't exist)
 * @param name Property name
 * @param state Boolean value (0 or 1)
 */
void ACAP_STATUS_SetBool(const char* group, const char* name, int state);

/**
 * @brief Set a numeric status value.
 * @param group Group name
 * @param name Property name
 * @param value Numeric value
 */
void ACAP_STATUS_SetNumber(const char* group, const char* name, double value);

/**
 * @brief Set a string status value.
 * @param group Group name
 * @param name Property name
 * @param string String value (will be copied)
 */
void ACAP_STATUS_SetString(const char* group, const char* name, const char* string);

/**
 * @brief Set a cJSON object status value.
 * @param group Group name
 * @param name Property name
 * @param data cJSON object (will be duplicated, caller retains ownership)
 */
void ACAP_STATUS_SetObject(const char* group, const char* name, cJSON* data);

/**
 * @brief Set a status value to null.
 * @param group Group name
 * @param name Property name
 */
void ACAP_STATUS_SetNull(const char* group, const char* name);

/*=====================================================
 * VAPIX API
 * 
 * Functions for making authenticated requests to the local
 * camera's VAPIX API endpoints.
 *=====================================================*/

/**
 * @brief Make a GET request to a VAPIX endpoint.
 * 
 * @param request The VAPIX CGI path (without http://camera/axis-cgi/ prefix)
 * @return Response body as string on success, NULL on failure
 * 
 * @warning Caller MUST free() the returned string.
 * 
 * Example:
 * @code
 * char* response = ACAP_VAPIX_Get("param.cgi?action=list&group=root.Brand");
 * if (response) {
 *     printf("Response: %s\n", response);
 *     free(response);
 * }
 * @endcode
 */
char* ACAP_VAPIX_Get(const char* request);

/**
 * @brief Make a POST request to a VAPIX endpoint.
 * 
 * @param request The VAPIX CGI path
 * @param body The POST body content
 * @return Response body as string on success, NULL on failure
 * 
 * @warning Caller MUST free() the returned string.
 * 
 * Example:
 * @code
 * const char* json = "{\"apiVersion\":\"1.0\",\"method\":\"getSupportedVersions\"}";
 * char* response = ACAP_VAPIX_Post("basicdeviceinfo.cgi", json);
 * if (response) {
 *     cJSON* parsed = cJSON_Parse(response);
 *     // ... process response ...
 *     cJSON_Delete(parsed);
 *     free(response);
 * }
 * @endcode
 */
char* ACAP_VAPIX_Post(const char* request, const char* body);

#ifdef __cplusplus
}
#endif

#endif /* _ACAP_H_ */
