# ACAP Development Rules

C template projects for Axis network camera applications (ACAP).
Built with ACAP Native SDK 12.2.0 inside Docker.

## Critical Rules

1. DO NOT modify ACAP.c, ACAP.h, cJSON.c, or cJSON.h — shared library files
2. Edit only: main.c, manifest.json, Makefile, settings/, html/
3. Names must match: PROG1 (Makefile) = APP_PACKAGE (main.c) = appName (manifest.json)
4. New .c files → add to OBJS1 in Makefile
5. New HTTP endpoints → declare in manifest.json httpConfig AND register via ACAP_HTTP_Node()
6. New library deps → add to PKGS in Makefile

## Template Selection

- base → Most projects (UI, image capture, events, settings)
- event_subscription → Static event monitoring (subscriptions.json)
- event_selection → Configurable trigger source (user picks event/timer via UI)
- mqtt → MQTT pub/sub services

## API Quick Reference (v4.0)

```c
// Initialization
cJSON* app = ACAP_Init(APP_PACKAGE, settings_callback);

// HTTP — opaque types, use accessors only
void handler(const ACAP_HTTP_Response response, const ACAP_HTTP_Request request) {
    const char* method = ACAP_HTTP_Get_Method(request);
    const char* body   = ACAP_HTTP_Get_Body(request);
    size_t len         = ACAP_HTTP_Get_Body_Length(request);
    char* param        = ACAP_HTTP_Request_Param(request, "key");  // must free()
    free(param);
}

// Settings (do NOT free the returned pointer)
cJSON* settings = ACAP_Get_Config("settings");

// Status
ACAP_STATUS_SetBool("system", "healthy", 1);
ACAP_STATUS_SetString("system", "status", "Running");

// Events
ACAP_EVENTS_Fire_State("state", value);
ACAP_EVENTS_Fire("trigger");
```

## Memory Ownership

- ACAP_Get_Config() → internal, do NOT free
- ACAP_STATUS_*() getters → internal, do NOT free
- ACAP_FILE_Read() → caller owns, MUST cJSON_Delete()
- ACAP_HTTP_Request_Param() → caller owns, MUST free()
- ACAP_VAPIX_Get/Post() → caller owns, MUST free()
- cJSON_Print*() → caller owns, MUST free()

## Build

```bash
cd template_dir && ./build.sh
```

## Known Build Pitfalls

| Symptom | Cause | Fix |
|---------|-------|-----|
| `'F_OK' undeclared` in storage/file code | `access()` / `F_OK` require `<unistd.h>` | Add `#include <unistd.h>` to any `.c` file using `access()`, `F_OK`, `R_OK`, `W_OK` |
| `-Wuse-after-free` on HTTP param | `ACAP_HTTP_Request_Param()` returns allocated `char*`; freeing before last use causes error | Always `free()` **after** the final use of the pointer; check all early-return paths |
| Timer trigger silently fails to capture (VDO deadlock) | `vdo_stream_snapshot()` is blocking; calling it from a `g_timeout_add_seconds` callback (main loop) deadlocks silently | Use `g_idle_add(do_capture_idle, NULL)` inside the timer callback to defer VDO to the next main-loop iteration |
| `Settings_Updated` never fires on UI save | `ACAP_UpdateCallback` is called once per property with the property name as `service` (e.g. `"triggerType"`). There is NO call with `service="settings"` | Match on individual property names: `strcmp(service, "triggerType") == 0 \|\| strcmp(service, "timer") == 0` etc. |
| Object settings (e.g. `triggerEvent`) lost on restart | ACAP.c startup merge cannot restore a `cJSON_Object` whose default is `null` — saved value silently dropped | Add `Restore_Object_Settings()` after `ACAP_Init()`: use `ACAP_FILE_Read("localdata/settings.json")` and `cJSON_ReplaceItemInObject` for any object property that is null in live config |
## Full Reference

- doc/ACAP.md — Complete API, code patterns, all template reference code
- doc/EVENTS.md — Event catalog with namespaces and properties
- doc/VDO.md — Video capture API
- doc/STORAGE.md — Edge Storage API (SD card, NAS)
