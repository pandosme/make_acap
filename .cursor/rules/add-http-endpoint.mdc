---
description: How to add a new HTTP endpoint to an ACAP application
globs: "**/manifest.json,**/main.c"
alwaysApply: false
---

# Adding an HTTP Endpoint

## Steps (all three are required)

### 1. manifest.json — Declare the endpoint
Add an entry to `httpConfig`:
```json
"httpConfig": [
  {"name": "myendpoint", "access": "admin", "type": "fastCgi"}
]
```

### 2. main.c — Register the handler
In `main()`, after `ACAP_Init()`:
```c
ACAP_HTTP_Node("myendpoint", HTTP_Endpoint_myendpoint);
```

### 3. main.c — Implement the handler
```c
void
HTTP_Endpoint_myendpoint(const ACAP_HTTP_Response response, const ACAP_HTTP_Request request) {
    const char* method = ACAP_HTTP_Get_Method(request);
    if (!method) {
        ACAP_HTTP_Respond_Error(response, 400, "Invalid request");
        return;
    }

    if (strcmp(method, "GET") == 0) {
        char* param = ACAP_HTTP_Request_Param(request, "key");
        // Use param...
        ACAP_HTTP_Respond_Text(response, "OK");
        free(param);  // MUST free — caller owns
        return;
    }

    if (strcmp(method, "POST") == 0) {
        const char* body = ACAP_HTTP_Get_Body(request);
        size_t len = ACAP_HTTP_Get_Body_Length(request);
        if (!body || len == 0) {
            ACAP_HTTP_Respond_Error(response, 400, "Missing body");
            return;
        }
        cJSON* json = cJSON_Parse(body);
        if (!json) {
            ACAP_HTTP_Respond_Error(response, 400, "Invalid JSON");
            return;
        }
        // Process json...
        ACAP_HTTP_Respond_JSON(response, json);
        cJSON_Delete(json);
        return;
    }

    ACAP_HTTP_Respond_Error(response, 405, "Method not allowed");
}
```

## Important Rules
- The endpoint name in manifest.json must match the first argument to `ACAP_HTTP_Node()`
- HTTP types are opaque — use accessor functions only
- `ACAP_HTTP_Request_Param()` returns `char*` — caller must `free()`
- `ACAP_HTTP_Get_Body()` returns `const char*` — do NOT free
- Always validate method, content type, and body before processing
