## Overview

ACAP is an open application platform that enables you to develop and deploy applications directly on Axis network devices. This template simplifies development by wrapping complex SDK interactions and offering modular, reusable code for common tasks such as HTTP endpoints, configuration, and event management[1][2][3].

---

## Features

- **HTTP Endpoints**: FastCGI-based GET/POST/file transfer for RESTful APIs and web UIs
- **Image Capture**: Access camera images for processing or streaming
- **Configuration Management**: JSON-based settings with live update callbacks
- **Event System**: Fire and subscribe to device and application events
- **MQTT Support**: Lightweight publish/subscribe messaging
- **Web UI**: HTML/CSS/JS interface with video streaming
- **Modular Structure**: Easy to extend and maintain

---

## Prerequisites

- Axis device supporting ACAP
- Docker installed on your development PC
- C programming knowledge (embedded focus)
- Familiarity with HTML/JS/CSS for UI development

---

## Template Examples
- base:  Provides a user interface that demonstrates how to capture an image, alter settings and fire events
- event_subscription: Demostrates how an ACAP can monitor events to create actions
- mqtt: Demonstrate how to instance an MQTT client and connect it to an MQTT broker

## Quick Start

1. **Clone the Template**
   ```bash
   git clone https://github.com/pandosme/make_acap.git
   cd make_acap/base
   ```
2. **Build the ACAP**
   ```bash
   ./build.sh
   ```
3. **Install on Device**
   - Upload the resulting `.eap` file via AXIS Device Manager or device web UI.
4. **Customize**
   - Edit `main.c`, `manifest.json`, and `Makefile` to implement your service logic.

---

## Project Structure

```
.
├── app
│   ├── ACAP.c / ACAP.h         # SDK abstraction layer
│   ├── main.c                  # Application entry point
│   ├── cJSON.c / cJSON.h       # JSON handling
│   ├── settings/
│   │   ├── events.json         # Event declarations
│   │   └── settings.json       # Configuration settings
│   ├── html/                   # Web interface assets
│   │   │── css/
│   │   │── js/
│   │   └── index.html
│   └── manifest.json           # ACAP package manifest
├── Dockerfile                  # Build environment
├── README.md                   # This file
```

---

## Docker Build Example

The Axis-provided Docker images contain all tools needed to compile and package an ACAP.

**Build Command:**
```bash
docker build --build-arg ARCH=aarch64 --tag acap .
docker cp $(docker create acap):/opt/app ./build
cp build/*.eap .
```

**Sample Dockerfile:**
```dockerfile
ARG ARCH=armv7hf
ARG VERSION=12.0.0
ARG UBUNTU_VERSION=24.04
ARG REPO=axisecp
ARG SDK=acap-native-sdk

FROM ${REPO}/${SDK}:${VERSION}-${ARCH}-ubuntu${UBUNTU_VERSION} as sdk

WORKDIR /opt/app
COPY ./app .
RUN . /opt/axis/acapsdk/environment-setup* && acap-build .
```

## manifest.json
The manifest defines sveral properties for the project and what resources it will use. It is very important that the manifest is complete and correct. [More info about manifest.json](https://developer.axis.com/acap/acap-sdk-version-3/develop-applications/application-project-structure/#manifest-file)  
Here is a typical manifest.json that defines ACAP, name, version, user interface page and HTTP enpoint it exposes.  
```
{
    "schemaVersion": "1.7.1",
    "acapPackageConf": {
        "setup": {
            "friendlyName": "Base",
            "appName": "base",
            "vendor": "Fred Juhlin",
            "embeddedSdkVersion": "3.0",
            "vendorUrl": "https://pandosme.github.io",
            "runMode": "once",
            "version": "4.0.0"
        },
        "configuration": {
			"settingPage": "index.html",
			"httpConfig": [
				{"name": "app","access": "admin","type": "fastCgi"},
				{"name": "settings","access": "admin","type": "fastCgi"},
				{"name": "capture","access": "admin","type": "fastCgi"},
				{"name": "fire","access": "admin","type": "fastCgi"}
			]
		}
    }
}

---

## Customizing Your ACAP

### Renaming the Package

Update these files for a new package name (e.g., "MyOwnACAP"):

- `app/Makefile`  
  ```
  PROG1 = myOwnACAP
  ```
- `app/main.c`  
  ```
  #define APP_PACKAGE "myOwnACAP"
  ```
- `app/manifest.json`  
  ```
  "package": "myOwnACAP",
  "friendlyName": "My Own ACAP",
  ```

---

## Configuration Settings

Settings are declared in `app/settings/settings.json` and managed via HTTP endpoints. Updates trigger a callback in your code.

**Example `settings.json`:**
```json
{
  "someStringParam": "Hello",
  "someNumberParam": 52,
  "someBoolParam": false,
  "someObjectParam": {
      "a": 1,
      "b": 2
  }
}
```

**Settings Callback Example:**
```c
void Settings_Updated_Callback(const char* property, cJSON* data) {
  char* json = cJSON_PrintUnformatted(data);
  LOG_TRACE("%s: Property=%s Data=%s\n", __func__, property, json);
  free(json);
}
```


---

## HTTP Endpoints

Endpoints are used for UI and API integration. Define them in `manifest.json` and register them in your code.  The template ACAP has two predefined end points
1. GET app   Responds with all information a page may have including settings, manifest, device info and more
2. GET settings  Responds with the ACAP settings
3. POST settings  Post updates on one or more settings properties

**Manifest Example:**
```json
"httpConfig": [
  {"name": "app", "access": "admin", "type": "fastCgi"},
  {"name": "settings", "access": "admin", "type": "fastCgi"},
  {"name": "capture", "access": "admin", "type": "fastCgi"},  <-- Customized end point
  {"name": "fire", "access": "admin", "type": "fastCgi"}  <-- Customized end point
]
```

**Endpoint Registration Example:**
```c
void My_Test_Endpoint_Callback(const ACAP_HTTP_Response response, const ACAP_HTTP_Request request) {
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


---

## Events: Declaration, Firing, and Subscription

### Event Declaration

Events are defined in `app/settings/events.json`. Each event object includes:

- `"id"`: Unique event identifier
- `"name"`: Human-readable name
- `"state"`: Boolean; `true` for stateful (high/low), `false` for trigger/pulse
- `"show"`: Boolean; whether to show in UI or for internal use
- `"data"`: Array of extra event data

**Example `events.json`:**
```json
[
  {
    "id": "theStateEvent",
    "name": "Demo: State",
    "state": true,
    "show": true,
    "data": []
  },
  {
    "id": "theTriggerEvent",
    "name": "Demo: Trigger",
    "state": false,
    "show": true,
    "data": []
  }
]
```


### Firing Events in Code

- **Stateful Event:**
  ```c
  ACAP_EVENTS_Fire_State("theStateEvent", 1); // Set high
  ACAP_EVENTS_Fire_State("theStateEvent", 0); // Set low
  ```
- **Trigger Event:**
  ```c
  ACAP_EVENTS_Fire("theTriggerEvent");
  ```


### Event Subscription

Subscriptions are declared in `app/subscriptions` as a list of topic objects. You can subscribe to entire branches or specific events using topic hierarchies.

**Example Subscription File:**
```json
[
  {
    "name": "All ACAP Events",
    "topic0": {"tnsaxis": "CameraApplicationPlatform"}
  },
  {
    "name": "Camera Day and Night Switching",
    "topic0": {"tns1": "VideoSource"},
    "topic1": {"tnsaxis": "DayNightVision"}
  },
  {
    "name": "Virtual Input",
    "topic0": {"tns1": "Device"},
    "topic1": {"tnsaxis": "IO"},
    "topic2": {"tnsaxis": "VirtualInput"}
  }
]
```


**Common Event Topics Table:**

Common events and their topic nodes:
| Event Name | TOPIC0 | TOPIC1 | TOPIC2 | TOPIC3 |
|------------|--------|--------|---------|---------|
| Object Analytics: Scenario 1 | tnsaxis:CameraApplicationPlatform | tnsaxis:ObjectAnalytics | tnsaxis:Device1Scenario1 | |
| Object Analytics: Any Scenario | tnsaxis:CameraApplicationPlatform | tnsaxis:ObjectAnalytics | tnsaxis:Device1ScenarioANY | |
| VMD 4: Any Profile | tnsaxis:CameraApplicationPlatform | tnsaxis:VMD | tnsaxis:Camera1ProfileANY | |
| VMD 4: Profile 1 | tnsaxis:CameraApplicationPlatform | tnsaxis:VMD | tnsaxis:Camera1Profile1 | |
| Motion | tns1:RuleEngine | tns1:MotionRegionDetector | tns1:Motion | |
| VMD 3 | tns1:RuleEngine | tnsaxis:VMD3 | tnsaxis:vmd3_video_1 | |
| Node-RED:Event | tnsaxis:CameraApplicationPlatform | tnsaxis:Node-RED | tnsaxis:event | |
| Node-RED:State | tnsaxis:CameraApplicationPlatform | tnsaxis:Node-RED | tnsaxis:state | |
| Node-RED:Data | tnsaxis:CameraApplicationPlatform | tnsaxis:Node-RED | tnsaxis:data | |
| MQTT Trigger | tnsaxis:MQTT | tnsaxis:Message | tnsaxis:Stateless | |
| Virtual input | tns1:Device | tnsaxis:IO | tnsaxis:VirtualInput | |
| Manual trigger | tns1:Device | tnsaxis:IO | tnsaxis:VirtualPort | |
| Digital output port | tns1:Device | tnsaxis:IO | tnsaxis:OutputPort | |
| Digital input port | tns1:Device | tnsaxis:IO | tnsaxis:Port | |
| Digital Input | tns1:Device | tns1:Trigger | tns1:DigitalInput | |
| Live stream accessed | tns1:VideoSource | tnsaxis:LiveStreamAccessed | | |
| Camera tampering | tns1:VideoSource | tnsaxis:Tampering | | |
| Day night vision | tns1:VideoSource | tnsaxis:DayNightVision | | |
| Scheduled event | tns1:UserAlarm | tnsaxis:Recurring | tnsaxis:Interval | |
| Recording ongoing | tnsaxis:Storage | tnsaxis:Recording | | |
| Within operating temperature | tns1:Device | tnsaxis:Status | tnsaxis:Temperature | tnsaxis:Inside |
| Above operating temperature | tns1:Device | tnsaxis:Status | tnsaxis:Temperature | tnsaxis:Above |
| Above or below operating temperature | tns1:Device | tnsaxis:Status | tnsaxis:Temperature | tnsaxis:Above_or_below |
| Below operating temperature | tns1:Device | tnsaxis:Status | tnsaxis:Temperature | tnsaxis:Below |
| Relays and Outputs | tns1:Device | tns1:Trigger | tns1:Relay | |

---

## Example: Main Loop with Event Handling

```c
#include 
#include 
#include 
#include 
#include 
#include 

#define LOG(fmt, args...) { syslog(LOG_INFO, fmt, ## args); printf(fmt, ## args);}
#define APP_PACKAGE "base"

static GMainLoop *main_loop = NULL;
static volatile sig_atomic_t shutdown_flag = 0;

void term_handler(int signum) {
    if (signum == SIGTERM) {
        shutdown_flag = 1;
        if (main_loop) {
            g_main_loop_quit(main_loop);
        }
    }
}

void cleanup_resources(void) {
    LOG("Performing cleanup before shutdown\n");
    closelog();
}

int main(void) {
    struct sigaction action;
    memset(&action, 0, sizeof(struct sigaction));
    action.sa_handler = term_handler;
    sigaction(SIGTERM, &action, NULL);

    openlog(APP_PACKAGE, LOG_PID|LOG_CONS, LOG_USER);

    ACAP(APP_PACKAGE, Settings_Updated_Callback);

    main_loop = g_main_loop_new(NULL, FALSE);

    atexit(cleanup_resources);
    g_main_loop_run(main_loop);

    if (shutdown_flag) {
        LOG("Received SIGTERM signal, shutting down gracefully\n");
    }

    if (main_loop) {
        g_main_loop_unref(main_loop);
    }
    return 0;
}
```


---

## Additional Resources

- [Axis ACAP SDK Documentation](https://axiscommunications.github.io/acap-documentation/)
- [ACAP Example Projects](https://github.com/AxisCommunications/acap-native-sdk-examples)
- [ACAP Template Projects](https://github.com/pandosme/make_acap)

---

This README is designed for clarity, completeness, and easy parsing by both human developers and LLM-based assistants. It includes detailed explanations, code snippets, and tables for event topics, ensuring a robust foundation for ACAP development[1][3][4].

Citations:  
[1] https://github.com/AxisCommunications/acap-native-sdk-examples
[2] https://github.com/AxisCommunications/acap3-examples
[3] https://github.com/pandosme/make_acap  
[4] https://github.com/AxisCommunications/acap-native-sdk-examples  
[5] https://axiscommunications.github.io/acap-documentation/3.5/api/axevent/html/ax__event__key__value__set_8h.html  
[6] https://axiscommunications.github.io/acap-documentation/3.5/api/axevent/html/ax_event_stateless_declaration_example_8c-example.html  
[7] https://developer.axis.com/acap/introduction/acap-sdk-overview/  
[8] https://developer.axis.com/computer-vision/computer-vision-on-device/develop-your-own-deep-learning-application/  
[9] https://experienceleague.adobe.com/en/docs/experience-platform/assurance/view/event-transactions  
[10] https://developer.axis.com/vapix/applications/fence-guard/  
[11] https://www.acapcommunity.org/wp-content/uploads/2023/06/Technical-Notes-Pre-Event-Looping-Slide-Deck-Updated_23Jun2023.pdf  
