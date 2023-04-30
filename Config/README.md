# Config Example
Demonstsrates how to add and manage dynamic configuration settings for an ACAP.
All files with CAPITOL letters are wrappers arround ACAP resources to simplify ACAP development. 


HTTP endpoints
```
GET /app
GET /settings
GET /settings?json={"some":"parameter,"value":1}
```

Setting parameters are declared in html/config/settings.json.
Modified parameters are stores in the device /user/local/packaID/localdata/settings.json.

As all settings are stored in cJSON, some additional coding is required to use them.  Look in main.c how to access settings.

