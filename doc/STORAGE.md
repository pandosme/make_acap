# Edge Storage API (axstorage)

Use the Edge Storage API to read and write files on SD cards and NAS (Network Attached Storage) shares from an ACAP application. Each ACAP gets its own sandboxed directory on the storage device — it cannot access files belonging to other applications.

- **API header:** `<axsdk/axstorage.h>`
- **pkg-config:** `axstorage`
- **Supported storage types:** SD cards (`SD_DISK`), network shares (`NetworkShare`)
- **Supported chips:** ARTPEC-6, ARTPEC-7, ARTPEC-8, ARTPEC-9, Ambarella CV25, i.MX 6SoloX
- **Official example:** [axstorage](https://github.com/AxisCommunications/acap-native-sdk-examples/tree/main/axstorage)
- **API reference:** [ax_storage.h](https://developer.axis.com/acap/api/src/api/axstorage/html/ax__storage_8h.html)

***

## Setup Checklist

### 1. manifest.json — Add the `storage` group

The ACAP must request membership in the `storage` Linux group. Add this to your manifest:

```json
{
    "schemaVersion": "1.7.1",
    "resources": {
        "linux": {
            "user": {
                "groups": [
                    "storage"
                ]
            }
        }
    },
    "acapPackageConf": {
        "setup": {
            "appName": "myapp",
            "friendlyName": "My App",
            "vendor": "Your Name",
            "embeddedSdkVersion": "3.0",
            "version": "1.0.0",
            "runMode": "once"
        }
    }
}
```

If your manifest already has a `resources` block (e.g. for D-Bus), merge the `linux.user.groups` array into it.

### 2. Makefile — Add axstorage to PKGS

```makefile
PKGS = glib-2.0 gio-2.0 axevent fcgi libcurl axstorage
```

### 3. main.c — Include the header

```c
#include <axsdk/axstorage.h>
```

If you use `access()` or `F_OK` for checking file existence (e.g. checking whether a thumbnail exists), also add:

```c
#include <unistd.h>   /* access(), F_OK, R_OK, W_OK */
```

> **Pitfall:** `access()` and `F_OK` are declared in `<unistd.h>`. Omitting this include causes a hard build error (`'F_OK' undeclared`) even though `#include <axsdk/axstorage.h>` is present. The cross-compiler flags `-Wall` escalate the implicit-function-declaration warning to an error.

***

## API Overview

The axstorage API is **entirely asynchronous** and callback-based, built on GLib. The lifecycle is:

```
ax_storage_list()          →  Get list of storage device names (e.g. "SD_DISK", "NetworkShare")
ax_storage_subscribe()     →  Subscribe to events for each device
  ↓ callback fires when status changes
ax_storage_get_status()    →  Check: available? writable? full? exiting?
ax_storage_setup_async()   →  Request the app-specific directory be created
  ↓ callback fires when setup completes
ax_storage_get_path()      →  Get the writable path (e.g. /var/spool/storage/areas/SD_DISK/myapp/)
  ↓ use standard C file I/O (fopen, fwrite, etc.)
ax_storage_release_async() →  Release the storage when done
```

### Key Types

```c
typedef struct _AXStorage AXStorage;  // Opaque storage handle

// Callback signatures
typedef void (*AXStorageSubscriptionCallback)(gchar *storage_id, gpointer user_data, GError *error);
typedef void (*AXStorageSetupCallback)(AXStorage *storage, gpointer user_data, GError *error);
typedef void (*AXStorageReleaseCallback)(gpointer user_data, GError *error);

// Storage status events
typedef enum {
    AX_STORAGE_AVAILABLE_EVENT,  // Storage became available/unavailable
    AX_STORAGE_EXITING_EVENT,    // Storage is about to disappear — stop using it
    AX_STORAGE_WRITABLE_EVENT,   // Storage became writable/readonly
    AX_STORAGE_FULL_EVENT        // Storage is full — stop writing, only delete allowed
} AXStorageStatusEventId;

// Storage type
typedef enum {
    LOCAL_TYPE,     // SD card
    EXTERNAL_TYPE,  // Network share (NAS)
    UNKNOWN_TYPE
} AXStorageType;
```

### Key Functions

| Function | Description |
|----------|-------------|
| `ax_storage_list(GError**)` | Returns `GList*` of storage device name strings. Caller must `g_free` each string and `g_list_free` the list. |
| `ax_storage_subscribe(id, callback, user_data, GError**)` | Subscribe to events for a storage device. Returns subscription ID (0 on error). |
| `ax_storage_unsubscribe(id, GError**)` | Stop subscribing. |
| `ax_storage_get_status(id, event, GError**)` | Get current status of a specific event (available, writable, full, exiting). |
| `ax_storage_setup_async(id, callback, user_data, GError**)` | Request the app directory. **Must call before any file I/O.** |
| `ax_storage_get_path(AXStorage*, GError**)` | Get the app-specific directory path. Caller must `g_free`. |
| `ax_storage_get_storage_id(AXStorage*, GError**)` | Get storage name from handle. Caller must `g_free`. |
| `ax_storage_get_type(AXStorage*, GError**)` | Get storage type (LOCAL_TYPE, EXTERNAL_TYPE). |
| `ax_storage_release_async(AXStorage*, callback, user_data, GError**)` | Release storage. **Must call when done or when exiting event fires.** |

### Memory Ownership

| Function | Returns | Ownership |
|----------|---------|-----------|
| `ax_storage_list()` | `GList*` of `gchar*` | Caller must `g_free` each string + `g_list_free` the list |
| `ax_storage_get_path()` | `gchar*` | Caller must `g_free` |
| `ax_storage_get_storage_id()` | `gchar*` | Caller must `g_free` |
| Callback `GError*` parameters | `GError*` | Callback must `g_error_free` |

***

## Complete Working Example

This example subscribes to SD card and NAS events, sets up storage when available, and provides a path for file I/O. It integrates with the ACAP wrapper pattern used in this repo's templates.

```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <glib.h>
#include <glib/gstdio.h>
#include <axsdk/axstorage.h>
#include "ACAP.h"
#include "cJSON.h"

#define APP_PACKAGE "myapp"
#define LOG(fmt, args...)      { syslog(LOG_INFO, fmt, ## args); printf(fmt, ## args); }
#define LOG_WARN(fmt, args...) { syslog(LOG_WARNING, fmt, ## args); printf(fmt, ## args); }

// ── Storage state per disk ──────────────────────────────────

typedef struct {
    AXStorage*    storage;
    gchar*        storage_id;
    gchar*        path;           // App-specific writable directory
    guint         subscription_id;
    gboolean      setup;
    gboolean      available;
    gboolean      writable;
    gboolean      full;
    gboolean      exiting;
} StorageDisk;

static GList* storage_disks = NULL;

static StorageDisk* find_disk(const gchar* storage_id) {
    for (GList* n = storage_disks; n; n = n->next) {
        StorageDisk* d = n->data;
        if (g_strcmp0(storage_id, d->storage_id) == 0)
            return d;
    }
    return NULL;
}

// ── Callbacks ───────────────────────────────────────────────

static void on_release(gpointer user_data, GError* error) {
    gchar* id = (gchar*)user_data;
    if (error) {
        LOG_WARN("Release %s failed: %s\n", id, error->message);
        g_error_free(error);
    } else {
        LOG("Released %s\n", id);
    }
}

static void on_setup(AXStorage* storage, gpointer user_data, GError* error) {
    (void)user_data;
    if (!storage || error) {
        LOG_WARN("Storage setup failed: %s\n", error ? error->message : "unknown");
        if (error) g_error_free(error);
        return;
    }

    GError* err = NULL;
    gchar* id   = ax_storage_get_storage_id(storage, &err);
    if (err) { LOG_WARN("get_storage_id: %s\n", err->message); g_error_free(err); return; }

    gchar* path = ax_storage_get_path(storage, &err);
    if (err) { LOG_WARN("get_path: %s\n", err->message); g_error_free(err); g_free(id); return; }

    StorageDisk* disk = find_disk(id);
    if (disk) {
        disk->storage = storage;
        disk->path    = g_strdup(path);
        disk->setup   = TRUE;
        LOG("Storage %s ready at %s\n", id, path);
        ACAP_STATUS_SetString("storage", disk->storage_id, path);
    }
    g_free(id);
    g_free(path);
}

static void on_subscribe(gchar* storage_id, gpointer user_data, GError* error) {
    (void)user_data;
    if (error) {
        LOG_WARN("Subscribe %s error: %s\n", storage_id, error->message);
        g_error_free(error);
        return;
    }

    StorageDisk* disk = find_disk(storage_id);
    if (!disk) return;

    GError* err = NULL;
    disk->available = ax_storage_get_status(storage_id, AX_STORAGE_AVAILABLE_EVENT, &err);
    if (err) { g_clear_error(&err); return; }
    disk->writable = ax_storage_get_status(storage_id, AX_STORAGE_WRITABLE_EVENT, &err);
    if (err) { g_clear_error(&err); return; }
    disk->full = ax_storage_get_status(storage_id, AX_STORAGE_FULL_EVENT, &err);
    if (err) { g_clear_error(&err); return; }
    disk->exiting = ax_storage_get_status(storage_id, AX_STORAGE_EXITING_EVENT, &err);
    if (err) { g_clear_error(&err); return; }

    LOG("Storage %s: available=%d writable=%d full=%d exiting=%d\n",
        storage_id, disk->available, disk->writable, disk->full, disk->exiting);

    // If storage is disappearing, release it
    if (disk->exiting && disk->setup) {
        ax_storage_release_async(disk->storage, on_release, disk->storage_id, &err);
        if (err) { LOG_WARN("release: %s\n", err->message); g_clear_error(&err); }
        else { disk->setup = FALSE; }
        return;
    }

    // If writable and not yet set up, request the app directory
    if (disk->writable && !disk->full && !disk->exiting && !disk->setup) {
        LOG("Setting up %s\n", storage_id);
        ax_storage_setup_async(storage_id, on_setup, NULL, &err);
        if (err) {
            LOG_WARN("setup_async: %s\n", err->message);
            g_clear_error(&err);
        }
    }
}

// ── Init / Cleanup ──────────────────────────────────────────

void Storage_Init(void) {
    GError* error = NULL;
    GList* disks = ax_storage_list(&error);
    if (error) {
        LOG_WARN("ax_storage_list: %s\n", error->message);
        g_error_free(error);
        return;
    }

    for (GList* n = disks; n; n = n->next) {
        gchar* id = (gchar*)n->data;
        GError* err = NULL;

        guint sub_id = ax_storage_subscribe(id, on_subscribe, NULL, &err);
        if (sub_id == 0 || err) {
            LOG_WARN("subscribe %s: %s\n", id, err ? err->message : "failed");
            g_clear_error(&err);
            g_free(id);
            continue;
        }

        StorageDisk* disk = g_new0(StorageDisk, 1);
        disk->storage_id      = g_strdup(id);
        disk->subscription_id = sub_id;
        storage_disks = g_list_append(storage_disks, disk);
        g_free(id);
    }
    g_list_free(disks);
    LOG("Storage initialized, monitoring %d device(s)\n", g_list_length(storage_disks));
}

void Storage_Cleanup(void) {
    for (GList* n = storage_disks; n; n = n->next) {
        StorageDisk* disk = n->data;
        GError* err = NULL;

        if (disk->setup) {
            ax_storage_release_async(disk->storage, on_release, disk->storage_id, &err);
            if (err) { LOG_WARN("release: %s\n", err->message); g_clear_error(&err); }
        }
        ax_storage_unsubscribe(disk->subscription_id, &err);
        if (err) { LOG_WARN("unsubscribe: %s\n", err->message); g_clear_error(&err); }

        g_free(disk->storage_id);
        g_free(disk->path);
    }
    g_list_free(storage_disks);
    storage_disks = NULL;
}

// ── Usage: get path for file I/O ────────────────────────────

// Returns the path string for a specific storage, or NULL if not ready.
// The returned string is owned by the StorageDisk — do NOT free.
const char* Storage_GetPath(const char* storage_id) {
    StorageDisk* disk = find_disk(storage_id);
    if (disk && disk->setup && disk->path)
        return disk->path;
    return NULL;
}
```

### Using it from main.c

```c
int main(void) {
    openlog(APP_PACKAGE, LOG_PID|LOG_CONS, LOG_USER);
    ACAP_Init(APP_PACKAGE, Settings_Updated_Callback);

    Storage_Init();

    // ... register HTTP endpoints, events, etc. ...

    main_loop = g_main_loop_new(NULL, FALSE);
    // ... signal handling ...
    g_main_loop_run(main_loop);

    Storage_Cleanup();
    ACAP_Cleanup();
    closelog();
    return 0;
}
```

### Writing a file to SD card

```c
const char* sd_path = Storage_GetPath("SD_DISK");
if (sd_path) {
    char filepath[256];
    snprintf(filepath, sizeof(filepath), "%s/data.json", sd_path);

    FILE* f = fopen(filepath, "w");
    if (f) {
        char* json = cJSON_Print(mydata);
        fputs(json, f);
        fclose(f);
        free(json);
        LOG("Wrote %s\n", filepath);
    }
} else {
    LOG_WARN("SD card not available\n");
}
```

### Reading a file from network share

```c
const char* nas_path = Storage_GetPath("NetworkShare");
if (nas_path) {
    char filepath[256];
    snprintf(filepath, sizeof(filepath), "%s/config.json", nas_path);

    FILE* f = fopen(filepath, "r");
    if (f) {
        fseek(f, 0, SEEK_END);
        long size = ftell(f);
        rewind(f);
        char* buf = malloc(size + 1);
        fread(buf, 1, size, f);
        buf[size] = '\0';
        fclose(f);

        cJSON* data = cJSON_Parse(buf);
        free(buf);
        // ... use data ...
        cJSON_Delete(data);
    }
}
```

***

## HTTP Endpoint: Save Data to SD Card

A complete endpoint that receives JSON via POST and saves it to the SD card:

```c
void HTTP_Endpoint_save(const ACAP_HTTP_Response response, const ACAP_HTTP_Request request) {
    const char* method = ACAP_HTTP_Get_Method(request);
    if (!method || strcmp(method, "POST") != 0) {
        ACAP_HTTP_Respond_Error(response, 405, "Method not allowed");
        return;
    }

    const char* body = ACAP_HTTP_Get_Body(request);
    if (!body || ACAP_HTTP_Get_Body_Length(request) == 0) {
        ACAP_HTTP_Respond_Error(response, 400, "Missing body");
        return;
    }

    const char* sd_path = Storage_GetPath("SD_DISK");
    if (!sd_path) {
        ACAP_HTTP_Respond_Error(response, 503, "SD card not available");
        return;
    }

    char filepath[256];
    snprintf(filepath, sizeof(filepath), "%s/upload.json", sd_path);

    FILE* f = fopen(filepath, "w");
    if (!f) {
        ACAP_HTTP_Respond_Error(response, 500, "Failed to write file");
        return;
    }
    fwrite(body, 1, ACAP_HTTP_Get_Body_Length(request), f);
    fclose(f);

    ACAP_HTTP_Respond_Text(response, "Saved");
}
```

Register in main:
```c
ACAP_HTTP_Node("save", HTTP_Endpoint_save);
```

And add to manifest.json httpConfig:
```json
{"name": "save", "access": "admin", "type": "fastCgi"}
```

***

## Important Considerations

### Storage Availability

- **SD cards** can be removed at any time. Always check `Storage_GetPath()` before file operations.
- **Network shares** depend on network connectivity. Treat them as unreliable.
- The `AX_STORAGE_EXITING_EVENT` fires when storage is about to disappear — stop all I/O and release immediately.
- The `AX_STORAGE_FULL_EVENT` means stop writing; you may still delete files.

### File Paths

- Your app's directory is: `/var/spool/storage/areas/<STORAGE_ID>/<appName>/`
- You can create subdirectories within your app directory.
- You **cannot** access files outside your app directory or other apps' directories.

### Performance

- SD cards and NAS have much higher latency than local flash. Avoid blocking the GLib main loop with large file operations.
- For large writes, consider using a GLib idle source or a worker thread.

### Persistence

- Files on SD card survive app restarts and firmware upgrades.
- Files on NAS survive as long as the share is mounted and accessible.
- The app directory itself is only created after `ax_storage_setup_async` completes.

***

## External Resources

- [axstorage API reference](https://developer.axis.com/acap/api/src/api/axstorage/html/ax__storage_8h.html)
- [Official axstorage example](https://github.com/AxisCommunications/acap-native-sdk-examples/tree/main/axstorage)
- [Edge Storage API overview](https://developer.axis.com/acap/api/native-sdk-api#edge-storage-api)
