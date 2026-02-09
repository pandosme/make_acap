# make_acap — ACAP Template Projects

## Overview

ACAP is an open application platform that enables you to develop and deploy applications directly on Axis network devices. This repository provides template projects with a C wrapper (`ACAP.c`/`ACAP.h`) that simplifies SDK interactions for HTTP endpoints, configuration management, event handling, and more.

## Documentation

| Document | Description |
|----------|-------------|
| [doc/ACAP.md](doc/ACAP.md) | **Complete technical reference** — ACAP.h API, build configuration, code patterns, and full reference implementations for all templates |
| [doc/EVENTS.md](doc/EVENTS.md) | Comprehensive catalog of Axis device events with namespaces, properties, and state types |
| [doc/VDO.md](doc/VDO.md) | VDO (Video Device Object) API reference for video capture and streaming |
| Template READMEs | Each template directory contains its own README with specific usage, configuration, and endpoint documentation |

For LLM-assisted development, provide [doc/ACAP.md](doc/ACAP.md) as context.

## Templates

| Template | Description |
|----------|-------------|
| [base](base/) | Full-featured template with web UI, HTTP endpoints, image capture, configuration settings, and event firing |
| [event_subscription](event_subscription/) | Demonstrates static event monitoring using topic subscriptions declared in `subscriptions.json` |
| [event_selection](event_selection/) | Dynamic event selection — users choose which event to monitor (or configure a timer) via a web UI; re-publishes its own declared events for downstream consumption |
| [mqtt](mqtt/) | MQTT client integration with broker configuration, publish/subscribe, TLS certificate management |

## Prerequisites

- Axis device supporting ACAP
- Docker installed on your development PC
- C programming knowledge (embedded focus)
- Familiarity with HTML/JS/CSS for UI development

## Quick Start

1. **Clone the repository**
   ```bash
   git clone https://github.com/pandosme/make_acap.git
   cd make_acap/base
   ```
2. **Build the ACAP**
   ```bash
   ./build.sh
   ```
3. **Install on device**
   Upload the resulting `.eap` file via AXIS Device Manager or the device's web interface.
4. **Customise**
   Edit `main.c`, `manifest.json`, and `Makefile` to implement your service logic. See [doc/ACAP.md](doc/ACAP.md) for the full API reference and code patterns.

## Project Structure

Each template follows this structure:

```
template_name/
├── app/
│   ├── ACAP.c / ACAP.h       # SDK wrapper (do not modify)
│   ├── main.c                # Application logic (edit this)
│   ├── cJSON.c / cJSON.h     # JSON library
│   ├── Makefile              # Build configuration
│   ├── manifest.json         # ACAP package metadata
│   ├── settings/             # JSON configuration files
│   └── html/                 # Web UI assets
├── Dockerfile                # Docker build environment
├── build.sh                  # Build helper script
└── README.md                 # Template-specific documentation
```

## Building

The build uses Docker with the Axis ACAP Native SDK 12.2.0. The `build.sh` script builds packages for both aarch64 and armv7hf architectures.

```bash
cd base  # or event_subscription, event_selection, mqtt
./build.sh
```

For manual single-architecture builds:

```bash
docker build --build-arg ARCH=aarch64 --tag acap .
docker cp $(docker create acap):/opt/app ./build
cp build/*.eap .
```

## Additional Resources

- [Axis ACAP SDK Documentation](https://developer.axis.com/acap/api)
- [ACAP Native SDK Examples](https://github.com/AxisCommunications/acap-native-sdk-examples)
- [ACAP Application Structure](https://developer.axis.com/acap/acap-sdk-version-3/develop-applications/application-project-structure/#manifest-file)

