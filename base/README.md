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

- [Axis ACAP SDK Documentation](https://www.axis.com/developer-community)
- [ACAP Developer Guide](https://developer.axis.com)

> **Note**: This template is designed to accelerate ACAP application development by providing a structured, feature-rich starting point.

### 4.3.1	February 22, 2025
- Bumbed ACAP.c to version 3.6

### 4.3.0	December 20, 2024
- Additional ACAP bindings
	* ACAP_DEVICE_Longitude();
	* ACAP_DEVICE_Latitude();
	* ACAP_DEVICE_Set_Location( double lat, double lon);
	* ACAP_VAPIX_Get(const char *request);
	* ACAP_VAPIX_Post(const char *request, const char* body );
- Removed all references to axParameter
