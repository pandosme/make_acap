# ACAP SDK v4.0 Project Template

## Overview

This project provides a comprehensive template for developing applications using the **Axis Camera Application Platform (ACAP) SDK version 4.0**. The template demonstrates key features and best practices for ACAP application development.

## 🚀 Key Features

- **HTTP CGI Endpoints**
- **Interactive HTML Page with Video Stream**
- **Device Parameter Configuration**
- **Event Handling Mechanisms**

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
│   │   ├── config
│   │   │   ├── events.json
│   │   │   └── settings.json
│   │   ├── index.html
│   │   └── js/css assets
│   └── manifest.json
├── Dockerfile
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

