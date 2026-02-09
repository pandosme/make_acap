# ACAP Technical Reference

This document is the authoritative technical reference for developing ACAP applications using the [make_acap](https://github.com/pandosme/make_acap) templates. It contains the full ACAP.h API, code patterns, build configuration, and complete reference implementations.

## Related Documentation

| Document | Description |
|----------|-------------|
| [../README.md](../README.md) | Project overview, template list, quick start |
| [EVENTS.md](EVENTS.md) | Comprehensive catalog of all Axis device events with namespaces, properties, and state types |
| [VDO.md](VDO.md) | VDO (Video Device Object) API reference for video capture and streaming |
| Template `README.md` files | Template-specific configuration, endpoints, and usage |

## Available Templates

| Template | Key Feature | Best For |
|----------|-------------|----------|
| `base` | UI, image capture, events, settings | Most new projects |
| `event_subscription` | Static event monitoring | Event-driven services |
| `event_selection` | Dynamic event/timer trigger UI | Configurable trigger sources |
| `mqtt` | MQTT pub/sub client | MQTT-connected services |

***

## Project Structure

A typical ACAP project tree:

```
app/
  ├── ACAP.c / ACAP.h          # ACAP API wrapper (do not edit)
  ├── main.c                   # Application logic (edit here)
  ├── cJSON.c / cJSON.h        # JSON helper
  ├── settings/
  │     ├── settings.json      # User/config parameters
  │     ├── events.json        # Device event declarations (if needed)
  │     ├── mqtt.json          # MQTT client  (if needed)
  │     └── subscriptions.json # Device event subscriptions (if needed)
  ├── html/
  │     └── index.html         # (Optional) web UI
  └── manifest.json            # App manifest (identity, endpoints, permissions)
Makefile                       # How the ACAP is built (sources + libraries)
Dockerfile                     # Docker/SDK build environment
build.sh                       # Helper: builds and packages for upload
```

***

## Core Build & Metadata Files

### Makefile

**Purpose:** Tells the SDK how to build your app, which files and libraries are required.

**How/when to edit:**
- Add every `.c` file you use to `OBJS1`
- Add all required libraries to `PKGS`
- `PROG1` must match `appName` in manifest.json and `APP_PACKAGE` in main.c

**Edit Example for MQTT + VDO:**
```makefile
OBJS1 = main.c ACAP.c cJSON.c MQTT.c CERTS.c
PKGS = glib-2.0 gio-2.0 vdostream vdo-frame vdo-types axevent fcgi libcurl
```

### manifest.json

**Purpose:** Declares app identity, endpoints, settings page, and permissions.

**How/when to edit:**
- Add endpoints in the `"httpConfig"` array for each API you want accessible
- Add or change `"settingPage"` for main web UI (`index.html`, `mqtt.html`, etc.)
- For advanced features (credentials, DBus...), add entries to `"resources"` as needed

```json
"httpConfig": [
  {"name": "app", "access": "admin", "type": "fastCgi"},
  {"name": "settings", "access": "admin", "type": "fastCgi"},
  {"name": "capture", "access": "admin", "type": "fastCgi"},
  {"name": "publish", "access": "admin", "type": "fastCgi"}
]
```

### Build Configuration Summary

| Feature         | Add to OBJS1                          | Add to PKGS                               | manifest.json            |
|-----------------|---------------------------------------|-------------------------------------------|--------------------------|
| Image Capture   | main.c ACAP.c cJSON.c                | vdostream vdo-frame vdo-types glib fcgi   | "capture" endpoint       |
| MQTT            | main.c ACAP.c cJSON.c MQTT.c CERTS.c | libcurl glib fcgi axevent                 | "publish" endpoint/ui    |
| Events/subscr   | main.c ACAP.c cJSON.c                | axevent glib fcgi                         | (no endpoint needed)     |

### Renaming an ACAP

To rename a package (e.g., from "base" to "myapp"), update these three files:

1. **Makefile**: `PROG1 = myapp`
2. **main.c**: `#define APP_PACKAGE "myapp"`
3. **manifest.json**: `"appName": "myapp"` and `"friendlyName": "My App"`

***

## ACAP SDK Wrapper (ACAP.c/.h) — API Reference

- Always call `ACAP(<package>, callback)` during startup
- Register HTTP endpoints with `ACAP_HTTP_Node("endpoint", handler_fn)`
- Built-in endpoints: `/local/<package>/app` (metadata), `/local/<package>/settings` (config)

### Full ACAP.h Header

```c
/*
 * ACAP SDK wrapper for verion 12
 * Copyright (c) 2025 Fred Juhlin
 * MIT License - See LICENSE file for details
 * Version 3.7
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
```

### Memory Management

| Function | Returns | Ownership |
|----------|---------|-----------|
| `ACAP_Get_Config()` | Internally managed `cJSON*` | **DO NOT** `cJSON_Delete()` |
| `ACAP_STATUS_*()` getters | Internally managed `cJSON*` | **DO NOT** `cJSON_Delete()` |
| `ACAP_DEVICE_JSON()` | Internally managed `cJSON*` | **DO NOT** `cJSON_Delete()` |
| `ACAP_FILE_Read()` | Newly allocated `cJSON*` | **MUST** `cJSON_Delete()` |
| `ACAP_VAPIX_Get()` / `ACAP_VAPIX_Post()` | Allocated `char*` | **MUST** `free()` |
| `ACAP_HTTP_Request_Param()` | Allocated `char*` | **MUST** `free()` |
| `cJSON_PrintUnformatted()` / `cJSON_Print()` | Allocated `char*` | **MUST** `free()` |

***

## HTTP Endpoints

Endpoints are pre-configured by ACAP.c:

- `/app` — Returns everything about the application (manifest, settings, device info, status)
- `/settings` — GET returns settings; POST updates settings
- `/status` — Returns all live/health/status fields

Example `/app` response:
```json
{"manifest":{ ... }, "settings":{...}, "status":{...}, "device":{...}}
```
Example `/settings` response:
```json
{"name":"John Doe","age":50}
```
Example `/status` response:
```json
{
  "system": {
    "healthy": true,
    "status": "Everything is up and running"
  },
  "video": {
    "fps": 25.0
  }
}
```

### Custom HTTP Endpoints

Custom endpoints are:
1. Declared in `manifest.json` under `httpConfig[]`
2. Registered in `main.c` using `ACAP_HTTP_Node("nodename", callback_function)`
3. Accessible at `http://camera/local/<appName>/<nodename>`

```c
void My_Endpoint(const ACAP_HTTP_Response response, const ACAP_HTTP_Request request) {
    const char* method = ACAP_HTTP_Get_Method(request);
    if (!method) {
        ACAP_HTTP_Respond_Error(response, 400, "Invalid Request Method");
        return;
    }
    if (strcmp(method, "GET") == 0) {
        const char* someParam = ACAP_HTTP_Request_Param(request, "param1");
        ACAP_HTTP_Respond_String(response, "Hello from GET %s", someParam ? someParam : "");
        return;
    }
    if (strcmp(method, "POST") == 0) {
        const char* contentType = ACAP_HTTP_Get_Content_Type(request);
        if (!contentType || strcmp(contentType, "application/json") != 0) {
            ACAP_HTTP_Respond_Error(response, 415, "Unsupported Media Type - Use application/json");
            return;
        }
        if (!request->postData || request->postDataLength == 0) {
            ACAP_HTTP_Respond_Error(response, 400, "Missing POST data");
            return;
        }
        cJSON* bodyObject = cJSON_Parse(request->postData);
        if (!bodyObject) {
            ACAP_HTTP_Respond_Error(response, 400, "Invalid JSON data");
            return;
        }
        ACAP_HTTP_Respond_JSON(response, bodyObject);
        return;
    }
    ACAP_HTTP_Respond_Error(response, 400, "Invalid Request Method");
}
```

***

## Configuration: settings/settings.json

Edit this file to expose any app settings that should be user-editable (handled in the `main.c` callback):

```json
{
  "name": "John Doe",
  "age": 50
}
```

The ACAP wrapper manages the GET and POST for settings.
To get your app's current configuration, always use:
```c
cJSON* settings = ACAP_Get_Config("settings");
```
- **Do NOT read config files directly** (i.e., don't use `ACAP_FILE_Read("settings/settings.json")` to fetch live settings).
- The ACAP SDK manages settings in memory, ensures atomic updates, and provides thread safety through `ACAP_Get_Config`.
- **Never manually delete or free** the returned `settings` pointer. It is handled by the SDK!

### Example: Using Settings in an Event Callback

```c
void
My_Event_Callback(cJSON *event, void* userdata) {
    // Get app settings safely
    cJSON* settings = ACAP_Get_Config("settings");
    const char* name = cJSON_GetObjectItem(settings,"name")?cJSON_GetObjectItem(settings,"name")->valuestring:0;
    int age = cJSON_GetObjectItem(settings,"age")?cJSON_GetObjectItem(settings,"age")->valueint:0;

    // Proceed with rest of logic...
}
```

The default settings are stored in `settings/settings.json`. Updated settings are stored in `localdata/settings.json`.

***

## Status Management

Use from any code to update health, app metrics, status, etc. All status fields are grouped by logical area (`system`, `mqtt`, etc.).

```c
// Setters:
ACAP_STATUS_SetBool("system", "healthy", 1);
ACAP_STATUS_SetNumber("video", "fps", 25.0);
ACAP_STATUS_SetString("system", "status", "Running");
ACAP_STATUS_SetObject("data", "latest", someJsonObject);
ACAP_STATUS_SetNull("data", "error");

// Getters:
int healthy = ACAP_STATUS_Bool("system", "healthy");
double fps = ACAP_STATUS_Double("video", "fps");
char* status = ACAP_STATUS_String("system", "status");
cJSON* obj = ACAP_STATUS_Object("data", "latest");
```

Web UIs can fetch `/status` on a timer to show the latest state.

***

## Capturing Images Using the Axis VDO API

Perform a live snapshot (JPEG) using Axis camera hardware:
```c
int width = 1920, height = 1080;
VdoMap* vdoSettings = vdo_map_new();
vdo_map_set_uint32(vdoSettings, "format", VDO_FORMAT_JPEG);
vdo_map_set_uint32(vdoSettings, "width", width);
vdo_map_set_uint32(vdoSettings, "height", height);

GError* error = NULL;
VdoBuffer* buffer = vdo_stream_snapshot(vdoSettings, &error);
g_clear_object(&vdoSettings);

if (error != NULL) {
    // handle error, call g_error_free()
}
unsigned char* data = vdo_buffer_get_data(buffer);
unsigned int size = vdo_frame_get_size(buffer);
// ...save, process, or transmit as needed...
g_object_unref(buffer);
```

Always free all objects after use.

***

## Declaring, Firing, and Subscribing to Events

**Declare events for your service in `settings/events.json`:**
```json
[
  { "id": "state", "name": "Demo: State", "state": true,  "show": true, "data": [] },
  { "id": "trigger", "name": "Demo: Trigger", "state": false, "show": true, "data": [] }
]
```
To fire events in code:
```c
ACAP_EVENTS_Fire_State("state", value);   // for stateful (1 = true, 0 = false)
ACAP_EVENTS_Fire("trigger");              // for trigger/stateless events
```

**Subscribe to device events using `settings/subscriptions.json`:**
```json
[
  { "name": "All ACAP Events", "topic0": {"tnsaxis":"CameraApplicationPlatform"} },
  { "name": "Virtual Input", "topic0": {"tns1":"Device"}, "topic1": {"tnsaxis":"IO"}, "topic2": {"tnsaxis":"VirtualInput"} }
]
```

For a comprehensive listing of all available Axis device events with their namespaces, properties, and state types, see [EVENTS.md](EVENTS.md).

Common event subscription examples:

| Event | topic0 | topic1 | topic2 |
|-------|--------|--------|--------|
| Virtual input | `tns1:Device` | `tnsaxis:IO` | `tnsaxis:VirtualInput` |
| Day/Night | `tns1:VideoSource` | `tnsaxis:DayNightVision` | — |
| Object Analytics | `tnsaxis:CameraApplicationPlatform` | `tnsaxis:ObjectAnalytics` | `tnsaxis:Device1ScenarioANY` |
| All ACAP events | `tnsaxis:CameraApplicationPlatform` | — | — |

Setup in main:
```c
void
My_Event_Callback(cJSON *event, void* userdata) {
	char* json = cJSON_PrintUnformatted(event);
	LOG("%s: %s\n", __func__,json);
	free(json);
}

cJSON* subscriptions = ACAP_FILE_Read("settings/subscriptions.json");
for (cJSON* sub = subscriptions ? subscriptions->child : 0; sub; sub = sub->next)
    ACAP_EVENTS_Subscribe(sub, NULL);
```

***

## Example Main Application Skeleton

```c
int main(void) {
    openlog(APP_PACKAGE, LOG_PID|LOG_CONS, LOG_USER);
    ACAP(APP_PACKAGE, Settings_Updated_Callback);
    ACAP_HTTP_Node("capture", HTTP_Endpoint_capture);
    ACAP_HTTP_Node("fire", HTTP_Endpoint_fire);
    ACAP_EVENTS_SetCallback(My_Event_Callback);
    cJSON* subs = ACAP_FILE_Read("settings/subscriptions.json");
    for (cJSON* sub = subs ? subs->child : 0; sub; sub = sub->next)
        ACAP_EVENTS_Subscribe(sub, NULL);
    // start main loop, handle signals, cleanup...
}
```

***

## MQTT in Axis ACAP Projects

Add MQTT client capability for publish/subscribe automation.

All broker/user/query settings stored in `settings/mqtt.json`:
```json
{
  "connect": false,
  "address": "",
  "port": "1883",
  "user": "",
  "password": "",
  "preTopic": "mqttdemo",
  "tls": false,
  "verify": false,
  "payload": { "name": "", "location": "" }
}
```

Call `MQTT_Init` in your main; add endpoints and callbacks as needed:
```c
MQTT_Init(Main_MQTT_Status, Main_MQTT_Subscription_Message);
MQTT_Subscribe("my/topic");
```

***

## Exception Handling & Logging

### Exception Handling — Always Validate Inputs

Whatever you read (settings, event fields, config), **check for validity:**
```c
cJSON* settings = ACAP_Get_Config("settings");
if (!settings) {
    LOG_WARN("No settings available, cannot continue.");
    return;
}
cJSON* age_item = cJSON_GetObjectItem(settings, "age");
int age = age_item ? age_item->valueint : 0;
if (!age) {
    LOG_WARN("Age setting missing or zero, cannot continue.");
    return;
}
```

Always test for NULL, missing, or invalid values before usage. On error, log with `LOG_WARN` and exit gracefully.

### Logging Style

- **Debug/verbose output** — Use `LOG_TRACE()`. Toggle by commenting/uncommenting the macro:
  ```c
  //#define LOG_TRACE(fmt, args...)  { syslog(LOG_INFO, fmt, ## args); printf(fmt, ## args); }
  #define LOG_TRACE(fmt, args...)   {}
  ```
- **Info messages** — Use `LOG()`
- **Warnings** — Use `LOG_WARN()`

***

## Best Practices

- **Modify only** needed files: `main.c`, settings, `manifest.json`, HTML, `Makefile`
- **Do not edit** library files: `ACAP.c` / `ACAP.h` / `cJSON.c` / `cJSON.h`
- New settings: put in `settings/settings.json` and handle in callback
- Event listening: add topic in `subscriptions.json`, handle in callback
- HTTP APIs: implement in C, register via `ACAP_HTTP_Node`, add to `manifest.json`

***

## Reference Code

### base/app/main.c
```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <glib.h>
#include <glib-unix.h>
#include <signal.h>

#include "vdo-stream.h"
#include "vdo-frame.h"
#include "vdo-types.h"
#include "ACAP.h"
#include "cJSON.h"

#define APP_PACKAGE	"base"

#define LOG(fmt, args...)    { syslog(LOG_INFO, fmt, ## args); printf(fmt, ## args);}
#define LOG_WARN(fmt, args...)    { syslog(LOG_WARNING, fmt, ## args); printf(fmt, ## args);}
//#define LOG_TRACE(fmt, args...)    { syslog(LOG_INFO, fmt, ## args); printf(fmt, ## args); }
#define LOG_TRACE(fmt, args...)    {}

void
Settings_Updated_Callback( const char* service, cJSON* data) {
	char* json = cJSON_PrintUnformatted(data);
	LOG_TRACE("%s: Service=%s Data=%s\n",__func__, service, json);
	free(json);
}


void
HTTP_Endpoint_fire(const ACAP_HTTP_Response response, const ACAP_HTTP_Request request) {
	//Note the the events are declared in ./app/html/config/events.json

    const char* method = ACAP_HTTP_Get_Method(request);
    if (!method) {
        ACAP_HTTP_Respond_Error(response, 400, "Invalid Request Method");
        return;
    }

    if (strcmp(method, "GET") != 0) {
        ACAP_HTTP_Respond_Error(response, 400, "Invalid Request Method");
		return;
	}


    const char* id = ACAP_HTTP_Request_Param(request, "id");
    const char* value_str = ACAP_HTTP_Request_Param(request, "value");
    int state = value_str ? atoi(value_str) : 0;

	LOG("Event fired %s %d\n",id,state);

	if(id) {
		LOG_TRACE("%s: Event id: %s\n",__func__,id);
	} else {
		LOG_WARN("%s: Missing event id\n",__func__);
		ACAP_HTTP_Respond_Error( response, 500, "Invalid event ID" );
		return;
	}

	if(value_str) {
		LOG_TRACE("%s: Event value: %s\n",__func__, value_str);
	}

	if( strcmp( id, "state" ) == 0 ) {
		ACAP_EVENTS_Fire_State( id, state );
		ACAP_HTTP_Respond_Text( response, "State event fired" );
		return;
	}
	if( strcmp( id, "trigger" ) == 0 ) {
		ACAP_EVENTS_Fire( id );
		ACAP_HTTP_Respond_Text( response, "Trigger event fired" );
		return;
	}
	ACAP_HTTP_Respond_Error( response, 500, "Invalid event ID" );
}

void
HTTP_Endpoint_capture(const ACAP_HTTP_Response response, const ACAP_HTTP_Request request) {
    const char* method = ACAP_HTTP_Get_Method(request);
    if (!method) {
        ACAP_HTTP_Respond_Error(response, 400, "Invalid Request Method");
        return;
    }

    if (strcmp(method, "GET") != 0) {
        ACAP_HTTP_Respond_Error(response, 400, "Invalid Request Method");
		return;
	}

    const char* width_str = ACAP_HTTP_Request_Param(request, "width");
    const char* height_str = ACAP_HTTP_Request_Param(request, "height");

    int width = width_str ? atoi(width_str) : 1920;
    int height = height_str ? atoi(height_str) : 1080;
	LOG("Image capture %dx%d\n",width,height);

    // Create VDO settings for snapshot
    VdoMap* vdoSettings = vdo_map_new();

    vdo_map_set_uint32(vdoSettings, "format", VDO_FORMAT_JPEG);
    vdo_map_set_uint32(vdoSettings, "width", width);
    vdo_map_set_uint32(vdoSettings, "height", height);

    // Take snapshot
    GError* error = NULL;
    VdoBuffer* buffer = vdo_stream_snapshot(vdoSettings, &error);
	g_clear_object(&vdoSettings);

    if (error != NULL) {
        // Handle error
        ACAP_HTTP_Respond_Error(response, 503, "Snapshot capture failed");
        g_error_free(error);
        return;
    }

    // Get snapshot data
    unsigned char* data = vdo_buffer_get_data(buffer);
    unsigned int size = vdo_frame_get_size(buffer);

    // Build HTTP response
	ACAP_HTTP_Respond_String( response, "status: 200 OK\r\n");
	ACAP_HTTP_Respond_String( response, "Content-Description: File Transfer\r\n");
	ACAP_HTTP_Respond_String( response, "Content-Type: image/jpeg\r\n");
	ACAP_HTTP_Respond_String( response, "Content-Disposition: attachment; filename=snapshot.jpeg\r\n");
	ACAP_HTTP_Respond_String( response, "Content-Transfer-Encoding: binary\r\n");
	ACAP_HTTP_Respond_String( response, "Content-Length: %u\r\n", size );
	ACAP_HTTP_Respond_String( response, "\r\n");
	ACAP_HTTP_Respond_Data( response, size, data );

    // Clean up
    g_object_unref(buffer);
}


static GMainLoop *main_loop = NULL;

static gboolean
signal_handler(gpointer user_data) {
    LOG("Received SIGTERM, initiating shutdown\n");
    if (main_loop && g_main_loop_is_running(main_loop)) {
        g_main_loop_quit(main_loop);
    }
    return G_SOURCE_REMOVE;
}


int main(void) {
    openlog(APP_PACKAGE, LOG_PID|LOG_CONS, LOG_USER);
    LOG("------ Starting ACAP Service ------\n");
    ACAP_STATUS_SetString("app", "status", "The application is starting");

    ACAP(APP_PACKAGE, Settings_Updated_Callback);
    ACAP_HTTP_Node("capture", HTTP_Endpoint_capture);
    ACAP_HTTP_Node("fire", HTTP_Endpoint_fire);

    LOG("Entering main loop\n");
	main_loop = g_main_loop_new(NULL, FALSE);
    GSource *signal_source = g_unix_signal_source_new(SIGTERM);
    if (signal_source) {
		g_source_set_callback(signal_source, signal_handler, NULL, NULL);
		g_source_attach(signal_source, NULL);
	} else {
		LOG_WARN("Signal detection failed");
	}
    g_main_loop_run(main_loop);
	LOG("Terminating and cleaning up %s\n",APP_PACKAGE);
    ACAP_Cleanup();
    closelog();
    return 0;
}
```

### event_subscription/app/main.c
```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <glib.h>
#include <glib-unix.h>
#include <signal.h>
#include "ACAP.h"
#include "cJSON.h"

#define APP_PACKAGE	"event_subscription"

#define LOG(fmt, args...)    { syslog(LOG_INFO, fmt, ## args); printf(fmt, ## args);}
#define LOG_WARN(fmt, args...)    { syslog(LOG_WARNING, fmt, ## args); printf(fmt, ## args);}
#define LOG_TRACE(fmt, args...)    { syslog(LOG_INFO, fmt, ## args); printf(fmt, ## args); }
//#define LOG_TRACE(fmt, args...)    {}

void
My_Event_Callback(cJSON *event, void* userdata) {
	char* json = cJSON_PrintUnformatted(event);
	LOG("%s: %s\n", __func__,json);
	free(json);
}

static GMainLoop *main_loop = NULL;

static gboolean
signal_handler(gpointer user_data) {
    LOG("Received SIGTERM, initiating shutdown\n");
    if (main_loop && g_main_loop_is_running(main_loop)) {
        g_main_loop_quit(main_loop);
    }
    return G_SOURCE_REMOVE;
}


cJSON* eventSubscriptions = 0;

int main(void) {
	openlog(APP_PACKAGE, LOG_PID|LOG_CONS, LOG_USER);
    LOG("------ Starting ACAP Service ------\n");


	ACAP( APP_PACKAGE, NULL );  //No configuration parameters

	//Add Event subscriptions
	ACAP_EVENTS_SetCallback( My_Event_Callback );
	eventSubscriptions = ACAP_FILE_Read( "settings/subscriptions.json" );
	cJSON* subscription = eventSubscriptions?eventSubscriptions->child:0;
	while(subscription){
		ACAP_EVENTS_Subscribe( subscription, NULL );
		subscription = subscription->next;
	}

    LOG("Entering main loop\n");
	main_loop = g_main_loop_new(NULL, FALSE);
    GSource *signal_source = g_unix_signal_source_new(SIGTERM);
    if (signal_source) {
		g_source_set_callback(signal_source, signal_handler, NULL, NULL);
		g_source_attach(signal_source, NULL);
	} else {
		LOG_WARN("Signal detection failed");
	}

    g_main_loop_run(main_loop);
	LOG("Terminating and cleaning up %s\n",APP_PACKAGE);
    ACAP_Cleanup();
	closelog();
	return 0;
}
```

### event_selection/app/main.c
```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <glib.h>
#include <glib-unix.h>
#include <signal.h>
#include "ACAP.h"
#include "cJSON.h"

#define APP_PACKAGE	"event_selection"

#define LOG(fmt, args...)    { syslog(LOG_INFO, fmt, ## args); printf(fmt, ## args);}
#define LOG_WARN(fmt, args...)    { syslog(LOG_WARNING, fmt, ## args); printf(fmt, ## args);}
//#define LOG_TRACE(fmt, args...)    { syslog(LOG_INFO, fmt, ## args); printf(fmt, ## args); }
#define LOG_TRACE(fmt, args...)    {}

static GMainLoop *main_loop = NULL;
static int currentSubscriptionId = 0;
static guint timerId = 0;

/*
 * Extract a boolean state value from an incoming event.
 * Checks common property names used across Axis event types.
 * Returns 1 for active/true, 0 for inactive/false.
 */
static int
Extract_Event_State(cJSON* event) {
	const char* stateProperties[] = {
		"active", "state", "triggered", "LogicalState",
		"ready", "Open", "lost", "Detected", "recording",
		"accessed", "connected", "day", "sensor_level",
		"disruption", "alert", "fan_failure", "Failed",
		"limit_exceeded", "abr_error", "tampering",
		"signal_status_ok", "Playing", NULL
	};

	for (int i = 0; stateProperties[i]; i++) {
		cJSON* item = cJSON_GetObjectItem(event, stateProperties[i]);
		if (item) {
			if (cJSON_IsBool(item))
				return cJSON_IsTrue(item) ? 1 : 0;
			if (cJSON_IsNumber(item))
				return item->valueint ? 1 : 0;
			if (cJSON_IsString(item)) {
				if (strcmp(item->valuestring, "1") == 0 ||
				    strcmp(item->valuestring, "true") == 0)
					return 1;
				return 0;
			}
		}
	}
	return 1;  // Default to active if no recognized state property
}

void
My_Event_Callback(cJSON *event, void* userdata) {
	char* json = cJSON_PrintUnformatted(event);
	if (json) {
		LOG("Event received: %s\n", json);
		free(json);
	}

	int state = Extract_Event_State(event);
	ACAP_EVENTS_Fire_State("state", state);
	ACAP_EVENTS_Fire("trigger");
	ACAP_STATUS_SetBool("trigger", "active", state);
	ACAP_STATUS_SetString("trigger", "lastTriggered", ACAP_DEVICE_ISOTime());
}

static gboolean
Timer_Callback(gpointer user_data) {
	LOG("Timer triggered\n");
	ACAP_EVENTS_Fire("trigger");
	ACAP_EVENTS_Fire_State("state", 1);
	ACAP_STATUS_SetString("trigger", "lastTriggered", ACAP_DEVICE_ISOTime());
	g_usleep(100000);
	ACAP_EVENTS_Fire_State("state", 0);
	return G_SOURCE_CONTINUE;
}

static void
Apply_Trigger(void) {
	if (currentSubscriptionId > 0) {
		ACAP_EVENTS_Unsubscribe(currentSubscriptionId);
		currentSubscriptionId = 0;
	}
	if (timerId > 0) {
		g_source_remove(timerId);
		timerId = 0;
	}

	ACAP_EVENTS_Fire_State("state", 0);
	ACAP_STATUS_SetBool("trigger", "active", 0);

	cJSON* settings = ACAP_Get_Config("settings");
	if (!settings) {
		LOG_WARN("No settings available\n");
		ACAP_STATUS_SetString("trigger", "type", "none");
		ACAP_STATUS_SetString("trigger", "status", "No configuration");
		return;
	}

	cJSON* triggerTypeItem = cJSON_GetObjectItem(settings, "triggerType");
	const char* triggerType = triggerTypeItem ? triggerTypeItem->valuestring : "none";
	if (!triggerType) triggerType = "none";

	if (strcmp(triggerType, "event") == 0) {
		cJSON* triggerEvent = cJSON_GetObjectItem(settings, "triggerEvent");
		if (triggerEvent && !cJSON_IsNull(triggerEvent)) {
			cJSON* nameItem = cJSON_GetObjectItem(triggerEvent, "name");
			const char* eventName = nameItem ? nameItem->valuestring : "Unknown";
			currentSubscriptionId = ACAP_EVENTS_Subscribe(triggerEvent, NULL);
			if (currentSubscriptionId > 0) {
				LOG("Subscribed to event: %s (id=%d)\n", eventName, currentSubscriptionId);
				ACAP_STATUS_SetString("trigger", "type", "event");
				ACAP_STATUS_SetString("trigger", "event", eventName);
				ACAP_STATUS_SetString("trigger", "status", "Monitoring event");
			} else {
				LOG_WARN("Failed to subscribe to event: %s\n", eventName);
				ACAP_STATUS_SetString("trigger", "status", "Subscription failed");
			}
		} else {
			ACAP_STATUS_SetString("trigger", "status", "No event selected");
		}
	} else if (strcmp(triggerType, "timer") == 0) {
		cJSON* timerItem = cJSON_GetObjectItem(settings, "timer");
		int interval = timerItem ? timerItem->valueint : 60;
		if (interval < 1) interval = 1;
		timerId = g_timeout_add_seconds(interval, Timer_Callback, NULL);
		LOG("Timer started: every %d seconds\n", interval);
		ACAP_STATUS_SetString("trigger", "type", "timer");
		ACAP_STATUS_SetNumber("trigger", "interval", interval);
		ACAP_STATUS_SetString("trigger", "status", "Timer running");
	} else {
		ACAP_STATUS_SetString("trigger", "type", "none");
		ACAP_STATUS_SetString("trigger", "status", "Disabled");
	}
}

void
Settings_Updated_Callback(const char* service, cJSON* data) {
	LOG_TRACE("Settings updated: %s\n", service);
	if (strcmp(service, "settings") == 0) {
		Apply_Trigger();
	}
}

void
HTTP_Endpoint_trigger(const ACAP_HTTP_Response response, const ACAP_HTTP_Request request) {
	const char* method = ACAP_HTTP_Get_Method(request);
	if (!method) {
		ACAP_HTTP_Respond_Error(response, 400, "Invalid request");
		return;
	}
	if (strcmp(method, "GET") == 0) {
		cJSON* status = ACAP_STATUS_Group("trigger");
		if (status) ACAP_HTTP_Respond_JSON(response, status);
		else ACAP_HTTP_Respond_Text(response, "{}");
		return;
	}
	if (strcmp(method, "POST") == 0) {
		ACAP_EVENTS_Fire("trigger");
		ACAP_EVENTS_Fire_State("state", 1);
		ACAP_STATUS_SetString("trigger", "lastTriggered", ACAP_DEVICE_ISOTime());
		g_usleep(100000);
		ACAP_EVENTS_Fire_State("state", 0);
		ACAP_HTTP_Respond_Text(response, "Trigger fired");
		return;
	}
	ACAP_HTTP_Respond_Error(response, 405, "Method not allowed");
}

static gboolean
signal_handler(gpointer user_data) {
	LOG("Received SIGTERM, initiating shutdown\n");
	if (main_loop && g_main_loop_is_running(main_loop))
		g_main_loop_quit(main_loop);
	return G_SOURCE_REMOVE;
}

int main(void) {
	openlog(APP_PACKAGE, LOG_PID|LOG_CONS, LOG_USER);
	LOG("------ Starting ACAP Service ------\n");
	ACAP_STATUS_SetString("trigger", "status", "Starting");

	ACAP(APP_PACKAGE, Settings_Updated_Callback);
	ACAP_HTTP_Node("trigger", HTTP_Endpoint_trigger);
	ACAP_EVENTS_SetCallback(My_Event_Callback);
	Apply_Trigger();

	main_loop = g_main_loop_new(NULL, FALSE);
	GSource *signal_source = g_unix_signal_source_new(SIGTERM);
	if (signal_source) {
		g_source_set_callback(signal_source, signal_handler, NULL, NULL);
		g_source_attach(signal_source, NULL);
	}
	g_main_loop_run(main_loop);

	if (timerId > 0) g_source_remove(timerId);
	ACAP_Cleanup();
	closelog();
	return 0;
}
```

### mqtt/app/main.c
```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <glib.h>
#include <glib-unix.h>
#include <signal.h>

#include "cJSON.h"
#include "ACAP.h"
#include "MQTT.h"

#define APP_PACKAGE	"basemqtt"

#define LOG(fmt, args...)    { syslog(LOG_INFO, fmt, ## args); printf(fmt, ## args);}
#define LOG_WARN(fmt, args...)    { syslog(LOG_WARNING, fmt, ## args); printf(fmt, ## args);}
//#define LOG_TRACE(fmt, args...)    { syslog(LOG_INFO, fmt, ## args); printf(fmt, ## args); }
#define LOG_TRACE(fmt, args...)    {}

void
Settings_Updated_Callback( const char* service, cJSON* data) {
	char* json = cJSON_PrintUnformatted(data);
	LOG_TRACE("%s: Service=%s Data=%s\n",__func__, service, json);
	free(json);
}

void
HTTP_ENDPOINT_Publish(ACAP_HTTP_Response response,const ACAP_HTTP_Request request) {
    const char* method = ACAP_HTTP_Get_Method(request);
    if (!method) {
        ACAP_HTTP_Respond_Error(response, 400, "Invalid Request Method");
        return;
    }

    if (strcmp(method, "POST") != 0) {
        ACAP_HTTP_Respond_Error(response, 400, "Invalid Request Method");
		return;
	}

	const char* contentType = ACAP_HTTP_Get_Content_Type(request);
	if (!contentType || strcmp(contentType, "application/json") != 0) {
		ACAP_HTTP_Respond_Error(response, 415, "Unsupported Media Type - Use application/json");
		return;
	}

	if (!request->postData || request->postDataLength == 0) {
		ACAP_HTTP_Respond_Error(response, 400, "Missing POST data");
		return;
	}

	cJSON* body = cJSON_Parse(request->postData);
	if (!body) {
		ACAP_HTTP_Respond_Error(response, 400, "Invalid JSON");
		LOG_WARN("Unable to parse json for MQTT settings\n");
		return;
	}
	const char* topic = cJSON_GetObjectItem(body,"topic")?cJSON_GetObjectItem(body,"topic")->valuestring:0;
	if( !topic || strlen(topic) == 0) {
		ACAP_HTTP_Respond_Error(response, 400, "Topic must be set");
		return;
	}
	const char* payload = cJSON_GetObjectItem(body,"payload")?cJSON_GetObjectItem(body,"payload")->valuestring:0;
	if( !payload ) {
		ACAP_HTTP_Respond_Error(response, 400, "Payload must be set");
		return;
	}
	if( MQTT_Publish( topic, payload, 0, 0 ) )
		ACAP_HTTP_Respond_Text(response, "Message sent");
	else
		ACAP_HTTP_Respond_Error(response, 500, "Message publish failed");
}

void
Main_MQTT_Status(int state) {
    char topic[64];
    cJSON* message = 0;

    switch (state) {
        case MQTT_INITIALIZING:
            LOG("%s: Initializing\n", __func__);
            break;
        case MQTT_CONNECTING:
            LOG("%s: Connecting\n", __func__);
            break;
        case MQTT_CONNECTED:
            LOG("%s: Connected\n", __func__);
            sprintf(topic, "connect/%s", ACAP_DEVICE_Prop("serial"));
            message = cJSON_CreateObject();
            cJSON_AddTrueToObject(message, "connected");
            cJSON_AddStringToObject(message, "address", ACAP_DEVICE_Prop("IPv4"));
            MQTT_Publish_JSON(topic, message, 0, 1);
            cJSON_Delete(message);
            break;
        case MQTT_DISCONNECTING:
            sprintf(topic, "connect/%s", ACAP_DEVICE_Prop("serial"));
            message = cJSON_CreateObject();
            cJSON_AddFalseToObject(message, "connected");
            cJSON_AddStringToObject(message, "address", ACAP_DEVICE_Prop("IPv4"));
            MQTT_Publish_JSON(topic, message, 0, 1);
            cJSON_Delete(message);
            break;
        case MQTT_RECONNECTED:
            LOG("%s: Reconnected\n", __func__);
            break;
        case MQTT_DISCONNECTED:
            LOG("%s: Disconnect\n", __func__);
            break;
    }
}

void
Main_MQTT_Subscription_Message(const char *topic, const char *payload) {
    LOG("Message arrived: %s %s\n", topic, payload);
}


static GMainLoop *main_loop = NULL;

static gboolean
signal_handler(gpointer user_data) {
    LOG("Received SIGTERM, initiating shutdown\n");
    if (main_loop && g_main_loop_is_running(main_loop)) {
        g_main_loop_quit(main_loop);
    }
    return G_SOURCE_REMOVE;
}

int
main(void) {
	openlog(APP_PACKAGE, LOG_PID|LOG_CONS, LOG_USER);
	LOG("------ Starting ACAP Service ------\n");

	ACAP( APP_PACKAGE, Settings_Updated_Callback );
	ACAP_HTTP_Node("publish", HTTP_ENDPOINT_Publish );

    MQTT_Init(Main_MQTT_Status, Main_MQTT_Subscription_Message);
	MQTT_Subscribe( "my_topic" );

	main_loop = g_main_loop_new(NULL, FALSE);
    GSource *signal_source = g_unix_signal_source_new(SIGTERM);
    if (signal_source) {
		g_source_set_callback(signal_source, signal_handler, NULL, NULL);
		g_source_attach(signal_source, NULL);
	} else {
		LOG_WARN("Signal detection failed");
	}

	g_main_loop_run(main_loop);
	LOG("Terminating and cleaning up %s\n",APP_PACKAGE);
    Main_MQTT_Status(MQTT_DISCONNECTING);
    MQTT_Cleanup();
	ACAP_Cleanup();

    return 0;
}
```

***

## LLM Development Guidelines

- **Always provide Makefile and manifest.json edits** when suggesting new features that require new source files, libraries, or HTTP endpoints.
- **Validate all inputs** — check for NULL, missing, or invalid values before use. Log warnings with `LOG_WARN()` and return gracefully on error.
- **Select the right template** as a starting point: `base` for most projects, `event_subscription` for event monitoring, `event_selection` for configurable triggers, `mqtt` for MQTT integration.
- More source code examples: https://github.com/AxisCommunications/acap-native-sdk-examples
- ACAP SDK API documentation: https://developer.axis.com/acap/api

