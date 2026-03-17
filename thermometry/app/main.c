#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <glib.h>
#include <glib-unix.h>
#include <signal.h>

#include "cJSON.h"
#include "ACAP.h"
#include "MQTT.h"

#define APP_PACKAGE	"thermal"

#define LOG(fmt, args...)    { syslog(LOG_INFO, fmt, ## args); printf(fmt, ## args);}
#define LOG_WARN(fmt, args...)    { syslog(LOG_WARNING, fmt, ## args); printf(fmt, ## args);}
#define LOG_TRACE(fmt, args...)    {}

static GMainLoop *main_loop = NULL;
static guint poll_timer_id = 0;
static cJSON* eventSubscriptions = NULL;

/*-----------------------------------------------------
 * Thermometry VAPIX helpers
 *-----------------------------------------------------*/

static cJSON*
Thermometry_Call(const char* method, cJSON* params) {
	cJSON* request = cJSON_CreateObject();
	cJSON_AddStringToObject(request, "apiVersion", "1.0");
	cJSON_AddStringToObject(request, "context", APP_PACKAGE);
	cJSON_AddStringToObject(request, "method", method);
	if (params)
		cJSON_AddItemToObject(request, "params", params);
	else
		cJSON_AddItemToObject(request, "params", cJSON_CreateObject());

	char* body = cJSON_PrintUnformatted(request);
	cJSON_Delete(request);

	char* response = ACAP_VAPIX_Post("thermometry.cgi", body);
	free(body);
	if (!response)
		return NULL;

	cJSON* json = cJSON_Parse(response);
	free(response);
	if (!json)
		return NULL;

	if (cJSON_GetObjectItem(json, "error")) {
		cJSON* err = cJSON_GetObjectItem(json, "error");
		LOG_WARN("Thermometry %s error: %s\n", method,
			cJSON_GetStringValue(cJSON_GetObjectItem(err, "message")));
		cJSON_Delete(json);
		return NULL;
	}
	return json;
}

/*-----------------------------------------------------
 * MQTT Publishing
 *-----------------------------------------------------*/

static void
Publish_Area_Status(void) {
	cJSON* json = Thermometry_Call("getAreaStatus", NULL);
	if (!json)
		return;

	cJSON* data = cJSON_GetObjectItem(json, "data");
	if (data) {
		cJSON* areaList = cJSON_GetObjectItem(data, "areaList");
		if (areaList && cJSON_GetArraySize(areaList) > 0) {
			cJSON* payload = cJSON_CreateObject();
			cJSON_AddStringToObject(payload, "timestamp", ACAP_DEVICE_ISOTime());
			cJSON_AddStringToObject(payload, "serial", ACAP_DEVICE_Prop("serial"));
			cJSON_AddItemReferenceToObject(payload, "areas", areaList);
			char topic[128];
			snprintf(topic, sizeof(topic), "temperature/areas/%s", ACAP_DEVICE_Prop("serial"));
			MQTT_Publish_JSON(topic, payload, 0, 0);
			cJSON_Delete(payload);

			/* Update status */
			ACAP_STATUS_SetObject("thermometry", "areas", areaList);
		}
	}
	cJSON_Delete(json);
}

static void
Publish_Spot_Temperature(void) {
	cJSON* params = cJSON_CreateObject();
	cJSON_AddStringToObject(params, "coordinateSystem", "coord_neg1_1");
	cJSON* json = Thermometry_Call("getSpotTemperature", params);
	if (!json)
		return;

	cJSON* data = cJSON_GetObjectItem(json, "data");
	if (data) {
		cJSON* spotTemp = cJSON_GetObjectItem(data, "spotTemperature");
		if (spotTemp) {
			cJSON* payload = cJSON_CreateObject();
			cJSON_AddStringToObject(payload, "timestamp", ACAP_DEVICE_ISOTime());
			cJSON_AddStringToObject(payload, "serial", ACAP_DEVICE_Prop("serial"));
			cJSON_AddNumberToObject(payload, "temperature", spotTemp->valuedouble);
			cJSON* coords = cJSON_GetObjectItem(data, "spotCoordinates");
			if (coords)
				cJSON_AddItemReferenceToObject(payload, "coordinates", coords);
			char topic[128];
			snprintf(topic, sizeof(topic), "temperature/spot/%s", ACAP_DEVICE_Prop("serial"));
			MQTT_Publish_JSON(topic, payload, 0, 0);
			cJSON_Delete(payload);

			/* Update status */
			ACAP_STATUS_SetNumber("thermometry", "spotTemperature", spotTemp->valuedouble);
		}
	}
	cJSON_Delete(json);
}

/*-----------------------------------------------------
 * Poll timer
 *-----------------------------------------------------*/

static gboolean
Poll_Timer_Callback(gpointer user_data) {
	cJSON* settings = ACAP_Get_Config("settings");

	if (cJSON_IsTrue(cJSON_GetObjectItem(settings, "publishAreas")))
		Publish_Area_Status();

	if (cJSON_IsTrue(cJSON_GetObjectItem(settings, "publishSpot")))
		Publish_Spot_Temperature();

	return G_SOURCE_CONTINUE;
}

static void
Update_Poll_Timer(void) {
	if (poll_timer_id) {
		g_source_remove(poll_timer_id);
		poll_timer_id = 0;
	}

	cJSON* settings = ACAP_Get_Config("settings");
	int interval = 10;
	cJSON* intervalObj = cJSON_GetObjectItem(settings, "pollInterval");
	if (intervalObj && cJSON_IsNumber(intervalObj)) {
		interval = intervalObj->valueint;
		if (interval < 5)
			interval = 5;
	}

	poll_timer_id = g_timeout_add_seconds(interval, Poll_Timer_Callback, NULL);
	LOG("Poll timer set to %d seconds\n", interval);
	ACAP_STATUS_SetNumber("thermometry", "pollInterval", interval);
}

/*-----------------------------------------------------
 * Temperature scale management
 *-----------------------------------------------------*/

static void
Apply_Temperature_Scale(void) {
	cJSON* settings = ACAP_Get_Config("settings");
	const char* scale = cJSON_GetStringValue(cJSON_GetObjectItem(settings, "temperatureScale"));
	if (!scale)
		scale = "celsius";

	cJSON* params = cJSON_CreateObject();
	cJSON_AddStringToObject(params, "unit", scale);
	cJSON* json = Thermometry_Call("setTemperatureScale", params);
	if (json) {
		LOG("Temperature scale set to %s\n", scale);
		ACAP_STATUS_SetString("thermometry", "scale", scale);
		cJSON_Delete(json);
	}
}

/*-----------------------------------------------------
 * Event subscriptions
 *-----------------------------------------------------*/

static void
Setup_Event_Subscriptions(void) {
	cJSON* settings = ACAP_Get_Config("settings");
	if (!cJSON_IsTrue(cJSON_GetObjectItem(settings, "publishEvents")))
		return;

	eventSubscriptions = ACAP_FILE_Read("settings/subscriptions.json");
	if (!eventSubscriptions) {
		LOG_WARN("No subscriptions.json found\n");
		return;
	}

	cJSON* sub = eventSubscriptions->child;
	int count = 0;
	while (sub) {
		ACAP_EVENTS_Subscribe(sub, NULL);
		count++;
		sub = sub->next;
	}
	LOG("Subscribed to %d thermometry event(s)\n", count);
}

static void
Event_Callback(cJSON* event, void* userdata) {
	cJSON* settings = ACAP_Get_Config("settings");
	if (!cJSON_IsTrue(cJSON_GetObjectItem(settings, "publishEvents")))
		return;

	cJSON* payload = cJSON_CreateObject();
	cJSON_AddStringToObject(payload, "timestamp", ACAP_DEVICE_ISOTime());
	cJSON_AddStringToObject(payload, "serial", ACAP_DEVICE_Prop("serial"));
	cJSON_AddItemReferenceToObject(payload, "event", event);

	char topic[128];
	snprintf(topic, sizeof(topic), "temperature/event/%s", ACAP_DEVICE_Prop("serial"));
	MQTT_Publish_JSON(topic, payload, 0, 0);
	cJSON_Delete(payload);

	/* Check if alarm is active and fire our own event */
	cJSON* alarmActive = cJSON_GetObjectItem(event, "AlarmActive");
	if (alarmActive) {
		int active = 0;
		if (cJSON_IsBool(alarmActive))
			active = cJSON_IsTrue(alarmActive);
		else if (cJSON_IsString(alarmActive))
			active = (strcmp(alarmActive->valuestring, "1") == 0 ||
			          strcasecmp(alarmActive->valuestring, "true") == 0);
		ACAP_EVENTS_Fire_State("temperature_alarm", active);
	}

	LOG_TRACE("Event published to MQTT\n");
}

/*-----------------------------------------------------
 * Settings callback
 *-----------------------------------------------------*/

void
Settings_Updated_Callback(const char* service, cJSON* data) {
	LOG_TRACE("%s: %s\n", __func__, service);

	if (strcmp(service, "pollInterval") == 0)
		Update_Poll_Timer();
	else if (strcmp(service, "temperatureScale") == 0)
		Apply_Temperature_Scale();
}

/*-----------------------------------------------------
 * HTTP Publish endpoint (retained from mqtt template)
 *-----------------------------------------------------*/

void
HTTP_ENDPOINT_Publish(ACAP_HTTP_Response response, const ACAP_HTTP_Request request) {
	const char* method = ACAP_HTTP_Get_Method(request);
	if (!method || strcmp(method, "POST") != 0) {
		ACAP_HTTP_Respond_Error(response, 400, "Invalid Request Method");
		return;
	}

	const char* contentType = ACAP_HTTP_Get_Content_Type(request);
	if (!contentType || strcmp(contentType, "application/json") != 0) {
		ACAP_HTTP_Respond_Error(response, 415, "Unsupported Media Type");
		return;
	}

	const char* postBody = ACAP_HTTP_Get_Body(request);
	size_t postBodyLen = ACAP_HTTP_Get_Body_Length(request);
	if (!postBody || postBodyLen == 0) {
		ACAP_HTTP_Respond_Error(response, 400, "Missing POST data");
		return;
	}

	cJSON* body = cJSON_Parse(postBody);
	if (!body) {
		ACAP_HTTP_Respond_Error(response, 400, "Invalid JSON");
		return;
	}

	const char* topic = cJSON_GetStringValue(cJSON_GetObjectItem(body, "topic"));
	if (!topic || strlen(topic) == 0) {
		cJSON_Delete(body);
		ACAP_HTTP_Respond_Error(response, 400, "Topic must be set");
		return;
	}

	const char* payload = cJSON_GetStringValue(cJSON_GetObjectItem(body, "payload"));
	if (!payload) {
		cJSON_Delete(body);
		ACAP_HTTP_Respond_Error(response, 400, "Payload must be set");
		return;
	}

	int published = MQTT_Publish(topic, payload, 0, 0);
	cJSON_Delete(body);
	if (published)
		ACAP_HTTP_Respond_Text(response, "Message sent");
	else
		ACAP_HTTP_Respond_Error(response, 500, "Message publish failed");
}

/*-----------------------------------------------------
 * MQTT callbacks
 *-----------------------------------------------------*/

void
MQTT_Status_Callback(int state) {
	char topic[64];
	cJSON* message = NULL;

	switch (state) {
		case MQTT_INITIALIZING:
			LOG("MQTT: Initializing\n");
			break;
		case MQTT_CONNECTING:
			LOG("MQTT: Connecting\n");
			break;
		case MQTT_CONNECTED:
			LOG("MQTT: Connected\n");
			snprintf(topic, sizeof(topic), "connect/%s", ACAP_DEVICE_Prop("serial"));
			message = cJSON_CreateObject();
			cJSON_AddTrueToObject(message, "connected");
			cJSON_AddStringToObject(message, "address", ACAP_DEVICE_Prop("IPv4"));
			cJSON_AddStringToObject(message, "app", APP_PACKAGE);
			MQTT_Publish_JSON(topic, message, 0, 1);
			cJSON_Delete(message);
			break;
		case MQTT_DISCONNECTING:
			snprintf(topic, sizeof(topic), "connect/%s", ACAP_DEVICE_Prop("serial"));
			message = cJSON_CreateObject();
			cJSON_AddFalseToObject(message, "connected");
			cJSON_AddStringToObject(message, "address", ACAP_DEVICE_Prop("IPv4"));
			MQTT_Publish_JSON(topic, message, 0, 1);
			cJSON_Delete(message);
			break;
		case MQTT_RECONNECTED:
			LOG("MQTT: Reconnected\n");
			break;
		case MQTT_DISCONNECTED:
			LOG("MQTT: Disconnected\n");
			break;
	}
}

void
MQTT_Message_Callback(const char* topic, const char* payload) {
	LOG("MQTT Message: %s %s\n", topic, payload);
}

/*-----------------------------------------------------
 * Signal handler
 *-----------------------------------------------------*/

static gboolean
signal_handler(gpointer user_data) {
	LOG("Received SIGTERM, initiating shutdown\n");
	if (main_loop && g_main_loop_is_running(main_loop))
		g_main_loop_quit(main_loop);
	return G_SOURCE_REMOVE;
}

/*-----------------------------------------------------
 * Main
 *-----------------------------------------------------*/

int
main(void) {
	openlog(APP_PACKAGE, LOG_PID | LOG_CONS, LOG_USER);
	LOG("------ Starting Thermal MQTT ACAP ------\n");

	ACAP_Init(APP_PACKAGE, Settings_Updated_Callback);
	ACAP_HTTP_Node("publish", HTTP_ENDPOINT_Publish);

	/* Initialize status */
	ACAP_STATUS_SetString("thermometry", "scale", "celsius");
	ACAP_STATUS_SetNumber("thermometry", "pollInterval", 10);
	ACAP_STATUS_SetNull("thermometry", "areas");
	ACAP_STATUS_SetNull("thermometry", "spotTemperature");

	/* Apply temperature scale from settings */
	Apply_Temperature_Scale();

	/* Set up event subscriptions */
	ACAP_EVENTS_SetCallback(Event_Callback);
	Setup_Event_Subscriptions();

	/* Initialize MQTT */
	MQTT_Init(MQTT_Status_Callback, MQTT_Message_Callback);

	/* Start poll timer */
	Update_Poll_Timer();

	/* Main loop */
	main_loop = g_main_loop_new(NULL, FALSE);
	GSource* signal_source = g_unix_signal_source_new(SIGTERM);
	if (signal_source) {
		g_source_set_callback(signal_source, signal_handler, NULL, NULL);
		g_source_attach(signal_source, NULL);
	} else {
		LOG_WARN("Signal detection failed\n");
	}

	g_main_loop_run(main_loop);

	LOG("Terminating and cleaning up %s\n", APP_PACKAGE);
	MQTT_Status_Callback(MQTT_DISCONNECTING);
	if (poll_timer_id) {
		g_source_remove(poll_timer_id);
		poll_timer_id = 0;
	}
	MQTT_Cleanup();
	if (eventSubscriptions) {
		cJSON_Delete(eventSubscriptions);
		eventSubscriptions = NULL;
	}
	ACAP_Cleanup();
	closelog();

	return 0;
}
