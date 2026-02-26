---
description: Core ACAP development rules that apply to all files
globs:
alwaysApply: true
---

# ACAP Development Rules

## Protected Files — DO NOT EDIT
- `ACAP.c`, `ACAP.h`, `cJSON.c`, `cJSON.h` are shared library files
- These are identical across all templates and must not be modified

## Editable Files
- `main.c` — application logic
- `manifest.json` — package identity, HTTP endpoints, permissions
- `Makefile` — build sources and library dependencies
- `settings/*.json` — configuration files
- `html/` — web UI assets

## Naming Requirement
These three values MUST always match:
- `PROG1` in Makefile
- `APP_PACKAGE` in main.c
- `appName` in manifest.json

## Template Selection
- `base` — Most projects (UI, image capture, events, settings)
- `event_subscription` — Static event monitoring via subscriptions.json
- `event_selection` — Configurable trigger source (user picks event/timer via UI)
- `mqtt` — MQTT pub/sub services

## API Version
This project uses ACAP API v4.0:
- Initialize with `ACAP_Init(APP_PACKAGE, callback)` (not `ACAP()`)
- HTTP types are opaque — use `ACAP_HTTP_Get_Method()`, `ACAP_HTTP_Get_Body()`, etc.
- `ACAP_HTTP_Request_Param()` returns `char*` that caller must `free()`

## Build
```bash
cd template_dir && ./build.sh
```

## Known Build Pitfalls

| Symptom | Cause | Fix |
|---------|-------|-----|
| `'F_OK' undeclared` | `access()` / `F_OK` require `<unistd.h>` which is not pulled in transitively | Add `#include <unistd.h>` to any `.c` using `access()` |
| `-Wuse-after-free` error | `free(param)` called before last use of the pointer | Always `free()` **after** the final use, on every code path |
| Timer triggers silently fail (VDO deadlock) | `vdo_stream_snapshot()` is blocking; calling it from a `g_timeout_add_seconds` callback (main loop) deadlocks silently — no image, no error | Use `g_idle_add(do_capture_idle, NULL)` inside the timer callback to defer VDO to the next main-loop iteration |
| `Settings_Updated` never fires on UI save | `ACAP_UpdateCallback` passes the **property name** as `service` (e.g. `"triggerType"`), never `"settings"`. The common pattern `strcmp(service, "settings")` always fails | Match on individual property names that affect your logic |
| Object settings (e.g. `triggerEvent`) lost on restart | ACAP.c startup merge cannot restore a `cJSON_Object` whose default is `null` — saved value silently dropped | After `ACAP_Init()`, call `Restore_Object_Settings()`: read `localdata/settings.json` with `ACAP_FILE_Read` and `cJSON_ReplaceItemInObject` for any object property that is null in the live config |

## Reference Documentation
- `doc/ACAP.md` — Complete API reference with all code patterns
- `doc/EVENTS.md` — Event catalog (namespaces, properties, state types)
- `doc/VDO.md` — Video capture API
- `doc/STORAGE.md` — Edge Storage API (SD card, NAS)
