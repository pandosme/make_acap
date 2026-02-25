# ACAP SDK Base Project Template

## Overview

This project provides a comprehensive template for developing applications using the **Axis Camera Application Platform (ACAP) SDK version 4.0**. The template demonstrates key features and best practices for ACAP application development.

> For complete API documentation, see [doc/ACAP.md](../doc/ACAP.md).
> For a comprehensive catalog of available device events, see [doc/EVENTS.md](../doc/EVENTS.md).

## 🚀 Key Features

- **HTTP CGI Endpoints**
- **Interactive HTML Page with Video Stream**
- **Device Parameter Configuration**
- **Event Handling Mechanisms**

## 📂 Project Structure
```
base/
├── app/
│   ├── ACAP.c / ACAP.h      # SDK wrapper (do not modify)
│   ├── cJSON.c / cJSON.h    # JSON parser
│   ├── main.c               # Application logic (edit here)
│   ├── Makefile
│   ├── manifest.json
│   ├── html/                 # Web UI
│   │   ├── index.html, config.html, events.html, image.html, about.html
│   │   ├── css/
│   │   └── js/
│   └── settings/
│       ├── settings.json     # App configuration
│       └── events.json       # Event declarations
├── Dockerfile
├── build.sh
└── README.md
```

## 🛠 Key Components

### 1. ACAP Wrapper (`ACAP.c` / `ACAP.h`)
- Simplifies resource initialization
- Provides helper functions for:
  - HTTP handling
  - Event management
  - Device information retrieval

### 2. HTTP Endpoints
Configured in `manifest.json`, supporting:
- `app`: Package configuration retrieval
- `settings`: Parameter management
- `capture`: Image snapshot generation
- `fire`: Event triggering

### 3. Event Handling
Demonstrates two event types:
- **Stateful Events**: Maintain state
- **Trigger Events**: One-time occurrences

## 🔧 Development Setup

### Prerequisites
- Axis Camera with ACAP SDK v4.0
- Docker (recommended for compilation)

### Compilation Steps
```bash
. build.sh
```

## 💡 Key Code Highlights

### Event Firing Example
```c
// Trigger a state event
ACAP_EVENTS_Fire_State("state", 1);

// Fire a trigger event
ACAP_EVENTS_Fire("trigger");
```

### HTTP Endpoint Handler
```c
void HTTP_Endpoint_capture(const ACAP_HTTP_Response response, 
                           const ACAP_HTTP_Request request) {
    // Capture and return JPEG snapshot
    VdoBuffer* buffer = vdo_stream_snapshot(vdoSettings, &error);
    ACAP_HTTP_Respond_Data(response, size, data);
}
```

## 🤝 Contributing

1. Fork the repository
2. Create a feature branch
3. Commit your changes
4. Push and submit a pull request

## ⚖️ License

MIT


## 📚 Additional Resources

- [Axis ACAP SDK Documentation](https://developer.axis.com/acap/api)
- [ACAP Native SDK Examples](https://github.com/AxisCommunications/acap-native-sdk-examples)

> **Note**: This template is designed to accelerate ACAP application development by providing a structured, feature-rich starting point.

## Version History

### 4.5.0
- ACAP.h/ACAP.c updated to v4.0: opaque HTTP types, `ACAP_Init()`, body getters, `ACAP_Version()`
- Makefile cleanup: correct PKGS (vdostream glib-2.0 gio-2.0 axevent fcgi libcurl)
- main.c updated: v4.0 API, proper `free()` for `ACAP_HTTP_Request_Param()` results

### 4.3.1
- Bumped ACAP.c to version 3.6

### 4.3.0
- Added location/VAPIX APIs, removed axParameter references
