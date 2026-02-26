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

## Known Build Pitfalls

| Symptom | Cause | Fix |
|---------|-------|-----|
| `'F_OK' undeclared` in storage/file code | `access()` / `F_OK` require `<unistd.h>` which is not pulled in transitively | Add `#include <unistd.h>` to any `.c` file that uses `access()`, `F_OK`, `R_OK`, `W_OK` |
| `-Wuse-after-free` build error on HTTP param | `ACAP_HTTP_Request_Param()` returns an allocated `char*`. Calling `free(param)` before the last use (including on early-return paths) causes this error | Always `free()` **after** the final use; audit all code paths |
| Timer trigger silently fails to capture (VDO deadlock) | `vdo_stream_snapshot()` is blocking and may need the GLib main loop internally. Calling it directly from a `g_timeout_add_seconds` callback (which runs on the main loop) deadlocks silently — no image is captured, no error is logged | Never call `vdo_stream_snapshot()` (or `Capture_Image()`) from a GLib timer callback. Use `g_idle_add(do_capture_idle, NULL)` inside the timer callback to defer the VDO call to the next main-loop iteration || `Settings_Updated` never fires on UI save | `ACAP_UpdateCallback` is called once **per property** with the property name as `service` (e.g. `"triggerType"`, `"timer"`). There is no final call with `service="settings"`. Checking `strcmp(service, "settings")` always fails | Match on the individual property names that affect your logic: `strcmp(service, "triggerType") == 0 \|\| strcmp(service, "timer") == 0` etc. |
| Object settings (e.g. `triggerEvent`) lost on restart | ACAP.c startup merge cannot restore a `cJSON_Object` whose default in `settings.json` is `null` — it tries to merge sub-keys into the null default, finds none, and the saved value is silently dropped | Add `Restore_Object_Settings()` after `ACAP_Init()`: read `localdata/settings.json` directly with `ACAP_FILE_Read` and call `cJSON_ReplaceItemInObject` for any property that is a `cJSON_Object` in the saved data but `null` in the live config |
## Full Reference

- `doc/ACAP.md` — Complete API reference, code patterns, all template main.c sources
- `doc/EVENTS.md` — Event catalog with namespaces and properties
- `doc/VDO.md` — Video capture API
- `doc/STORAGE.md` — Edge Storage API (SD card, NAS)
