# MQTT Template

## Overview

This ACAP template provides an MQTT client that runs alongside the device's built-in MQTT client as an independent instance. It demonstrates broker connection management, publish/subscribe, and TLS certificate handling — all configurable through a web interface.

> For complete API documentation, see [doc/ACAP.md](../doc/ACAP.md).
> For a comprehensive catalog of available device events, see [doc/EVENTS.md](../doc/EVENTS.md).

Key points:

1. Uses the same MQTT libraries as the firmware client to create an additional MQTT client instance.
2. The web UI provides broker configuration, certificate management, and a message publishing form.
3. Customize by editing `main.c` — see `Main_MQTT_Status()` for connection lifecycle and `Main_MQTT_Subscription_Message()` for incoming messages.
4. Default MQTT settings are in `app/settings/mqtt.json`.
5. For the MQTT C API, inspect `MQTT.h`.

## Project Structure

```
mqtt/
├── app/
│   ├── ACAP.c / ACAP.h          # SDK wrapper (do not modify)
│   ├── cJSON.c / cJSON.h        # JSON library
│   ├── main.c                   # Application logic
│   ├── MQTT.c / MQTT.h          # MQTT client wrapper
│   ├── CERTS.c / CERTS.h        # TLS certificate management
│   ├── MQTTAsync.h / MQTTClient.h / ...  # Paho MQTT headers
│   ├── Makefile
│   ├── manifest.json
│   ├── html/
│   │   ├── index.html           # Main dashboard
│   │   ├── mqtt.html            # Broker config & publish UI
│   │   ├── certificate.html     # TLS certificate upload
│   │   ├── about.html           # App info
│   │   ├── css/
│   │   └── js/
│   └── settings/
│       ├── settings.json        # App configuration
│       ├── events.json          # Event declarations
│       └── mqtt.json            # MQTT broker configuration
├── Dockerfile
├── build.sh
└── README.md
```

## HTTP Endpoints

| Endpoint | Method | Description |
|----------|--------|-------------|
| `/local/basemqtt/app` | GET | App metadata, settings, device info |
| `/local/basemqtt/settings` | GET/POST | App configuration |
| `/local/basemqtt/status` | GET | Runtime status |
| `/local/basemqtt/mqtt` | GET/POST | MQTT broker configuration |
| `/local/basemqtt/certs` | GET/POST | TLS certificate management |
| `/local/basemqtt/publish` | POST | Publish an MQTT message |

### Publish Endpoint

```
POST /local/basemqtt/publish
Content-Type: application/json

{"topic": "my/topic", "payload": "Hello World"}
```

## Configuration

### settings/mqtt.json

```json
{
    "connect": false,
    "address": "",
    "port": "1883",
    "user": "",
    "password": "",
    "clientID": "",
    "preTopic": "mqttdemo",
    "tls": false,
    "verify": false,
    "lwt": null,
    "announce": null,
    "payload": { "name": "", "location": "" }
}
```

| Property | Type | Description |
|----------|------|-------------|
| `connect` | bool | Enable/disable broker connection |
| `address` | string | Broker hostname or IP |
| `port` | string | Broker port (1883 or 8883 for TLS) |
| `user` / `password` | string | Broker credentials |
| `clientID` | string | MQTT client identifier (auto-generated if empty) |
| `preTopic` | string | Prefix prepended to all published topics |
| `tls` | bool | Enable TLS encryption |
| `verify` | bool | Verify server certificate |
| `payload` | object | Default payload metadata |

## Code Examples

### Publishing a Message

```c
MQTT_Publish("my/topic", "Hello World", 0, 0);

// Or publish a JSON object
cJSON* msg = cJSON_CreateObject();
cJSON_AddStringToObject(msg, "status", "online");
MQTT_Publish_JSON("status/device", msg, 0, 1);
cJSON_Delete(msg);
```

### Connection Lifecycle Callback

```c
void Main_MQTT_Status(int state) {
    switch (state) {
        case MQTT_CONNECTED:
            LOG("Connected to broker\n");
            break;
        case MQTT_DISCONNECTED:
            LOG("Disconnected\n");
            break;
    }
}
```

### Subscription Message Callback

```c
void Main_MQTT_Subscription_Message(const char *topic, const char *payload) {
    LOG("Message: %s %s\n", topic, payload);
}

// Subscribe in main()
MQTT_Subscribe("my_topic");
```

## Building

```bash
cd mqtt
./build.sh
```

Builds `.eap` packages for both aarch64 and armv7hf. Install on your Axis device via the web UI or AXIS Device Manager.

## Customisation

1. Rename the package: update `PROG1` in `Makefile`, `APP_PACKAGE` in `main.c`, and `appName` in `manifest.json` (all three must match).
2. Edit `Main_MQTT_Status()` to add actions on connect/disconnect.
3. Edit `Main_MQTT_Subscription_Message()` to process incoming messages.
4. Add subscriptions with `MQTT_Subscribe("topic")` in `main()`.

## Version History

### 4.5.0
- ACAP.h/ACAP.c updated to v4.0: opaque HTTP types, `ACAP_Init()`, body getters
- CERTS.c updated for v4.0 opaque types and body getters
- Fixed memory leak in `HTTP_ENDPOINT_Publish`
- Makefile cleanup: removed unused vdostream dependency

### 4.3.1
- Bumped ACAP.c to v3.6

### 4.3.0
- Added location/VAPIX APIs, removed axParameter references
