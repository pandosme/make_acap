# Make ACAP
Working with the ACAP ASK API is not trivial.  Also, the SDK API may change over time.  A solution to both challanges is to use an ACAP SDK Wrapper that provides abstraction layer API.  One such Layer is ACAP.h and ACAP.c.
Including the files in the project allows faster development as you can focus on the use case.  If future ACAP SDK version introduces breaking changes, you only need to adjut the ACAP.c in order to support future versions.

# Template starting Point
## Base
The standard base for majority ACAP including
- HTTP (based on fastgci) incuding GET, POST and file transfer
- Image capture
- Managing ACAP configuration parameters
- Decalreing and Fire events
- Web Interface with video streaming capabilities

## Event Subscription
- Simple declarations of events to be subscribed to
- Simple callback to process events

## MQTT
- MQTT Client
  * Using the same MQTT libraries as the Axis Device MQTT client
  * Does not impact the device MQTT client
- Simple configuration
- Simple Publish and Subscribe functions

# Overview
The ACAP SDK provide interface for services in the Axis Device including
* HTTP Endpoints
* Evenent management system in order for an ACAP service to fire events or subscribe to events fired by other services
* Configuration management
* Image captureing and image processing
* PTZ control
* I/O management
* SD-Card storage
* AI model inference (Larod)
* Curl
* OpenCV

Detaile information can be found at https://axiscommunications.github.io/acap-documentation/ 

## Prerequisites
* One or more Axis Devices supporting the ACAP addons.
* A PC that have Docker installed
* Knowldge in C programming (embedded development)
* Knowldge in HTML, JavaScript and CSS for the ACAP user interface

# Template project description

## Directory structure
```
.
├── app
│   ├── ACAP.c
│   ├── ACAP.h
│   ├── main.c
│   ├── cJSON.c
│   ├── cJSON.h
│   ├── html
│   │   ├── config
│   │   │   ├── events.json
│   │   │   └── settings.json
│   │   ├── index.html
│   │   ├── css
│   │   │   └── style.css
│   │   └── js
│   │       └── app.js
│   └── manifest.json
├── Dockerfile
└── README.md
```

## Dockerfile
The development tool-cahin is provided ba Axis as images, containing all services to compile and package an ACAP.  The Dockerfile defines which Axis ACAP development image to use targeting various platforms

To build an ACAP the user runns the continer with the following command
```
docker build --build-arg ARCH=aarch64 --tag acap .
```
The users can get the compile EAP file using
```
docker cp $(docker create acap):/opt/app ./build
cp build/*.eap .
```

### Example of a typical Dockerfile
```
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

## Makefile
In order to compile the ACAP Project there must be a Makefile.  The Makefile must include reference to all C-files and packages used by the Project.

### Typical Makefile
```
PROG1	= base
OBJS1	= main.c ACAP.c cJSON.c
PROGS	= $(PROG1)

PKGS = glib-2.0 gio-2.0 vdostream axevent fcgi

CFLAGS += $(shell PKG_CONFIG_PATH=$(PKG_CONFIG_PATH) pkg-config --cflags $(PKGS))
LDLIBS += $(shell PKG_CONFIG_PATH=$(PKG_CONFIG_PATH) pkg-config --libs $(PKGS))
LDLIBS  += -s -lm -ldl -laxparameter

all:	$(PROGS)

$(PROG1): $(OBJS1)
	$(CC) $(CFLAGS) $(LDFLAGS) $^ $(LDLIBS) -o $@

clean:
	rm -rf $(PROGS) *.o $(LIBDIR) *.eap* *_LICENSE.txt manifest.json package.conf* param.conf
```

## manifest.json
The manifest defines sveral properties for the project and what resources it will use.  Here is a typical manifest.json that defines ACAP, name, version, user interface page and HTTP enpoint it exposes.
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
```

## Example code
The following examples use ACAP.h and ACAP.c SDK Wrappers for common integration with device services.
 
### main.c
main.c is where to add the code depending on use case.  The project may use GLIB.  This example use GLIB

```
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <glib.h>
#include <signal.h>


#define LOG(fmt, args...)    { syslog(LOG_INFO, fmt, ## args); printf(fmt, ## args);}

#define APP_PACKAGE	"base"

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
    
    // Initialize signal handling
    memset(&action, 0, sizeof(struct sigaction));
    action.sa_handler = term_handler;
    sigaction(SIGTERM, &action, NULL);
	
	openlog(APP_PACKAGE, LOG_PID|LOG_CONS, LOG_USER);

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

### ACAP Addon Configuration
Axis ACAP SDK provides ax_paramter.  It is not recommended to use this as to store Addon configuration params.  The ACAP.h and ACAP.c wroter provides a simple way to declare and manage configurations using JSON.
1. Add the file ./app/html/config/settings.json with a valid JSON structure representing the settings.
2. Initialize the ACAP Wrapper with a settings-changed-callback
The callback will be called when the ACAP starts in order to initialize internal variable;

```
void
Settings_Updated_Callback( const char *service, cJSON* data) {
	if( strcmp(serice,"settings") != 0 )
		return;

	cJSON* setting = data->child;
	while(setting) {
		if( strcmp( "param1", setting->string ) == 0 ) {
			LOG("Changed event param1 to %d\n", setting->valueint);
		}
		...
		setting = setting->next;
	}
}


int main(void) {
    struct sigaction action;
    
    // Initialize signal handling
    memset(&action, 0, sizeof(struct sigaction));
    action.sa_handler = term_handler;
    sigaction(SIGTERM, &action, NULL);
	
	openlog(APP_PACKAGE, LOG_PID|LOG_CONS, LOG_USER);

	ACAP( APP_PACKAGE, Settings_Updated_Callback );

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

### HTTP Endpoints
In order to have one or more HTTP endpoints interface to an ACAP addon, the following must be added:
1. Define the enpoint name in manifest.json
2. Register the endpoint in the code with an an HTTP enpoint callback
3. Add the HTTP management to the main loop

Example:
```
void
My_Test_Endpoint_Callback(const ACAP_HTTP_Response response, const ACAP_HTTP_Request request) {
    const char* method = ACAP_HTTP_Get_Method(request);
    if (!method) {
        ACAP_HTTP_Respond_Error(response, 400, "Invalid Request Method");
        return;
    }

    if (strcmp(method, "GET") == 0) {
		//Process GET
		const char* someParam = ACAP_HTTP_Request_Param(request, "param1");
        int ACAP_HTTP_Respond_String(response, "Hello from GET %s", someParam?someParam:"");
		return;
	}

    if (strcmp(method, "POST") == 0) {
		//Process POST
		
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
        cJSON* bodyObject = cJSON_Parse(request->postData);
        if (!bodyObject) {
            ACAP_HTTP_Respond_Error(response, 400, "Invalid JSON data");
            return;
        }
		//Respond back with the same JSON 
		ACAP_HTTP_Respond_JSON(response, bodyObject);
		return;
	}
    ACAP_HTTP_Respond_Error(response, 400, "Invalid Request Method");
}


int main(void) {
   ...
	ACAP( APP_PACKAGE, Settings_Updated_Callback );
	ACAP_HTTP_Node( "test", My_Test_Endpoint_Callback );

	g_idle_add(ACAP_HTTP_Process, NULL);  //Process HTTP request
	main_loop = g_main_loop_new(NULL, FALSE);
...
}
```

### Event Fire
An ACAP service will typically fire an event when something occurs/detected in the ACAP Service.  
1. Add file ./app/html/config/events.json that defined the events.  Events may be triggere or stateful (having high and low states)
2. Initialize the ACAP wrapper
2. Call the fire events finctions in ACAP.h and ACAP.c

Typical events.json
```
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
In your code
```
	ACAP_EVENTS_Fire_State( "theStateEvent", 1 );
	ACAP_EVENTS_Fire_State( "theStateEvent", 0 );
	ACAP_EVENTS_Fire( "theTriggerEvent" );
```

## Get started Quickly
1. Clone ACAP Template Directory
```git clone https://github.com/pandosme/make_acap.git```
2. Go to selected project
```cd make_acap/base```
3. Build ACAP
```. build.sh```
4. Install ACAP
5. Alter to code and make a custom service
- main.c
- manifest.json
- Makefile



## Additional Resources
- [Axis ACAP SDK Documentation](https://www.axis.com/developer-community)
- [ACAP Developer Guide](https://developer.axis.com)
- [ACAP Template projects by Fred Juhlin](https://github.com/pandosme/make_acap)
- [Axis Communications GitHub of examples] (https://github.com/AxisCommunications/acap-native-sdk-examples)

