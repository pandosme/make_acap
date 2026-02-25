# make_acap — ACAP Template Projects

## Overview

ACAP is an open application platform that enables you to develop and deploy applications directly on Axis network devices. This repository provides template projects with a C wrapper (`ACAP.c`/`ACAP.h`) that simplifies SDK interactions for HTTP endpoints, configuration management, event handling, and more.

## Documentation

| Document | Description |
|----------|-------------|
| [doc/ACAP.md](doc/ACAP.md) | **Complete technical reference** — ACAP.h API, build configuration, code patterns, and full reference implementations for all templates |
| [doc/EVENTS.md](doc/EVENTS.md) | Comprehensive catalog of Axis device events with namespaces, properties, and state types |
| [doc/VDO.md](doc/VDO.md) | VDO (Video Device Object) API reference for video capture and streaming |
| [doc/STORAGE.md](doc/STORAGE.md) | Edge Storage API — SD card and NAS read/write with axstorage |
| Template READMEs | Each template directory contains its own README with specific usage, configuration, and endpoint documentation |

For LLM-assisted development, provide [doc/ACAP.md](doc/ACAP.md) as context. Agent-specific instruction files are included for all major AI coding assistants — see [AI Agent Support](#ai-agent-support) below.

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

## Development Rules

### Protected Files — Do Not Edit
`ACAP.c`, `ACAP.h`, `cJSON.c`, `cJSON.h` are shared library files identical across all templates.
Edit only: `main.c`, `manifest.json`, `Makefile`, `settings/`, `html/`.

### Naming Convention
These three values **must always match**:
- `PROG1` in `Makefile`
- `APP_PACKAGE` in `main.c`
- `appName` in `manifest.json`

### Template Selection

| Start With | When You Need |
|------------|---------------|
| `base` | HTTP endpoints, image capture, events, settings, web UI |
| `event_subscription` | Static event monitoring (topics defined in `subscriptions.json`) |
| `event_selection` | User-configurable trigger source (event or timer, picked via UI) |
| `mqtt` | MQTT broker connectivity, publish/subscribe |

### API Quick Reference (v4.0)

```c
cJSON* app = ACAP_Init(APP_PACKAGE, settings_callback);
ACAP_HTTP_Node("endpoint", handler);

void handler(const ACAP_HTTP_Response response, const ACAP_HTTP_Request request) {
    const char* method = ACAP_HTTP_Get_Method(request);
    const char* body   = ACAP_HTTP_Get_Body(request);
    char* param        = ACAP_HTTP_Request_Param(request, "key");  // must free()
    free(param);
}

cJSON* settings = ACAP_Get_Config("settings");          // do NOT free
ACAP_STATUS_SetBool("system", "healthy", 1);
ACAP_EVENTS_Fire_State("state", value);
ACAP_EVENTS_Fire("trigger");
```

### Memory Ownership

| Function | Ownership |
|----------|-----------|
| `ACAP_Get_Config()`, `ACAP_STATUS_*()` getters | Internal — do NOT free |
| `ACAP_FILE_Read()` | Caller — `cJSON_Delete()` |
| `ACAP_HTTP_Request_Param()` | Caller — `free()` |
| `ACAP_VAPIX_Get/Post()`, `cJSON_Print*()` | Caller — `free()` |

### Adding an HTTP Endpoint
1. Add `{"name": "myendpoint", "access": "admin", "type": "fastCgi"}` to `manifest.json` httpConfig
2. Call `ACAP_HTTP_Node("myendpoint", my_handler)` in `main()`
3. Implement the handler using opaque accessor functions above

### Adding a New Source File
Add the `.c` file to `OBJS1` in `Makefile`. Add any new library to `PKGS`.

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

## AI Agent Support

This repo includes instruction files for all major AI coding assistants. Each file contains the same core rules (protected files, naming conventions, API quick reference, memory ownership) so any agent produces correct ACAP code out of the box.

| Agent | Config File | Auto-Loaded |
|-------|-------------|-------------|
| GitHub Copilot | `.github/copilot-instructions.md` | Yes |
| Cursor | `.cursor/rules/*.mdc` | Yes (skill-based) |
| Windsurf | `.windsurfrules` | Yes |
| Cline | `.clinerules` | Yes |
| Aider | `CONVENTIONS.md` | Yes |

**Cursor skills** provide context-aware rules that activate based on the files being edited:
- `acap-general.mdc` — Always on: core rules, template selection, build commands
- `add-http-endpoint.mdc` — Activates for manifest.json and main.c: step-by-step endpoint recipe
- `add-event.mdc` — Activates for events.json and subscriptions.json: event declaration and subscription
- `memory-management.mdc` — Activates for all .c files: ownership rules and common pitfalls

