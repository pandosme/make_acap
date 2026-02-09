# Event Subscription Template

## Overview

Demonstrates how an Axis ACAP can subscribe to and monitor device events.
Subscribed events are logged to syslog. By parsing the cJSON event data in the callback,
you can trigger different actions for different event types.

> For complete API documentation, see [doc/ACAP.md](../doc/ACAP.md).
> For a comprehensive catalog of available device events, see [doc/EVENTS.md](../doc/EVENTS.md).

## Project Structure

```
event_subscription/
├── app/
│   ├── ACAP.c / ACAP.h      # SDK wrapper (do not modify)
│   ├── cJSON.c / cJSON.h    # JSON parser
│   ├── main.c               # Application entry point
│   ├── Makefile
│   ├── manifest.json
│   ├── html/                 # Web interface
│   │   ├── index.html
│   │   ├── about.html
│   │   ├── css/
│   │   └── js/
│   └── settings/
│       ├── settings.json         # App configuration
│       ├── events.json           # Event declarations
│       └── subscriptions.json    # Event subscriptions
├── Dockerfile
├── build.sh
└── README.md
```

## Key Components

### main.c

Application entry point. Sets up event subscription callback and subscribes to events
defined in `settings/subscriptions.json`.

### ACAP.c / ACAP.h (SDK Wrapper)

Simplifies ACAP resource initialization and provides helper functions for HTTP handling,
event management, and device information retrieval. **Do not modify these files.**
See [doc/ACAP.md](../doc/ACAP.md) for the full API reference.

### settings/subscriptions.json

Declares which device events the ACAP subscribes to. Events are defined by topic node hierarchy.

Example with three subscriptions:
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

For a comprehensive listing of available events and their topic nodes, see [doc/EVENTS.md](../doc/EVENTS.md).

### HTTP Endpoints

Configured in `manifest.json`. Supports `app` (package configuration) and `settings`
(parameter management) endpoints. This template has no custom configuration pages but
the APIs are still available.

## Code Example

```c
void My_Event_Callback(cJSON *event) {
    char *json = cJSON_PrintUnformatted(event);
    LOG("%s: %s\n", __func__, json);
    free(json);
}

// Initialize all event subscriptions
ACAP_EVENTS_SetCallback(My_Event_Callback);
cJSON *list = ACAP_FILE_Read("localdata/subscriptions.json");
cJSON *event = list->child;
while (event) {
    ACAP_EVENTS_Subscribe(event);
    event = event->next;
}
cJSON_Delete(list);
```

## Build

### Prerequisites
- Docker
- Git

### Steps
```bash
cd event_subscription
. build.sh
```

Install the appropriate EAP file (armv7hf or aarch64) on the Axis device.

## Version History

| Version | Date | Changes |
|---------|------|---------|
| 4.3.1 | 2025-02-22 | Bumped ACAP.c to v3.6 |
| 4.3.0 | 2024-12-20 | Added location/VAPIX APIs, removed axParameter references |





