# ACAP SDK v4.0 Project Template

## Overview

This template is designed to accelerate ACAP application development by providing a structured, 
feature-rich starting point.  This project demonstrates how an Axis ACAP can monitor events occuring in the device.
The events are logged to the syslog.  By parsing the cJSON data you can add different actions when different evetns occur.


## 📂 Project Structure
The project can be cloned from https://github.com/pandosme/make_acap/tree/main/demo_template_sdk4
```
.
├── app
│   ├── ACAP.c
│   ├── ACAP.h
│   ├── main.c
│   ├── cJSON.c
│   ├── cJSON.h
│   ├── html
│   │   └── config
│   │       └── subscriptions.json
│   └── manifest.json
├── Dockerfile
└── README.md
```

## 🛠 Key Components

### 1. main.c
- Initialize the ACAP
- Starting point for code development
- Provides helper functions for:
  - HTTP handling
  - Event management
  - Device information retrieval

### 2. `ACAP.c` / `ACAP.h` (ACAP Wrapper)
- Simplifies resource initialization
- Provides helper functions for:
  - HTTP handling
  - Event management
  - Device information retrieval
THere is no need to edit these files.  ACAP.h should be analyzed to understand how to interact with resources.

### 3. subscriptions.json
Declares the events or topics that the ACAP shall describe on.  Events are grouped into topic nodes
Example subscription declarations of three event subscriptions.

```
[
	{
		"name": "All ACAP Events",
		"topic0": {"tnsaxis":"CameraApplicationPlatform"}
	},
	},
	{
		"name": "Camera Day and Night Switching",
		"topic0": {"tns1":"VideoSource"},
		"topic1": {"tnsaxis":"DayNightVision"}
	},
	{
		"name": "Virtual Input",
		"topic0": {"tns1":"Device"},
		"topic1": {"tnsaxis":"IO"},
		"topic2": {"tnsaxis":"VirtualInput"}
	}
]
```
Typical JSON output in events callback



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


### 3. HTTP Endpoints
Configured in `manifest.json`, supporting:
- `app`: Package configuration retrieval
- `settings`: Parameter management
Note that this project does not have any configuration but the API are still avaialable

## 💡 Key Code Highlights

void My_Event_Callback(cJSON *event) {
	char* json = cJSON_PrintUnformatted(event);
	LOG("%s: %s\n", __func__,json);
	free(json);
}

//Initialize all event subscriptions
ACAP_EVENTS_SetCallback( My_Event_Callback );
cJSON* list = ACAP_FILE_Read( "html/config/subscriptions.json" );
cJSON* event = list->child;
while(event){
	ACAP_EVENTS_Subscribe(event);
	event = event->next;
}
cJSON_Delete(list);

## 🔧 Development Setup

### Prerequisites
- Axis Camera with ACAP SDK v4.0
- Docker (recommended for compilation)
- Git (for cloneing directory)

### Compilation Steps
```
git clone git@github.com:pandosme/make_acap.git
cd demo_event_subscription_sdk4
. build.sh
```
Install the appropriate EAP file (armv7hf or aarch64) in the Axis device.





