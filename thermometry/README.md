# Thermometry MQTT

## Overview

This ACAP is designed exclusively for Axis thermal cameras. It polls the device's Thermometry VAPIX API on a configurable interval and publishes temperature data — area statistics, spot meter readings, and threshold alarm events — to an MQTT broker.

> For complete API documentation, see [doc/ACAP.md](../doc/ACAP.md).
> For a comprehensive catalog of available device events, see [doc/EVENTS.md](../doc/EVENTS.md).

Key points:

1. Only install on Axis thermal cameras — the `thermometry.cgi` VAPIX endpoint is not available on standard cameras.
2. Publishes three independent data streams: temperature areas, spot temperature, and device events.
3. Each stream can be individually enabled or disabled from the web UI.
4. Temperature scale (Celsius/Fahrenheit) is applied directly to the camera via VAPIX on settings save.
5. Default MQTT settings are in `app/settings/mqtt.json` (`preTopic` defaults to `thermal`).

## Project Structure

```
thermometry/
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
│   │   ├── index.html           # Settings & live temperature dashboard
│   │   ├── mqtt.html            # Broker configuration & publish UI
│   │   ├── certificate.html     # TLS certificate upload
│   │   ├── about.html           # App info
│   │   ├── css/
│   │   └── js/
│   └── settings/
│       ├── settings.json        # App configuration
│       ├── events.json          # ACAP event declarations
│       ├── subscriptions.json   # Device event subscriptions
│       └── mqtt.json            # MQTT broker configuration
├── Dockerfile
├── build.sh
└── README.md
```

## HTTP Endpoints

| Endpoint | Method | Description |
|----------|--------|-------------|
| `/local/thermal/app` | GET | App metadata, settings, device info |
| `/local/thermal/settings` | GET/POST | App configuration |
| `/local/thermal/status` | GET | Runtime status (MQTT state, live temperatures) |
| `/local/thermal/mqtt` | GET/POST | MQTT broker configuration |
| `/local/thermal/certs` | GET/POST | TLS certificate management |
| `/local/thermal/publish` | POST | Publish an arbitrary MQTT message |

### Publish Endpoint

```
POST /local/thermal/publish
Content-Type: application/json

{"topic": "my/topic", "payload": "Hello World"}
```

## MQTT Topics

All topics use the device serial number to namespace data per-camera.

| Topic | Trigger | Payload |
|-------|---------|---------|
| `temperature/areas/{serial}` | Every poll interval (if enabled) | `{timestamp, serial, areas: [...]}` |
| `temperature/spot/{serial}` | Every poll interval (if enabled) | `{timestamp, serial, temperature, coordinates}` |
| `temperature/event/{serial}` | On device event (if enabled) | `{timestamp, serial, event: {...}}` |
| `connect/{serial}` | On MQTT connect/disconnect | `{connected, address, app}` |

### Area Payload Example

```json
{
  "timestamp": "2026-03-17T12:00:00Z",
  "serial": "ACCC8E123456",
  "areas": [
    {"id": 1, "avg": 24.5, "min": 22.1, "max": 27.3, "triggered": false}
  ]
}
```

### Spot Temperature Payload Example

```json
{
  "timestamp": "2026-03-17T12:00:00Z",
  "serial": "ACCC8E123456",
  "temperature": 31.2,
  "coordinates": {"x": 0.1, "y": -0.3}
}
```

## Configuration

### settings/settings.json

```json
{
  "pollInterval": 10,
  "publishAreas": true,
  "publishSpot": true,
  "publishEvents": true,
  "temperatureScale": "celsius"
}
```

| Property | Type | Default | Description |
|----------|------|---------|-------------|
| `pollInterval` | number | `10` | Seconds between VAPIX polls (minimum 5) |
| `publishAreas` | bool | `true` | Publish temperature area statistics |
| `publishSpot` | bool | `true` | Publish spot meter reading |
| `publishEvents` | bool | `true` | Forward device threshold events to MQTT |
| `temperatureScale` | string | `"celsius"` | `"celsius"` or `"fahrenheit"` — applied to camera on save |

### settings/mqtt.json

```json
{
  "connect": false,
  "address": "",
  "port": "1883",
  "user": "",
  "password": "",
  "clientID": "",
  "preTopic": "thermal",
  "tls": false,
  "verify": false,
  "lwt": null,
  "announce": null,
  "payload": {"name": "", "location": ""}
}
```

### settings/subscriptions.json

Defines the device events forwarded to MQTT when `publishEvents` is enabled:

| Event Name | Topic Path |
|------------|-----------|
| Temperature Area Alarm | `tns1:VideoSource/tns1:RadiometryAlarm/tnsaxis:TemperatureDetection` |
| Temperature Info | `tns1:VideoSource/tns1:Thermometry/tnsaxis:TemperatureDetection` |
| Temperature Deviation Alarm | `tns1:VideoSource/tns1:RadiometryAlarm/tnsaxis:DeviationDetection` |

When an event contains an `AlarmActive` field, a `temperature_alarm` ACAP state event is also fired locally.

## Building

```bash
cd thermometry
./build.sh
```

Builds `.eap` packages for both aarch64 and armv7hf. Install on your Axis thermal camera via the web UI or AXIS Device Manager.

## Customisation

1. Rename the package: update `PROG1` in `Makefile`, `APP_PACKAGE` in `main.c`, and `appName` in `manifest.json` (all three must match).
2. Add additional VAPIX thermometry calls in `main.c` using the `Thermometry_Call(method, params)` helper.
3. Modify `MQTT_Status_Callback()` to add actions on connect/disconnect.
4. Edit `subscriptions.json` to subscribe to different or additional device events.
