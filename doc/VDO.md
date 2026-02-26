# VDO (Video Device Object) Reference

Video capture and streaming API for Axis cameras in ACAP applications.

## Overview

VDO provides access to camera video streams with configurable resolution, format, and framerate. It handles buffer management and delivers frames for processing.

## ⚠ Critical: Do Not Call `vdo_stream_snapshot` from GLib Timer Callbacks

`vdo_stream_snapshot()` is a blocking call that waits for a full video frame. It may internally require GLib main-loop event dispatch to complete. If you call it **directly** from a GLib timer callback (which runs on the main loop), the main loop is blocked while waiting for VDO, which itself needs the main loop — resulting in a silent hang with no image captured.

The function **does work** when called from a non-main-loop context (e.g., a FastCGI handler thread), which is why `Capture Now` buttons work but `g_timeout_add_seconds` timers appear to silently fail.

### Fix: defer via `g_idle_add()`

Never call `vdo_stream_snapshot()` directly from a timer callback. Instead, schedule the capture as a GLib idle source so the timer returns immediately and VDO runs in the next main-loop iteration:

```c
static gboolean do_capture_idle(gpointer user_data) {
    (void)user_data;
    // Safe to call vdo_stream_snapshot() here — main loop is free
    Capture_Image();
    return G_SOURCE_REMOVE;   /* run once */
}

static gboolean Timer_Callback(gpointer user_data) {
    (void)user_data;
    g_idle_add(do_capture_idle, NULL);   /* defer; return immediately */
    return G_SOURCE_CONTINUE;
}
```

The same rule applies to any other blocking I/O (large file writes, network requests) called from the GLib main loop.

## Headers

```c
#include "vdo-stream.h"
#include "vdo-buffer.h"
#include "vdo-map.h"
#include "vdo-channel.h"
#include "vdo-types.h"
#include "vdo-frame.h"
#include "vdo-error.h"
```

## Stream Creation

### Basic Stream Setup

```c
VdoMap* settings = vdo_map_new();

// Required settings
vdo_map_set_uint32(settings, "channel", 1);
vdo_map_set_uint32(settings, "format", VDO_FORMAT_YUV);
vdo_map_set_uint32(settings, "width", 640);
vdo_map_set_uint32(settings, "height", 480);

// Create stream
GError* error = NULL;
VdoStream* stream = vdo_stream_new(settings, NULL, &error);
```

### Stream Settings Reference

| Setting | Type | Description |
|---------|------|-------------|
| `channel` | uint32 | Video channel (typically 1 for main sensor) |
| `format` | uint32 | Pixel format (VDO_FORMAT_*) |
| `width` | uint32 | Stream width in pixels |
| `height` | uint32 | Stream height in pixels |
| `framerate` | double | Target framerate (e.g., 15.0, 30.0) |
| `buffer.strategy` | uint32 | Buffer management strategy |
| `buffer.count` | uint32 | Number of buffers to allocate |
| `dynamic.framerate` | boolean | Allow runtime framerate changes |
| `socket.blocking` | boolean | Blocking vs non-blocking buffer fetch |
| `image.fit` | string | Aspect ratio handling mode |

### Resolution Setting

```c
// Using VdoPair32u for resolution
VdoPair32u resolution = { .w = 1920, .h = 1080 };
vdo_map_set_pair32u(settings, "resolution", resolution);
```

## Pixel Formats

```c
typedef enum {
    VDO_FORMAT_YUV,         // NV12 (Y plane + interleaved UV), 1.5 bytes/pixel
    VDO_FORMAT_RGB,         // RGB interleaved, 3 bytes/pixel
    VDO_FORMAT_PLANAR_RGB,  // RGB planar (separate R, G, B planes), 3 bytes/pixel
    VDO_FORMAT_JPEG,        // JPEG compressed
    VDO_FORMAT_H264,        // H.264 encoded
    VDO_FORMAT_H265,        // H.265/HEVC encoded
} VdoFormat;
```

**Buffer Size Calculations:**
- NV12: `width * height * 3 / 2`
- RGB/Planar RGB: `width * height * 3`

## Buffer Strategies

```c
typedef enum {
    VDO_BUFFER_STRATEGY_INFINITE,  // Default - VDO manages buffers internally
    VDO_BUFFER_STRATEGY_EXPLICIT,  // Application allocates and manages buffers
} VdoBufferStrategy;
```

### Explicit Buffer Management

```c
vdo_map_set_uint32(settings, "buffer.strategy", VDO_BUFFER_STRATEGY_EXPLICIT);

// After stream creation, allocate buffers
for (int i = 0; i < NUM_BUFFERS; i++) {
    VdoBuffer* buffer = vdo_stream_buffer_alloc(stream, NULL, &error);

    // Pre-map memory (optimization)
    void* data = vdo_buffer_get_data(buffer);

    // Enqueue buffer for VDO to fill
    vdo_stream_buffer_enqueue(stream, buffer, &error);
}
```

### Infinite Buffer Strategy (Default)

```c
// VDO manages buffers internally
vdo_map_set_uint32(settings, "buffer.count", 2);  // Suggest buffer count
```

## Image Fit Modes

The `image.fit` setting controls aspect ratio handling when stream resolution differs from requested:

```c
vdo_map_set_string(settings, "image.fit", "scale");  // or "crop"
```

| Mode | Behavior | Use Case |
|------|----------|----------|
| `"scale"` | Scale to fit, may distort aspect ratio | Full field of view needed |
| `"crop"` | Center-crop to maintain aspect ratio | Aspect ratio preservation critical |

## Dynamic Framerate

Enable runtime framerate adjustment:

```c
// During setup
vdo_map_set_boolean(settings, "dynamic.framerate", true);

// At runtime - adjust based on processing speed
VdoMap* info = vdo_stream_get_info(stream, &error);
double current_fps = vdo_map_get_double(info, "framerate", NULL);

// Calculate new framerate based on inference time
double new_fps = 1000.0 / inference_time_ms;
vdo_stream_set_framerate(stream, new_fps, &error);
```

## Channel Utilities

### Get Available Resolutions

```c
VdoChannel* channel = vdo_channel_get(1, &error);
VdoResolutionSet* resolutions = vdo_channel_get_resolutions(channel, NULL, &error);

for (size_t i = 0; i < resolutions->count; i++) {
    VdoResolution* res = &resolutions->resolutions[i];
    printf("Available: %ux%u\n", res->width, res->height);
}

g_free(resolutions);
g_object_unref(channel);
```

### Get Aspect Ratio

```c
VdoChannel* channel = vdo_channel_get(1, &error);
VdoPair32u aspect = vdo_channel_get_aspect_ratio(channel);
printf("Aspect ratio: %u:%u\n", aspect.w, aspect.h);  // e.g., 16:9
```

### Get Global Rotation

```c
uint32_t rotation = vdo_channel_get_rotation(channel);  // 0, 90, 180, 270
```

### Find Best Resolution

```c
// Find smallest resolution >= requested size
bool choose_resolution(unsigned int req_w, unsigned int req_h,
                       unsigned int* out_w, unsigned int* out_h) {
    VdoChannel* channel = vdo_channel_get(1, &error);
    VdoResolutionSet* set = vdo_channel_get_resolutions(channel, NULL, &error);

    unsigned int best_area = UINT_MAX;
    int best_idx = -1;

    for (size_t i = 0; i < set->count; i++) {
        VdoResolution* res = &set->resolutions[i];
        if (res->width >= req_w && res->height >= req_h) {
            unsigned int area = res->width * res->height;
            if (area < best_area) {
                best_area = area;
                best_idx = i;
            }
        }
    }

    if (best_idx >= 0) {
        *out_w = set->resolutions[best_idx].width;
        *out_h = set->resolutions[best_idx].height;
        return true;
    }
    return false;
}
```

## Frame Fetching

### Blocking Mode

```c
VdoBuffer* buffer = vdo_stream_get_buffer(stream, &error);
if (buffer) {
    void* data = vdo_buffer_get_data(buffer);
    size_t size = vdo_buffer_get_size(buffer);

    // Process frame...

    // Return buffer (explicit strategy)
    vdo_stream_buffer_enqueue(stream, buffer, &error);
    g_object_unref(buffer);
}
```

### Non-Blocking Mode with Poll

```c
vdo_map_set_boolean(settings, "socket.blocking", false);

// Get file descriptor for polling
int fd = vdo_stream_get_fd(stream);

struct pollfd pfd = { .fd = fd, .events = POLLIN };
int ret = poll(&pfd, 1, timeout_ms);

if (ret > 0 && (pfd.revents & POLLIN)) {
    VdoBuffer* buffer = vdo_stream_get_buffer(stream, &error);
    // Process...
}
```

## Stream Lifecycle

```c
// Start streaming
vdo_stream_start(stream, &error);

// ... process frames ...

// Stop streaming
vdo_stream_stop(stream);

// Cleanup
g_object_unref(stream);
```

## Error Handling

```c
#include "vdo-error.h"

GError* error = NULL;
VdoBuffer* buffer = vdo_stream_get_buffer(stream, &error);

if (!buffer) {
    if (vdo_error_is_expected(&error)) {
        // Transient error (e.g., global rotation change)
        syslog(LOG_INFO, "Expected VDO error: %s", error->message);
        g_clear_error(&error);
        // Retry or recreate stream
    } else {
        // Unexpected error
        syslog(LOG_ERR, "VDO error: %s", error->message);
    }
}
```

## Complete Example

```c
#include <vdo-stream.h>
#include <vdo-buffer.h>
#include <vdo-map.h>
#include <glib.h>

bool create_video_stream(unsigned int width, unsigned int height,
                         double framerate, VdoFormat format,
                         VdoStream** out_stream) {
    GError* error = NULL;

    VdoMap* settings = vdo_map_new();
    if (!settings) return false;

    vdo_map_set_uint32(settings, "channel", 1);
    vdo_map_set_uint32(settings, "format", format);

    VdoPair32u resolution = { .w = width, .h = height };
    vdo_map_set_pair32u(settings, "resolution", resolution);

    vdo_map_set_double(settings, "framerate", framerate);
    vdo_map_set_boolean(settings, "dynamic.framerate", true);
    vdo_map_set_uint32(settings, "buffer.count", 3);
    vdo_map_set_string(settings, "image.fit", "scale");

    VdoStream* stream = vdo_stream_new(settings, NULL, &error);
    g_object_unref(settings);

    if (!stream) {
        syslog(LOG_ERR, "Failed to create stream: %s", error->message);
        g_clear_error(&error);
        return false;
    }

    if (!vdo_stream_start(stream, &error)) {
        syslog(LOG_ERR, "Failed to start stream: %s", error->message);
        g_object_unref(stream);
        g_clear_error(&error);
        return false;
    }

    *out_stream = stream;
    return true;
}
```

## Best Practices

1. **Buffer Count**: Use minimal buffers (2-3) to reduce memory usage
2. **Resolution**: Query available resolutions rather than assuming support
3. **Framerate**: Start conservative, adjust based on actual processing time
4. **Error Handling**: Always check for expected vs unexpected errors
5. **Memory**: Unref GObjects and free GLib allocations properly
6. **Dynamic Framerate**: Enable for ML applications to balance quality vs latency

## Related Documentation

- [LAROD.md](LAROD.md) - ML inference framework (often used with VDO frames)
- ACAP Native SDK documentation
