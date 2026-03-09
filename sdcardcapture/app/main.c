#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <syslog.h>
#include <stdint.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <time.h>
#include <glib.h>
#include <glib-unix.h>
#include <glib/gstdio.h>
#include <signal.h>
#include <axsdk/axstorage.h>
#include <vdo-stream.h>
#include <vdo-frame.h>
#include <vdo-types.h>
#include "ACAP.h"
#include "cJSON.h"
#include "imgutil.h"

#define APP_PACKAGE "sdcardcapture"

#define LOG(fmt, args...)      { syslog(LOG_INFO,    fmt, ## args); printf(fmt, ## args); }
#define LOG_WARN(fmt, args...) { syslog(LOG_WARNING, fmt, ## args); printf(fmt, ## args); }
#define LOG_TRACE(fmt, args...) {}

/* ═══════════════════════════════════════════════════════════════════════════
   Storage
   ═══════════════════════════════════════════════════════════════════════════ */

typedef struct {
    AXStorage* storage;
    gchar*     storage_id;
    gchar*     path;
    guint      subscription_id;
    gboolean   setup;
    gboolean   available;
    gboolean   writable;
    gboolean   full;
    gboolean   exiting;
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
        if (disk->path) g_free(disk->path);
        disk->path  = g_strdup(path);
        disk->setup = TRUE;

        /* Create images/ and thumbs/ subdirectories */
        char imgdir[768], tmbdir[768];
        snprintf(imgdir, sizeof(imgdir), "%s/images", path);
        snprintf(tmbdir, sizeof(tmbdir), "%s/thumbs", path);
        g_mkdir_with_parents(imgdir, 0755);
        g_mkdir_with_parents(tmbdir, 0755);

        LOG("Storage %s ready at %s\n", id, path);
        ACAP_STATUS_SetString("storage", disk->storage_id, path);
        ACAP_STATUS_SetString("storage", "status", "Ready");
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

    ACAP_STATUS_SetString("storage", "status",
        disk->available ? (disk->writable ? "Ready" : "Read-only") : "Not available");

    if (disk->exiting && disk->setup) {
        ax_storage_release_async(disk->storage, on_release, disk->storage_id, &err);
        if (err) { LOG_WARN("release: %s\n", err->message); g_clear_error(&err); }
        else { disk->setup = FALSE; disk->path = NULL; }
        return;
    }

    if (disk->writable && !disk->full && !disk->exiting && !disk->setup) {
        LOG("Setting up %s\n", storage_id);
        ax_storage_setup_async(storage_id, on_setup, NULL, &err);
        if (err) { LOG_WARN("setup_async: %s\n", err->message); g_clear_error(&err); }
    }
}

static void Storage_Init(void) {
    GError* error = NULL;
    GList* disks = ax_storage_list(&error);
    if (error) {
        LOG_WARN("ax_storage_list: %s\n", error->message);
        g_error_free(error);
        ACAP_STATUS_SetString("storage", "status", "No storage devices");
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
        StorageDisk* disk  = g_new0(StorageDisk, 1);
        disk->storage_id   = g_strdup(id);
        disk->subscription_id = sub_id;
        storage_disks = g_list_append(storage_disks, disk);
        g_free(id);
    }
    g_list_free(disks);
    LOG("Storage initialized, monitoring %d device(s)\n", g_list_length(storage_disks));
}

static void Storage_Cleanup(void) {
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

/* Returns the path for a storage device, or NULL if not ready. Do NOT free. */
static const char* Storage_GetPath(const char* storage_id) {
    StorageDisk* disk = find_disk(storage_id);
    if (disk && disk->setup && disk->path)
        return disk->path;
    return NULL;
}

/* ═══════════════════════════════════════════════════════════════════════════
   Globals
   ═══════════════════════════════════════════════════════════════════════════ */

static GMainLoop* main_loop = NULL;
static int currentSubscriptionId = 0;
static guint timerId = 0;

/* ═══════════════════════════════════════════════════════════════════════════
   Retention
   ═══════════════════════════════════════════════════════════════════════════ */

/* Compare strings for qsort (alphabetical = chronological with timestamp names) */
static int str_cmp(const void* a, const void* b) {
    return strcmp(*(const char**)a, *(const char**)b);
}

/*
 * Delete a pair of image + thumbnail files.
 * images_dir and thumbs_dir must be provided.
 */
static void delete_image_pair(const char* images_dir, const char* thumbs_dir, const char* filename) {
    char path[1024];
    snprintf(path, sizeof(path), "%s/%s", images_dir, filename);
    remove(path);

    /* Build thumbnail name: replace .jpg with _thumb.jpg if needed, or look for matching name */
    snprintf(path, sizeof(path), "%s/%s", thumbs_dir, filename);
    remove(path);
}

static void Enforce_Retention(void) {
    const char* sd_path = Storage_GetPath("SD_DISK");
    if (!sd_path) return;

    char images_dir[768], thumbs_dir[768];
    snprintf(images_dir, sizeof(images_dir), "%s/images", sd_path);
    snprintf(thumbs_dir, sizeof(thumbs_dir), "%s/thumbs", sd_path);

    cJSON* settings = ACAP_Get_Config("settings");
    if (!settings) return;

    cJSON* maxImagesItem = cJSON_GetObjectItem(settings, "maxImages");
    int maxImages = maxImagesItem ? maxImagesItem->valueint : 1000;

    cJSON* retentionWeeksItem = cJSON_GetObjectItem(settings, "retentionWeeks");
    int retentionWeeks = retentionWeeksItem ? retentionWeeksItem->valueint : 4;

    cJSON* minFreeSpaceItem = cJSON_GetObjectItem(settings, "minFreeSpacePercent");
    int minFreeSpacePercent = minFreeSpaceItem ? minFreeSpaceItem->valueint : 10;

    /* Scan images directory and collect sorted filenames */
    DIR* dir = opendir(images_dir);
    if (!dir) return;

    char** names = NULL;
    int count = 0, capacity = 0;

    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_name[0] == '.') continue;
        size_t len = strlen(entry->d_name);
        if (len < 4 || strcmp(entry->d_name + len - 4, ".jpg") != 0) continue;

        if (count >= capacity) {
            capacity = capacity ? capacity * 2 : 64;
            names = realloc(names, capacity * sizeof(char*));
        }
        names[count++] = strdup(entry->d_name);
    }
    closedir(dir);

    if (count == 0) { free(names); return; }

    qsort(names, count, sizeof(char*), str_cmp);  /* oldest first */

    /* Policy 1: max image count */
    int idx = 0;
    while (count - idx > maxImages) {
        LOG("Retention (max count): deleting %s\n", names[idx]);
        delete_image_pair(images_dir, thumbs_dir, names[idx]);
        idx++;
    }

    /* Policy 2: retention period */
    time_t cutoff = time(NULL) - (time_t)retentionWeeks * 7 * 24 * 3600;
    while (idx < count) {
        /* Parse timestamp from filename: YYYYMMDDTHHmmss.jpg */
        const char* n = names[idx];
        if (strlen(n) < 15) break;
        struct tm tm_file;
        memset(&tm_file, 0, sizeof(tm_file));
        if (sscanf(n, "%4d%2d%2dT%2d%2d%2d",
                   &tm_file.tm_year, &tm_file.tm_mon, &tm_file.tm_mday,
                   &tm_file.tm_hour, &tm_file.tm_min, &tm_file.tm_sec) == 6) {
            tm_file.tm_year -= 1900;
            tm_file.tm_mon  -= 1;
            tm_file.tm_isdst = -1;
            time_t file_time = mktime(&tm_file);
            if (file_time < cutoff) {
                LOG("Retention (age): deleting %s\n", names[idx]);
                delete_image_pair(images_dir, thumbs_dir, names[idx]);
                idx++;
                continue;
            }
        }
        break;
    }

    /* Policy 3: minimum free space */
    struct statvfs vfs;
    if (statvfs(images_dir, &vfs) == 0) {
        while (idx < count) {
            unsigned long long total = (unsigned long long)vfs.f_blocks * vfs.f_frsize;
            unsigned long long free_space = (unsigned long long)vfs.f_bavail * vfs.f_frsize;
            int free_pct = (total > 0) ? (int)((free_space * 100) / total) : 100;

            if (free_pct < minFreeSpacePercent) {
                LOG("Retention (free space %d%%<%d%%): deleting %s\n",
                    free_pct, minFreeSpacePercent, names[idx]);
                delete_image_pair(images_dir, thumbs_dir, names[idx]);
                idx++;
                /* Re-check after deletion */
                statvfs(images_dir, &vfs);
            } else {
                break;
            }
        }
    }

    /* Update image count status */
    int remaining = count - idx;
    ACAP_STATUS_SetNumber("capture", "imageCount", remaining);

    for (int i = 0; i < count; i++) free(names[i]);
    free(names);
}

/* ═══════════════════════════════════════════════════════════════════════════
   Image Capture
   ═══════════════════════════════════════════════════════════════════════════ */

static void Capture_Image(void) {
    const char* sd_path = Storage_GetPath("SD_DISK");
    if (!sd_path) {
        LOG_WARN("Capture skipped: SD card not available\n");
        return;
    }

    cJSON* settings = ACAP_Get_Config("settings");
    if (!settings) return;

    /* Parse resolution */
    int width = 1920, height = 1080;
    cJSON* resItem = cJSON_GetObjectItem(settings, "resolution");
    if (resItem && resItem->valuestring)
        sscanf(resItem->valuestring, "%dx%d", &width, &height);

    /* Generate timestamp filename */
    time_t now = time(NULL);
    struct tm* tm_now = localtime(&now);
    char timestamp[32];
    strftime(timestamp, sizeof(timestamp), "%Y%m%dT%H%M%S", tm_now);

    /* Ensure subdirectories exist (defensive — in case on_setup mkdir failed) */
    char images_dir[768], thumbs_dir[768];
    snprintf(images_dir, sizeof(images_dir), "%s/images", sd_path);
    snprintf(thumbs_dir, sizeof(thumbs_dir), "%s/thumbs", sd_path);
    if (g_mkdir_with_parents(images_dir, 0755) != 0)
        LOG_WARN("Failed to create %s: %s\n", images_dir, g_strerror(errno));
    if (g_mkdir_with_parents(thumbs_dir, 0755) != 0)
        LOG_WARN("Failed to create %s: %s\n", thumbs_dir, g_strerror(errno));

    char img_path[1024], thumb_path[1024];
    snprintf(img_path,   sizeof(img_path),   "%s/%s.jpg", images_dir, timestamp);
    snprintf(thumb_path, sizeof(thumb_path), "%s/%s.jpg", thumbs_dir, timestamp);

    /* Capture full-resolution image */
    GError* error = NULL;
    VdoMap* vdoSettings = vdo_map_new();
    vdo_map_set_uint32(vdoSettings, "format", VDO_FORMAT_JPEG);
    vdo_map_set_uint32(vdoSettings, "width",  (uint32_t)width);
    vdo_map_set_uint32(vdoSettings, "height", (uint32_t)height);

    VdoBuffer* buffer = vdo_stream_snapshot(vdoSettings, &error);
    g_clear_object(&vdoSettings);

    if (error || !buffer) {
        LOG_WARN("Full-res snapshot failed: %s\n", error ? error->message : "unknown");
        if (error) g_error_free(error);
        return;
    }

    unsigned char* data = vdo_buffer_get_data(buffer);
    unsigned int   size = vdo_frame_get_size(buffer);
    imgutil_save_jpeg(img_path, data, size);
    g_object_unref(buffer);

    /* Capture thumbnail */
    error = NULL;
    VdoMap* thumbSettings = vdo_map_new();
    vdo_map_set_uint32(thumbSettings, "format", VDO_FORMAT_JPEG);
    vdo_map_set_uint32(thumbSettings, "width",  320);
    vdo_map_set_uint32(thumbSettings, "height", 180);

    VdoBuffer* tbuffer = vdo_stream_snapshot(thumbSettings, &error);
    g_clear_object(&thumbSettings);

    if (error || !tbuffer) {
        LOG_WARN("Thumbnail snapshot failed: %s\n", error ? error->message : "unknown");
        if (error) g_error_free(error);
    } else {
        unsigned char* tdata = vdo_buffer_get_data(tbuffer);
        unsigned int   tsize = vdo_frame_get_size(tbuffer);
        imgutil_save_jpeg(thumb_path, tdata, tsize);
        g_object_unref(tbuffer);
    }

    LOG("Captured: %s (%dx%d)\n", timestamp, width, height);
    ACAP_STATUS_SetString("capture", "lastCapture", ACAP_DEVICE_ISOTime());

    /* Enforce retention after each capture */
    Enforce_Retention();
}

static gboolean do_capture_idle(gpointer user_data) {
    (void)user_data;
    Capture_Image();
    return G_SOURCE_REMOVE;
}

/* ═══════════════════════════════════════════════════════════════════════════
   Event / Timer
   ═══════════════════════════════════════════════════════════════════════════ */

static int Extract_Event_State(cJSON* event) {
    const char* stateProperties[] = {
        "active", "state", "triggered", "LogicalState",
        "ready", "Open", "lost", "Detected", "recording",
        "accessed", "connected", "day", "sensor_level",
        "disruption", "alert", "fan_failure", "Failed",
        "limit_exceeded", "abr_error", "tampering",
        "signal_status_ok", "Playing", NULL
    };
    for (int i = 0; stateProperties[i]; i++) {
        cJSON* item = cJSON_GetObjectItem(event, stateProperties[i]);
        if (item) {
            if (cJSON_IsBool(item))   return cJSON_IsTrue(item) ? 1 : 0;
            if (cJSON_IsNumber(item)) return item->valueint ? 1 : 0;
            if (cJSON_IsString(item)) {
                if (strcmp(item->valuestring, "1") == 0 ||
                    strcmp(item->valuestring, "true") == 0) return 1;
                return 0;
            }
        }
    }
    return 1;
}

void My_Event_Callback(cJSON* event, void* userdata) {
    (void)userdata;
    int state = Extract_Event_State(event);
    ACAP_EVENTS_Fire_State("state", state);
    ACAP_EVENTS_Fire("trigger");
    ACAP_STATUS_SetBool("trigger", "active", state);
    ACAP_STATUS_SetString("trigger", "lastTriggered", ACAP_DEVICE_ISOTime());

    if (state) {
        g_idle_add(do_capture_idle, NULL);
    }
}

static gboolean Timer_Callback(gpointer user_data) {
    (void)user_data;
    LOG_TRACE("Timer triggered\n");
    ACAP_EVENTS_Fire("trigger");
    ACAP_EVENTS_Fire_State("state", 1);
    ACAP_STATUS_SetString("trigger", "lastTriggered", ACAP_DEVICE_ISOTime());
    g_idle_add(do_capture_idle, NULL);
    g_usleep(100000);
    ACAP_EVENTS_Fire_State("state", 0);
    return G_SOURCE_CONTINUE;
}

static void Apply_Trigger(void) {
    if (currentSubscriptionId > 0) {
        ACAP_EVENTS_Unsubscribe(currentSubscriptionId);
        currentSubscriptionId = 0;
    }
    if (timerId > 0) {
        g_source_remove(timerId);
        timerId = 0;
    }

    ACAP_EVENTS_Fire_State("state", 0);
    ACAP_STATUS_SetBool("trigger", "active", 0);

    cJSON* settings = ACAP_Get_Config("settings");
    if (!settings) {
        ACAP_STATUS_SetString("trigger", "type",   "none");
        ACAP_STATUS_SetString("trigger", "status", "No configuration");
        return;
    }

    cJSON* triggerTypeItem = cJSON_GetObjectItem(settings, "triggerType");
    const char* triggerType = (triggerTypeItem && triggerTypeItem->valuestring)
                              ? triggerTypeItem->valuestring : "none";

    if (strcmp(triggerType, "event") == 0) {
        cJSON* ev = cJSON_GetObjectItem(settings, "triggerEvent");
        if (ev && !cJSON_IsNull(ev)) {
            cJSON* nameItem = cJSON_GetObjectItem(ev, "name");
            const char* eventName = nameItem ? nameItem->valuestring : "Unknown";
            currentSubscriptionId = ACAP_EVENTS_Subscribe(ev, NULL);
            if (currentSubscriptionId > 0) {
                LOG("Subscribed to event: %s (id=%d)\n", eventName, currentSubscriptionId);
                ACAP_STATUS_SetString("trigger", "type",   "event");
                ACAP_STATUS_SetString("trigger", "event",  eventName);
                ACAP_STATUS_SetString("trigger", "status", "Monitoring event");
            } else {
                LOG_WARN("Failed to subscribe to event: %s\n", eventName);
                ACAP_STATUS_SetString("trigger", "type",   "event");
                ACAP_STATUS_SetString("trigger", "event",  eventName);
                ACAP_STATUS_SetString("trigger", "status", "Subscription failed");
            }
        } else {
            ACAP_STATUS_SetString("trigger", "type",   "event");
            ACAP_STATUS_SetString("trigger", "status", "No event selected");
        }
    } else if (strcmp(triggerType, "timer") == 0) {
        cJSON* timerItem = cJSON_GetObjectItem(settings, "timer");
        int interval = timerItem ? timerItem->valueint : 60;
        if (interval < 1) interval = 1;
        timerId = g_timeout_add_seconds(interval, Timer_Callback, NULL);
        LOG("Timer started: every %d seconds\n", interval);
        ACAP_STATUS_SetString("trigger", "type",   "timer");
        ACAP_STATUS_SetNumber("trigger", "interval", interval);
        ACAP_STATUS_SetString("trigger", "status", "Timer running");
    } else {
        ACAP_STATUS_SetString("trigger", "type",   "none");
        ACAP_STATUS_SetString("trigger", "status", "Disabled");
    }
}

/* ═══════════════════════════════════════════════════════════════════════════
   Settings
   ═══════════════════════════════════════════════════════════════════════════ */

/*
 * Restore object-type settings (like triggerEvent) that are lost on restart
 * because ACAP_Init resets cJSON_Object settings to null from defaults.
 */
static void Restore_Object_Settings(void) {
    cJSON* saved = ACAP_FILE_Read("localdata/settings.json");
    if (!saved) return;

    cJSON* live = ACAP_Get_Config("settings");
    if (!live) { cJSON_Delete(saved); return; }

    cJSON* savedEvent = cJSON_GetObjectItem(saved, "triggerEvent");
    cJSON* liveEvent  = cJSON_GetObjectItem(live,  "triggerEvent");

    if (savedEvent && !cJSON_IsNull(savedEvent) &&
        liveEvent  &&  cJSON_IsNull(liveEvent)) {
        cJSON* copy = cJSON_Duplicate(savedEvent, 1);
        cJSON_ReplaceItemInObject(live, "triggerEvent", copy);
        LOG("Restored triggerEvent from saved settings\n");
    }

    cJSON_Delete(saved);
}

void Settings_Updated_Callback(const char* service, cJSON* data) {
    (void)data;
    LOG_TRACE("Settings updated: %s\n", service);

    if (strcmp(service, "triggerType")   == 0 ||
        strcmp(service, "triggerEvent")  == 0 ||
        strcmp(service, "timer")         == 0) {
        Apply_Trigger();
    }

    if (strcmp(service, "maxImages")          == 0 ||
        strcmp(service, "retentionWeeks")     == 0 ||
        strcmp(service, "minFreeSpacePercent") == 0) {
        Enforce_Retention();
    }
}

/* ═══════════════════════════════════════════════════════════════════════════
   HTTP: trigger
   ═══════════════════════════════════════════════════════════════════════════ */

void HTTP_Endpoint_trigger(const ACAP_HTTP_Response response, const ACAP_HTTP_Request request) {
    const char* method = ACAP_HTTP_Get_Method(request);
    if (!method) { ACAP_HTTP_Respond_Error(response, 400, "Invalid request"); return; }

    if (strcmp(method, "GET") == 0) {
        cJSON* status = ACAP_STATUS_Group("trigger");
        if (status) ACAP_HTTP_Respond_JSON(response, status);
        else        ACAP_HTTP_Respond_Text(response, "{}");
        return;
    }

    if (strcmp(method, "POST") == 0) {
        LOG("Manual trigger fired\n");
        ACAP_EVENTS_Fire("trigger");
        ACAP_EVENTS_Fire_State("state", 1);
        ACAP_STATUS_SetString("trigger", "lastTriggered", ACAP_DEVICE_ISOTime());
        Capture_Image();
        g_usleep(100000);
        ACAP_EVENTS_Fire_State("state", 0);
        ACAP_HTTP_Respond_Text(response, "OK");
        return;
    }

    ACAP_HTTP_Respond_Error(response, 405, "Method not allowed");
}

/* ═══════════════════════════════════════════════════════════════════════════
   HTTP: capture (resolutions + manual capture)
   ═══════════════════════════════════════════════════════════════════════════ */

void HTTP_Endpoint_capture(const ACAP_HTTP_Response response, const ACAP_HTTP_Request request) {
    const char* method = ACAP_HTTP_Get_Method(request);
    if (!method) { ACAP_HTTP_Respond_Error(response, 400, "Invalid request"); return; }

    if (strcmp(method, "GET") == 0) {
        /* ?status — return capture status */
        cJSON* status = ACAP_STATUS_Group("capture");
        if (status) ACAP_HTTP_Respond_JSON(response, status);
        else        ACAP_HTTP_Respond_Text(response, "{}");
        return;
    }

    if (strcmp(method, "POST") == 0) {
        /* Manual capture */
        const char* sd_path = Storage_GetPath("SD_DISK");
        if (!sd_path) {
            ACAP_HTTP_Respond_Error(response, 503, "SD card not available");
            return;
        }
        Capture_Image();
        ACAP_HTTP_Respond_Text(response, "OK");
        return;
    }

    ACAP_HTTP_Respond_Error(response, 405, "Method not allowed");
}

/* ═══════════════════════════════════════════════════════════════════════════
   HTTP: images (list, serve, delete)
   ═══════════════════════════════════════════════════════════════════════════ */

void HTTP_Endpoint_images(const ACAP_HTTP_Response response, const ACAP_HTTP_Request request) {
    const char* method = ACAP_HTTP_Get_Method(request);
    if (!method) { ACAP_HTTP_Respond_Error(response, 400, "Invalid request"); return; }

    const char* sd_path = Storage_GetPath("SD_DISK");
    if (!sd_path) {
        ACAP_HTTP_Respond_Error(response, 503, "SD card not available");
        return;
    }

    char images_dir[768], thumbs_dir[768];
    snprintf(images_dir, sizeof(images_dir), "%s/images", sd_path);
    snprintf(thumbs_dir, sizeof(thumbs_dir), "%s/thumbs", sd_path);

    if (strcmp(method, "GET") == 0) {
        /* ?list — return JSON array of image metadata */
        char* listParam = ACAP_HTTP_Request_Param(request, "list");
        if (listParam) {
            free(listParam);
            DIR* dir = opendir(images_dir);
            if (!dir) {
                ACAP_HTTP_Respond_JSON(response, cJSON_CreateArray());
                return;
            }
            char** names = NULL;
            int count = 0, capacity = 0;
            struct dirent* entry;
            while ((entry = readdir(dir)) != NULL) {
                if (entry->d_name[0] == '.') continue;
                size_t len = strlen(entry->d_name);
                if (len < 4 || strcmp(entry->d_name + len - 4, ".jpg") != 0) continue;
                if (count >= capacity) {
                    capacity = capacity ? capacity * 2 : 64;
                    names = realloc(names, capacity * sizeof(char*));
                }
                names[count++] = strdup(entry->d_name);
            }
            closedir(dir);

            qsort(names, count, sizeof(char*), str_cmp);

            cJSON* arr = cJSON_CreateArray();
            for (int i = count - 1; i >= 0; i--) {  /* newest first */
                char filepath[1024];
                snprintf(filepath, sizeof(filepath), "%s/%s", images_dir, names[i]);
                struct stat st;
                long size = (stat(filepath, &st) == 0) ? (long)st.st_size : 0;

                cJSON* obj = cJSON_CreateObject();
                cJSON_AddStringToObject(obj, "filename", names[i]);

                /* Parse timestamp from filename for display */
                char ts[32] = "";
                const char* n = names[i];
                if (strlen(n) >= 15) {
                    snprintf(ts, sizeof(ts),
                             "%.4s-%.2s-%.2s %.2s:%.2s:%.2s",
                             n, n+4, n+6, n+9, n+11, n+13);
                }
                cJSON_AddStringToObject(obj, "timestamp", ts);
                cJSON_AddNumberToObject(obj, "size", size);

                /* Check if thumbnail exists */
                char tpath[1024];
                snprintf(tpath, sizeof(tpath), "%s/%s", thumbs_dir, names[i]);
                cJSON_AddBoolToObject(obj, "hasThumb", access(tpath, F_OK) == 0);

                cJSON_AddItemToArray(arr, obj);
                free(names[i]);
            }
            free(names);
            ACAP_HTTP_Respond_JSON(response, arr);
            cJSON_Delete(arr);
            return;
        }

        /* ?file=NAME — serve a full image */
        char* filename = ACAP_HTTP_Request_Param(request, "file");
        if (filename) {
            /* Sanitize: no path traversal */
            if (strchr(filename, '/') || strchr(filename, '\\')) {
                free(filename);
                ACAP_HTTP_Respond_Error(response, 400, "Invalid filename");
                return;
            }
            char filepath[1024];
            snprintf(filepath, sizeof(filepath), "%s/%s", images_dir, filename);
            free(filename);

            FILE* f = fopen(filepath, "rb");
            if (!f) { ACAP_HTTP_Respond_Error(response, 404, "Not found"); return; }
            fseek(f, 0, SEEK_END);
            long size = ftell(f);
            rewind(f);

            unsigned char* buf = malloc(size);
            if (!buf) { fclose(f); ACAP_HTTP_Respond_Error(response, 500, "Out of memory"); return; }
            size_t rd = fread(buf, 1, size, f);
            fclose(f);

            ACAP_HTTP_Header_FILE(response, "image.jpg", "image/jpeg", (unsigned)rd);
            ACAP_HTTP_Respond_Data(response, rd, buf);
            free(buf);
            return;
        }

        /* ?thumb=NAME — serve a thumbnail */
        char* thumbname = ACAP_HTTP_Request_Param(request, "thumb");
        if (thumbname) {
            if (strchr(thumbname, '/') || strchr(thumbname, '\\')) {
                free(thumbname);
                ACAP_HTTP_Respond_Error(response, 400, "Invalid filename");
                return;
            }
            char tpath[1024];
            snprintf(tpath, sizeof(tpath), "%s/%s", thumbs_dir, thumbname);
            free(thumbname);

            FILE* f = fopen(tpath, "rb");
            if (!f) { ACAP_HTTP_Respond_Error(response, 404, "Not found"); return; }
            fseek(f, 0, SEEK_END);
            long size = ftell(f);
            rewind(f);

            unsigned char* buf = malloc(size);
            if (!buf) { fclose(f); ACAP_HTTP_Respond_Error(response, 500, "Out of memory"); return; }
            size_t rd = fread(buf, 1, size, f);
            fclose(f);

            ACAP_HTTP_Header_FILE(response, "thumb.jpg", "image/jpeg", (unsigned)rd);
            ACAP_HTTP_Respond_Data(response, rd, buf);
            free(buf);
            return;
        }

        ACAP_HTTP_Respond_Error(response, 400, "Missing parameter");
        return;
    }

    /* POST — delete files: {"delete": ["file1.jpg", ...]} */
    if (strcmp(method, "POST") == 0) {
        const char* body = ACAP_HTTP_Get_Body(request);
        if (!body) { ACAP_HTTP_Respond_Error(response, 400, "Missing body"); return; }

        cJSON* req = cJSON_Parse(body);
        if (!req) { ACAP_HTTP_Respond_Error(response, 400, "Invalid JSON"); return; }

        cJSON* deleteArr = cJSON_GetObjectItem(req, "delete");
        if (!cJSON_IsArray(deleteArr)) {
            cJSON_Delete(req);
            ACAP_HTTP_Respond_Error(response, 400, "Missing 'delete' array");
            return;
        }

        int deleted = 0;
        cJSON* item;
        cJSON_ArrayForEach(item, deleteArr) {
            if (!cJSON_IsString(item)) continue;
            const char* fname = item->valuestring;
            if (strchr(fname, '/') || strchr(fname, '\\')) continue;  /* reject traversal */

            char ipath[1024], tpath[1024];
            snprintf(ipath, sizeof(ipath), "%s/%s", images_dir, fname);
            snprintf(tpath, sizeof(tpath), "%s/%s", thumbs_dir, fname);
            if (remove(ipath) == 0) deleted++;
            remove(tpath);
        }
        cJSON_Delete(req);

        cJSON* result = cJSON_CreateObject();
        cJSON_AddNumberToObject(result, "deleted", deleted);
        ACAP_HTTP_Respond_JSON(response, result);
        cJSON_Delete(result);
        Enforce_Retention();
        return;
    }

    ACAP_HTTP_Respond_Error(response, 405, "Method not allowed");
}

/* ═══════════════════════════════════════════════════════════════════════════
   HTTP: export (ZIP download)
   ═══════════════════════════════════════════════════════════════════════════ */

void HTTP_Endpoint_export(const ACAP_HTTP_Response response, const ACAP_HTTP_Request request) {
    const char* method = ACAP_HTTP_Get_Method(request);
    if (!method) { ACAP_HTTP_Respond_Error(response, 400, "Invalid request"); return; }

    const char* sd_path = Storage_GetPath("SD_DISK");
    if (!sd_path) {
        ACAP_HTTP_Respond_Error(response, 503, "SD card not available");
        return;
    }

    char images_dir[768], thumbs_dir[768];
    snprintf(images_dir, sizeof(images_dir), "%s/images", sd_path);
    snprintf(thumbs_dir, sizeof(thumbs_dir), "%s/thumbs", sd_path);

    /* Collect filenames to export */
    char** filenames = NULL;
    int count = 0;
    int do_delete = 0;

    if (strcmp(method, "GET") == 0) {
        /* ?from=YYYYMMDDTHHmmss&to=YYYYMMDDTHHmmss&delete=0|1 */
        char* fromParam   = ACAP_HTTP_Request_Param(request, "from");
        char* toParam     = ACAP_HTTP_Request_Param(request, "to");
        char* deleteParam = ACAP_HTTP_Request_Param(request, "delete");
        if (deleteParam) { do_delete = atoi(deleteParam); free(deleteParam); }

        DIR* dir = opendir(images_dir);
        if (dir) {
            struct dirent* entry;
            int capacity = 0;
            while ((entry = readdir(dir)) != NULL) {
                if (entry->d_name[0] == '.') continue;
                size_t len = strlen(entry->d_name);
                if (len < 4 || strcmp(entry->d_name + len - 4, ".jpg") != 0) continue;

                /* Filter by date range if provided */
                char base[32];
                strncpy(base, entry->d_name, 15);
                base[15] = '\0';

                if (fromParam && strcmp(base, fromParam) < 0) continue;
                if (toParam   && strcmp(base, toParam)   > 0) continue;

                if (count >= capacity) {
                    capacity = capacity ? capacity * 2 : 64;
                    filenames = realloc(filenames, capacity * sizeof(char*));
                }
                filenames[count++] = strdup(entry->d_name);
            }
            closedir(dir);
        }
        if (fromParam) free(fromParam);
        if (toParam)   free(toParam);

    } else if (strcmp(method, "POST") == 0) {
        /* {"files": ["file1.jpg", ...], "delete": 0} */
        const char* body = ACAP_HTTP_Get_Body(request);
        if (!body) { ACAP_HTTP_Respond_Error(response, 400, "Missing body"); return; }

        cJSON* req = cJSON_Parse(body);
        if (!req) { ACAP_HTTP_Respond_Error(response, 400, "Invalid JSON"); return; }

        cJSON* delItem = cJSON_GetObjectItem(req, "delete");
        if (delItem && cJSON_IsNumber(delItem)) do_delete = delItem->valueint;

        cJSON* filesArr = cJSON_GetObjectItem(req, "files");
        if (cJSON_IsArray(filesArr)) {
            int capacity = 0;
            cJSON* item;
            cJSON_ArrayForEach(item, filesArr) {
                if (!cJSON_IsString(item)) continue;
                const char* fname = item->valuestring;
                if (strchr(fname, '/') || strchr(fname, '\\')) continue;
                if (count >= capacity) {
                    capacity = capacity ? capacity * 2 : 64;
                    filenames = realloc(filenames, capacity * sizeof(char*));
                }
                filenames[count++] = strdup(fname);
            }
        }
        cJSON_Delete(req);
    } else {
        ACAP_HTTP_Respond_Error(response, 405, "Method not allowed");
        return;
    }

    if (count == 0) {
        for (int i = 0; i < count; i++) free(filenames[i]);
        free(filenames);
        ACAP_HTTP_Respond_Error(response, 404, "No matching images");
        return;
    }

    qsort(filenames, count, sizeof(char*), str_cmp);

    /* Stream ZIP */
    imgutil_send_zip(response, images_dir, filenames, count, "sdcardcapture_export.zip");

    /* Optionally delete exported files */
    if (do_delete) {
        for (int i = 0; i < count; i++) {
            char ipath[1024], tpath[1024];
            snprintf(ipath, sizeof(ipath), "%s/%s", images_dir, filenames[i]);
            snprintf(tpath, sizeof(tpath), "%s/%s", thumbs_dir, filenames[i]);
            remove(ipath);
            remove(tpath);
        }
    }

    for (int i = 0; i < count; i++) free(filenames[i]);
    free(filenames);
}

/* ═══════════════════════════════════════════════════════════════════════════
   Signal & main
   ═══════════════════════════════════════════════════════════════════════════ */

static gboolean signal_handler(gpointer user_data) {
    (void)user_data;
    LOG("Received SIGTERM, shutting down\n");
    if (main_loop && g_main_loop_is_running(main_loop))
        g_main_loop_quit(main_loop);
    return G_SOURCE_REMOVE;
}

int main(void) {
    openlog(APP_PACKAGE, LOG_PID | LOG_CONS, LOG_USER);
    LOG("------ Starting %s ------\n", APP_PACKAGE);

    ACAP_STATUS_SetString("trigger", "status",  "Starting");
    ACAP_STATUS_SetString("storage", "status",  "Initializing");
    ACAP_STATUS_SetString("capture", "lastCapture", "Never");
    ACAP_STATUS_SetNumber("capture", "imageCount", 0);

    ACAP_Init(APP_PACKAGE, Settings_Updated_Callback);
    Restore_Object_Settings();

    Storage_Init();

    ACAP_HTTP_Node("trigger", HTTP_Endpoint_trigger);
    ACAP_HTTP_Node("capture", HTTP_Endpoint_capture);
    ACAP_HTTP_Node("images",  HTTP_Endpoint_images);
    ACAP_HTTP_Node("export",  HTTP_Endpoint_export);

    ACAP_EVENTS_SetCallback(My_Event_Callback);
    Apply_Trigger();

    LOG("Entering main loop\n");
    main_loop = g_main_loop_new(NULL, FALSE);
    GSource* sig = g_unix_signal_source_new(SIGTERM);
    if (sig) {
        g_source_set_callback(sig, signal_handler, NULL, NULL);
        g_source_attach(sig, NULL);
    }

    g_main_loop_run(main_loop);

    LOG("Terminating %s\n", APP_PACKAGE);
    if (timerId > 0) { g_source_remove(timerId); timerId = 0; }
    if (currentSubscriptionId > 0) { ACAP_EVENTS_Unsubscribe(currentSubscriptionId); }
    Storage_Cleanup();
    ACAP_Cleanup();
    closelog();
    return 0;
}
