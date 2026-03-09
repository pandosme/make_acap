# SD Card Capture

An Axis Camera Application Platform (ACAP) that captures JPEG images directly to the camera's SD card. Images can be triggered by any device event, on a configurable timer, or manually via the web UI or REST API.

This application was built as a demonstration of **Vibe Coding** — using AI to design and implement a fully functional ACAP from scratch through natural language conversation.

---

## What It Does

- **Event-triggered capture** — subscribe to any camera event (motion detection, digital input, analytics, etc.) and capture a JPEG when it fires
- **Timer-triggered capture** — capture at a fixed interval (seconds, minutes, or hours)
- **Manual capture** — trigger a capture from the web UI or via HTTP POST
- **Dual-resolution storage** — saves a full-resolution image and a 320×180 thumbnail side-by-side
- **Automatic retention** — enforces configurable policies to keep SD card usage under control:
  - Maximum image count (oldest deleted first)
  - Maximum image age in weeks
  - Minimum free space percentage
- **Image viewer** — built-in browser UI to browse, preview, and delete captured images
- **Export API** — download images as a ZIP archive, optionally filtered by date range
- **ACAP events** — fires its own trigger/state events so other applications or rules can react

---

## Web UI

The application has three pages accessible from the camera's ACAP web interface:

| Page | Description |
|------|-------------|
| **Trigger** | Configure trigger type, event selection, timer interval, capture resolution, and retention policy |
| **Viewer** | Browse thumbnails, view full images, delete individual or selected images |
| **About** | Application details, device info, and a reference for the REST API endpoints |

---

## REST API

All endpoints are under `/local/sdcardcapture/` and require admin credentials.

| Method | Endpoint | Description |
|--------|----------|-------------|
| `GET` | `/images?list` | JSON array of all stored images (filename, timestamp, size, hasThumb) |
| `GET` | `/images?file=YYYYMMDDTHHmmss.jpg` | Download a full-resolution image |
| `GET` | `/images?thumb=YYYYMMDDTHHmmss.jpg` | Download a 320×180 thumbnail |
| `POST` | `/images` | Delete images: `{"delete": ["file1.jpg", ...]}` |
| `POST` | `/capture` | Manually trigger a capture |
| `GET` | `/trigger` | Current trigger status (JSON) |
| `POST` | `/trigger` | Manually fire a trigger + capture |
| `GET` | `/export?from=20260101T000000&to=20261231T235959&delete=0` | Download filtered images as ZIP |
| `POST` | `/export` | Download selected images as ZIP: `{"files": [...], "delete": 0}` |

---

## Building

Requires Docker with access to the Axis ACAP Native SDK images.

```sh
./build.sh
```

This produces two `.eap` packages:
- `SD_Card_Capture_1_0_0_aarch64.eap` — for ARTPEC-8 and newer cameras
- `SD_Card_Capture_1_0_0_armv7hf.eap` — for ARTPEC-7 and older cameras

Install via **Apps** in the camera's web interface, or use the VAPIX application management API.

**Requirements:** Docker, ACAP Native SDK 12.2.0, firmware 10.x or later, SD card inserted and formatted.

---

## Project Structure

```
sdcardcapture/
├── build.sh              # Build script (Docker-based, both architectures)
├── Dockerfile            # SDK build container
└── app/
    ├── main.c            # Application logic (storage, capture, events, HTTP endpoints)
    ├── ACAP.h / ACAP.c   # ACAP framework (HTTP, settings, status, events)
    ├── imgutil.h / .c    # JPEG save and ZIP streaming utilities
    ├── cJSON.h / .c      # JSON library
    ├── Makefile
    ├── manifest.json     # Package metadata and permissions
    ├── settings/
    │   ├── settings.json # Default settings
    │   └── events.json   # ACAP event declarations
    └── html/
        ├── index.html    # Trigger & retention configuration UI
        ├── viewer.html   # Image browser UI
        ├── about.html    # About page + API reference
        ├── css/
        └── js/
```

---

## Vibe Coding This ACAP

This application was created entirely through **Vibe Coding** — a workflow where you describe what you want in plain language and an AI model writes and iterates on the code. No prior ACAP or C experience required to get started.

### The Workflow Used

| Phase | Model | Role |
|-------|-------|------|
| **Planning** | Claude Opus 4.6 | Architecture, file structure, API design, trade-off analysis |
| **Coding** | Claude Sonnet 4.6 | Implementation, iteration, debugging, UI development |

**Opus 4.6** was used in plan mode (`/plan` in Claude Code) to think through the overall design: what the ACAP should do, what C APIs to use (VDO for snapshots, axStorage for SD card, ACAP framework for HTTP/settings/events), how retention should work, and what the REST API should look like.

**Sonnet 4.6** then implemented everything — writing `main.c`, the HTML/JS pages, and iterating based on feedback — staying in the existing ACAP framework conventions.

### Getting Started with Claude Code

Install Claude Code and open this project:

```sh
cd sdcardcapture
claude
```

Use plan mode for design decisions:
```
/plan
```

Then describe changes or enhancements in plain language. Claude Code reads the existing code, understands the ACAP framework conventions, and makes targeted edits.

### Example Prompts to Enhance This ACAP

**Add a new capture resolution option:**
> "Add 2560x1440 as a selectable resolution option in the settings dropdown."

**Add a cooldown between event captures:**
> "After an event triggers a capture, ignore further events for 30 seconds. Add a configurable cooldown setting to the settings page."

**Add MQTT publish on capture:**
> "After each capture, publish a message to an MQTT broker with the filename and timestamp. Add broker/topic fields to the settings page."

**Add overlay text to images:**
> "Burn the camera name and timestamp as text into each captured JPEG before saving."

**Add FTP upload:**
> "After each capture, upload the image to an FTP server. Add FTP host, username, password, and path to settings."

**Change the thumbnail size:**
> "Change thumbnails from 320x180 to 640x360."

**Add per-event capture count limits:**
> "Limit how many images can be captured per event trigger per hour. Add a 'max captures per hour' setting."

**Expose a webhook on capture:**
> "Send an HTTP POST to a configurable webhook URL each time an image is captured, with filename, timestamp, and camera serial number in the JSON body."

### Tips for Effective Vibe Coding

- **Start with planning.** Use `/plan` with Opus 4.6 to think through new features before writing code. Ask it to identify which files need to change and what the approach should be.
- **Reference existing patterns.** Tell Claude to follow the same patterns already used in the codebase — e.g., "add a new HTTP endpoint the same way the `/images` endpoint is structured."
- **Iterate in small steps.** Add one feature at a time, build, test on the camera, then add the next.
- **Be specific about constraints.** If something must be non-blocking, thread-safe, or memory-limited, say so explicitly.
- **Test with the build script.** After each change, run `./build.sh` and install the `.eap` to verify the change works on real hardware before proceeding.
- **Use syslog for debugging.** The ACAP logs to syslog — check it on the camera at `System > Logs` or via SSH with `journalctl`.

---

## Settings Reference

| Setting | Default | Description |
|---------|---------|-------------|
| `triggerType` | `"none"` | `"none"`, `"event"`, or `"timer"` |
| `triggerEvent` | `null` | Full event descriptor object (set via UI) |
| `timer` | `60` | Timer interval in seconds |
| `resolution` | `"1920x1080"` | Capture resolution (populated from camera VAPIX) |
| `maxImages` | `1000` | Maximum number of stored images |
| `retentionWeeks` | `4` | Delete images older than this many weeks |
| `minFreeSpacePercent` | `10` | Delete oldest images when SD card free space drops below this |

---

## License

MIT — Fred Juhlin
