# ACAPP Config
Demonstgrates how to manage dynamic ACAP configuration settings.  The configutation definitions and default values are in file source/html/config/settings.json.  This demo also shows how to setup a simple http node (/test) that responds with the text "Hello World".

APP.c/.h creates two http enpoints.  The /app should is for a web page to get all the different settings in a single call.  /settings is there to get/set ACAP specific settings.

```
GET /app
GET /settings
GET /settings?json={"some":"parameter,"value":1}
```

Modified parameters are stores in the device /user/local/packaID/localdata/settings.json and Additional settings can be easily added in future updates.

As all settings are stored in cJSON, some additional coding is required to use them.  And you need to know what type they are.  Moe info in cJSON.h.

The following example shows one way to use settings.  It checks that the property "someString" is present and gets a pointer to the string;
```
cJSON* settings = APP_Settings();
char* someString = cJSON_GetObjectItem(settings,"someString")?cJSON_GetObject(settings,"someString")->valuestring:0;
if( !someString ) {
  LOG("Error");
  return;
}

int someValue = cJSON_GetObjectItem(settings,"someValue")?cJSON_GetObject(settings,"someValue")->valueint:0;

int someBool = cJSON_GetObjectItem(settings,"someBool")?(cJSON_GetObject(settings,"someBool")->type == cJSON_True):0;

```
