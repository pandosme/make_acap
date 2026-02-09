# Event Selection

## Overview

Event Selection is an ACAP template that lets users dynamically select which device event to monitor — or configure a periodic timer — through a web interface.  When the selected event fires (or the timer elapses), the ACAP re-publishes its own declared events so that other ACAPs, action rules, or external systems can react to them.

This is useful when you need an ACAP that acts as a configurable trigger source.  Instead of hard-coding event subscriptions at build time, users choose the trigger source at runtime from a searchable list of every event the device supports.

### Comparison with event_subscription

| | event_subscription | event_selection |
|---|---|---|
| Event subscriptions | Static — declared in `subscriptions.json` at build time | Dynamic — user selects at runtime via the web UI |
| Timer trigger | Not supported | Built-in with configurable interval and unit |
| Re-published events | None | Fires its own stateful + stateless events |
| Use case | Monitoring / logging multiple events | Configurable single-event trigger source |

## Features

- **Dynamic Event Discovery** — The web UI queries the device's VAPIX/SOAP interface to list every available event, presented in a searchable TomSelect dropdown.
- **Three Trigger Modes**
  - **Disabled** — No monitoring or timer active.
  - **Event-Based** — Subscribes to one user-selected device event.  When the event fires, the ACAP re-publishes its own declared events.
  - **Timer-Based** — Fires the declared events on a periodic interval. The user sets the value and unit (seconds, minutes, or hours).
- **Declared Events** — Other services can subscribe to these ACAP events without knowing which underlying device event is being monitored:
  - *Event Selection: State* (stateful) — mirrors the active/inactive state of the monitored event.
  - *Event Selection: Trigger* (stateless) — fires each time the trigger condition occurs.
- **Live Status Panel** — The UI polls the status endpoint every 5 seconds showing current trigger type, subscribed event, interval, active state, and last trigger timestamp.
- **Manual Trigger** — A test button (or `POST /trigger`) lets you fire the events on demand for integration testing.
- **Smart State Extraction** — Handles the wide variety of Axis event property names (`active`, `state`, `triggered`, `LogicalState`, `Detected`, `Open`, etc.) to correctly extract a boolean state from any incoming event.

## Project Structure

```
event_selection/
├── app/
│   ├── ACAP.c / ACAP.h          # SDK wrapper (do not modify)
│   ├── cJSON.c / cJSON.h        # JSON library
│   ├── main.c                   # Application logic
│   ├── Makefile
│   ├── manifest.json
│   ├── html/
│   │   ├── index.html           # Trigger configuration UI
│   │   ├── about.html           # Application info page
│   │   ├── css/
│   │   │   ├── app.css
│   │   │   ├── bootstrap.min.css
│   │   │   └── tom-select.bootstrap5.min.css
│   │   └── js/
│   │       ├── events.js                  # SOAP event discovery
│   │       ├── toast.js                   # Notification helper
│   │       ├── tom-select.complete.min.js # Searchable dropdown
│   │       ├── bootstrap.bundle.min.js
│   │       ├── jquery-3.7.1.min.js
│   │       └── media-stream-player.min.js
│   └── settings/
│       ├── settings.json         # Trigger configuration
│       ├── events.json           # Declared ACAP events
│       └── subscriptions.json    # Empty (subscriptions are dynamic)
├── Dockerfile
├── build.sh
└── README.md
```

## How It Works

1. **Startup** — `main.c` initialises the ACAP wrapper, registers the `/trigger` HTTP endpoint, sets the event callback, and calls `Apply_Trigger()` which reads `settings.json` and sets up the configured trigger mode.
2. **Event mode** — `Apply_Trigger()` calls `ACAP_EVENTS_Subscribe()` with the topic filter stored in `triggerEvent`.  When the event fires, `My_Event_Callback()` extracts the boolean state and re-publishes the declared events.
3. **Timer mode** — A GLib timer (`g_timeout_add_seconds`) calls `Timer_Callback()` at the configured interval, firing the declared events on each tick.
4. **Settings change** — When the user saves a new configuration via the web UI (`POST /settings`), `Settings_Updated_Callback()` fires and calls `Apply_Trigger()` again.  The previous subscription or timer is cleaned up before the new one is applied.  No restart required.

## Configuration

### settings.json

```json
{
    "triggerType": "none",
    "triggerEvent": null,
    "timer": 60
}
```

| Property | Type | Values | Description |
|----------|------|--------|-------------|
| `triggerType` | string | `"none"`, `"event"`, `"timer"` | Active trigger mode |
| `triggerEvent` | object / null | `{ "name": "...", "topic0": {...}, ... }` | Event topic filter selected by the user |
| `timer` | number | Positive integer (seconds) | Timer interval; the UI converts minutes/hours to seconds |

### events.json

Two events are declared for other services or action rules to subscribe to:

| Event | ID | Type | Description |
|-------|----|------|-------------|
| Event Selection: State | `state` | Stateful | Mirrors the active/inactive state of the monitored event |
| Event Selection: Trigger | `trigger` | Stateless | Fires each time the event triggers or the timer elapses |

## HTTP Endpoints

| Endpoint | Method | Description |
|----------|--------|-------------|
| `/local/event_selection/app` | GET | Returns app metadata, settings, and device info |
| `/local/event_selection/settings` | GET | Returns current settings |
| `/local/event_selection/settings` | POST | Updates trigger configuration (JSON body) |
| `/local/event_selection/status` | GET | Returns current trigger status |
| `/local/event_selection/trigger` | GET | Returns trigger status object |
| `/local/event_selection/trigger` | POST | Manually fires both declared events |

## Web Interface

### Trigger Configuration (index.html)

The main page provides:
- **Trigger Type** — Three radio buttons to select Disabled / Event Based / Timer Based.
- **Event Selection** — A TomSelect-powered searchable dropdown populated by querying the device's SOAP event list.  Events are sorted alphabetically and system-internal events (RecordingConfig, MediaClip, SystemMessage, Log, MQTT) are filtered out.
- **Timer Configuration** — A numeric input with a unit selector (seconds / minutes / hours).
- **Status Panel** — Live-updating card showing trigger type, event name, interval, active state, and last triggered timestamp.
- **Declared Events** — Information card listing the two ACAP events that other services can subscribe to.

### About (about.html)

Displays ACAP name, version, vendor, and device information.

## Building

```bash
cd event_selection
./build.sh
```

This builds `.eap` packages for both aarch64 and armv7hf.  Install the appropriate file on your Axis device via the web UI or AXIS Device Manager.

## Customisation

To use this as a starting point for your own ACAP:

1. Rename the package in `Makefile` (`PROG1`), `manifest.json` (`appName`), and `main.c` (`APP_PACKAGE`).
2. Edit `My_Event_Callback()` and `Timer_Callback()` in `main.c` to add your own actions when the trigger fires.
3. Add additional settings to `settings.json` and handle them in `Settings_Updated_Callback()`.
4. Modify the declared events in `events.json` if you need different event names or additional data fields.

## Version History

### 1.0.0
- Initial release
- Dynamic event selection via searchable dropdown (TomSelect + SOAP discovery)
- Event-based and timer-based trigger modes
- Declared stateful and stateless events for downstream consumption
- Real-time status monitoring with 5-second polling
- Manual trigger endpoint for testing
