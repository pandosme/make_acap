# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Overview

This repository contains ACAP (Axis Camera Application Platform) template projects for building applications on Axis network devices. It uses Docker-based builds with the ACAP Native SDK 12.2.0 and includes a C wrapper (ACAP.c/ACAP.h) that simplifies common SDK interactions.

## Documentation Map

| File | Purpose |
|------|---------|
| [README.md](README.md) | Project overview, template list, quick start |
| [doc/ACAP.md](doc/ACAP.md) | **Complete technical reference** — full ACAP.h API, code patterns, and reference implementations for all templates |
| [doc/EVENTS.md](doc/EVENTS.md) | Comprehensive catalog of Axis device events with namespaces, properties, and state types |
| [doc/VDO.md](doc/VDO.md) | VDO (Video Device Object) API reference for video capture and streaming |
| Template `README.md` files | Template-specific configuration, endpoints, and usage |

**Always consult [doc/ACAP.md](doc/ACAP.md) before making code changes.** It contains the complete API, all code patterns, and working examples for every template.

## Build Commands

Navigate to a template directory and run:

```bash
cd base  # or event_subscription, event_selection, mqtt
./build.sh
```

This builds `.eap` packages for both aarch64 and armv7hf architectures. The build script uses Docker with the Axis ACAP Native SDK 12.2.0.

### Manual Docker Build (single architecture)

```bash
docker build --build-arg ARCH=aarch64 --tag acap .
docker cp $(docker create acap):/opt/app ./build
cp build/*.eap .
```

## Critical Rules

1. **DO NOT modify** `ACAP.c`, `ACAP.h`, `cJSON.c`, or `cJSON.h` — these are shared library files.
2. All customisation happens in `main.c`, `manifest.json`, `Makefile`, settings files, and HTML assets.
3. `PROG1` in Makefile, `APP_PACKAGE` in main.c, and `appName` in manifest.json **must all match**.
4. When adding `.c` files, add them to `OBJS1` in Makefile.
5. New HTTP endpoints must be declared in both `manifest.json` (`httpConfig`) and registered in code via `ACAP_HTTP_Node()`.
6. When adding new library dependencies, add them to `PKGS` in Makefile.

## Memory Management

These conventions are critical — violating them causes crashes or leaks:

| Function | Returns | Ownership |
|----------|---------|-----------|
| `ACAP_Get_Config()` | Internally managed `cJSON*` | **DO NOT** `cJSON_Delete()` |
| `ACAP_STATUS_*()` getters | Internally managed `cJSON*` | **DO NOT** `cJSON_Delete()` |
| `ACAP_DEVICE_JSON()` | Internally managed `cJSON*` | **DO NOT** `cJSON_Delete()` |
| `ACAP_FILE_Read()` | Newly allocated `cJSON*` | **MUST** `cJSON_Delete()` |
| `ACAP_VAPIX_Get()` / `ACAP_VAPIX_Post()` | Allocated `char*` | **MUST** `free()` |
| `ACAP_HTTP_Request_Param()` | Allocated `char*` | **MUST** `free()` |
| `cJSON_PrintUnformatted()` / `cJSON_Print()` | Allocated `char*` | **MUST** `free()` |

## Template Overview

| Template | Key Feature | Extra Source Files | Extra PKGS |
|----------|-------------|-------------------|------------|
| `base` | UI, image capture, events | VDO headers | `vdostream vdo-frame vdo-types` |
| `event_subscription` | Static event monitoring | — | — |
| `event_selection` | Dynamic event/timer trigger UI | TomSelect, SOAP discovery | — |
| `mqtt` | MQTT pub/sub client | `MQTT.c/h`, `CERTS.c/h` | `libcurl` |

See each template's `README.md` for specific configuration and endpoint details.
