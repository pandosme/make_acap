# ACAP Development Conventions

C template projects for Axis network camera applications (ACAP).
Built with ACAP Native SDK 12.2.0 inside Docker.

## Critical Rules

1. DO NOT modify `ACAP.c`, `ACAP.h`, `cJSON.c`, or `cJSON.h` — they are shared library files
2. Edit only: `main.c`, `manifest.json`, `Makefile`, `settings/`, `html/`
3. Names must match: `PROG1` (Makefile) = `APP_PACKAGE` (main.c) = `appName` (manifest.json)
4. New `.c` files → add to `OBJS1` in Makefile
5. New HTTP endpoints → declare in `manifest.json` httpConfig AND register via `ACAP_HTTP_Node()`
6. New library deps → add to `PKGS` in Makefile

## Template Selection

| Template | Use When |
|----------|----------|
| `base` | Most projects — UI, image capture, events, settings |
| `event_subscription` | Static event monitoring (subscriptions.json) |
| `event_selection` | Configurable trigger source (user picks event/timer via UI) |
| `mqtt` | MQTT pub/sub services |

## API Quick Reference (v4.0)

```c
// Initialization (returns cJSON* app config)
cJSON* app = ACAP_Init(APP_PACKAGE, settings_callback);

// HTTP endpoints — register named nodes
ACAP_HTTP_Node("myendpoint", my_handler);

// HTTP handler signature — types are opaque pointers
void my_handler(const ACAP_HTTP_Response response, const ACAP_HTTP_Request request) {
    const char* method = ACAP_HTTP_Get_Method(request);
    const char* body   = ACAP_HTTP_Get_Body(request);
    size_t len         = ACAP_HTTP_Get_Body_Length(request);
    char* param        = ACAP_HTTP_Request_Param(request, "key");  // caller must free()
    // ... respond ...
    free(param);
}

// Settings (internally managed — do NOT cJSON_Delete)
cJSON* settings = ACAP_Get_Config("settings");

// Status
ACAP_STATUS_SetBool("system", "healthy", 1);
ACAP_STATUS_SetString("system", "status", "Running");

// Events
ACAP_EVENTS_Fire_State("state", value);
ACAP_EVENTS_Fire("trigger");
ACAP_EVENTS_Subscribe(subscription_json, NULL);
```

## Memory Ownership

| Function | Returns | Ownership |
|----------|---------|-----------|
| `ACAP_Get_Config()` | `cJSON*` | Internal — do NOT free |
| `ACAP_STATUS_*()` getters | `cJSON*` | Internal — do NOT free |
| `ACAP_FILE_Read()` | `cJSON*` | Caller owns — MUST `cJSON_Delete()` |
| `ACAP_HTTP_Request_Param()` | `char*` | Caller owns — MUST `free()` |
| `ACAP_VAPIX_Get/Post()` | `char*` | Caller owns — MUST `free()` |
| `cJSON_Print*()` | `char*` | Caller owns — MUST `free()` |

## Build

```bash
cd template_dir && ./build.sh
```

## Full Reference

- `doc/ACAP.md` — Complete API reference, code patterns, all template main.c sources
- `doc/EVENTS.md` — Event catalog with namespaces and properties
- `doc/VDO.md` — Video capture API
- `doc/STORAGE.md` — Edge Storage API (SD card, NAS)
