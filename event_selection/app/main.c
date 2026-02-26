#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <glib.h>
#include <glib-unix.h>
#include <signal.h>
#include "ACAP.h"
#include "cJSON.h"

#define APP_PACKAGE	"event_selection"

#define LOG(fmt, args...)    { syslog(LOG_INFO, fmt, ## args); printf(fmt, ## args);}
#define LOG_WARN(fmt, args...)    { syslog(LOG_WARNING, fmt, ## args); printf(fmt, ## args);}
//#define LOG_TRACE(fmt, args...)    { syslog(LOG_INFO, fmt, ## args); printf(fmt, ## args); }
#define LOG_TRACE(fmt, args...)    {}

static GMainLoop *main_loop = NULL;
static int currentSubscriptionId = 0;
static guint timerId = 0;

/*
 * Extract a boolean state value from an incoming event.
 * Checks common property names used across Axis event types.
 * Returns 1 for active/true, 0 for inactive/false.
 */
static int
Extract_Event_State(cJSON* event) {
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
			if (cJSON_IsBool(item))
				return cJSON_IsTrue(item) ? 1 : 0;
			if (cJSON_IsNumber(item))
				return item->valueint ? 1 : 0;
			if (cJSON_IsString(item)) {
				if (strcmp(item->valuestring, "1") == 0 ||
				    strcmp(item->valuestring, "true") == 0)
					return 1;
				return 0;
			}
		}
	}
	return 1;  // Default to active if no recognized state property
}

/*
 * Event subscription callback.
 * Fires the ACAP's own declared events when the monitored event triggers.
 */
void
My_Event_Callback(cJSON *event, void* userdata) {
	char* json = cJSON_PrintUnformatted(event);
	if (json) {
		LOG("Event received: %s\n", json);
		free(json);
	}

	int state = Extract_Event_State(event);
	ACAP_EVENTS_Fire_State("state", state);
	ACAP_EVENTS_Fire("trigger");
	ACAP_STATUS_SetBool("trigger", "active", state);
	ACAP_STATUS_SetString("trigger", "lastTriggered", ACAP_DEVICE_ISOTime());
}

/*
 * Timer callback. Fires the trigger event periodically.
 */
static gboolean
Timer_Callback(gpointer user_data) {
	LOG("Timer triggered\n");
	ACAP_EVENTS_Fire("trigger");
	ACAP_EVENTS_Fire_State("state", 1);
	ACAP_STATUS_SetString("trigger", "lastTriggered", ACAP_DEVICE_ISOTime());

	// Brief delay to reset the stateful event
	g_usleep(100000);  // 100ms
	ACAP_EVENTS_Fire_State("state", 0);
	return G_SOURCE_CONTINUE;
}

/*
 * Read settings and apply the trigger configuration.
 * Unsubscribes/removes any existing trigger before applying the new one.
 */
static void
Apply_Trigger(void) {
	// Clean up existing subscription
	if (currentSubscriptionId > 0) {
		ACAP_EVENTS_Unsubscribe(currentSubscriptionId);
		currentSubscriptionId = 0;
		LOG("Unsubscribed from previous event\n");
	}

	// Clean up existing timer
	if (timerId > 0) {
		g_source_remove(timerId);
		timerId = 0;
		LOG("Removed previous timer\n");
	}

	// Reset state
	ACAP_EVENTS_Fire_State("state", 0);
	ACAP_STATUS_SetBool("trigger", "active", 0);

	cJSON* settings = ACAP_Get_Config("settings");
	if (!settings) {
		LOG_WARN("No settings available\n");
		ACAP_STATUS_SetString("trigger", "type", "none");
		ACAP_STATUS_SetString("trigger", "status", "No configuration");
		return;
	}

	cJSON* triggerTypeItem = cJSON_GetObjectItem(settings, "triggerType");
	const char* triggerType = triggerTypeItem ? triggerTypeItem->valuestring : "none";

	if (!triggerType) {
		triggerType = "none";
	}

	if (strcmp(triggerType, "event") == 0) {
		cJSON* ev = cJSON_GetObjectItem(settings, "event");
		if (ev && !cJSON_IsNull(ev)) {
			cJSON* nameItem = cJSON_GetObjectItem(ev, "name");
			const char* eventName = nameItem ? nameItem->valuestring : "Unknown";

			currentSubscriptionId = ACAP_EVENTS_Subscribe(ev, NULL);
			if (currentSubscriptionId > 0) {
				LOG("Subscribed to event: %s (id=%d)\n", eventName, currentSubscriptionId);
				ACAP_STATUS_SetString("trigger", "type", "event");
				ACAP_STATUS_SetString("trigger", "event", eventName);
				ACAP_STATUS_SetString("trigger", "status", "Monitoring event");
			} else {
				LOG_WARN("Failed to subscribe to event: %s\n", eventName);
				ACAP_STATUS_SetString("trigger", "type", "event");
				ACAP_STATUS_SetString("trigger", "event", eventName);
				ACAP_STATUS_SetString("trigger", "status", "Subscription failed");
			}
		} else {
			LOG_WARN("Event trigger selected but no event configured\n");
			ACAP_STATUS_SetString("trigger", "type", "event");
			ACAP_STATUS_SetString("trigger", "status", "No event selected");
		}
	} else if (strcmp(triggerType, "timer") == 0) {
		cJSON* timerItem = cJSON_GetObjectItem(settings, "timer");
		int interval = timerItem ? timerItem->valueint : 60;
		if (interval < 1) interval = 1;

		timerId = g_timeout_add_seconds(interval, Timer_Callback, NULL);
		LOG("Timer started: every %d seconds\n", interval);
		ACAP_STATUS_SetString("trigger", "type", "timer");
		ACAP_STATUS_SetNumber("trigger", "interval", interval);
		ACAP_STATUS_SetString("trigger", "status", "Timer running");
	} else {
		LOG("Trigger disabled\n");
		ACAP_STATUS_SetString("trigger", "type", "none");
		ACAP_STATUS_SetString("trigger", "status", "Disabled");
	}
}

/*
 * Settings update callback.
 * Called by ACAP wrapper once per changed property — `service` is the
 * property name (e.g. "triggerType", "event", "timer").
 * There is NO final call with service="settings".
 */
void
Settings_Updated_Callback(const char* service, cJSON* data) {
	(void)data;
	LOG_TRACE("Settings updated: %s\n", service);
	if (strcmp(service, "triggerType") == 0 ||
	    strcmp(service, "event")       == 0 ||
	    strcmp(service, "timer")       == 0) {
		Apply_Trigger();
	}
}

/*
 * HTTP endpoint for manual trigger testing.
 * GET: Returns trigger status
 * POST: Manually fires the trigger events
 */
void
HTTP_Endpoint_trigger(const ACAP_HTTP_Response response, const ACAP_HTTP_Request request) {
	const char* method = ACAP_HTTP_Get_Method(request);
	if (!method) {
		ACAP_HTTP_Respond_Error(response, 400, "Invalid request");
		return;
	}

	if (strcmp(method, "GET") == 0) {
		cJSON* status = ACAP_STATUS_Group("trigger");
		if (status) {
			ACAP_HTTP_Respond_JSON(response, status);
		} else {
			ACAP_HTTP_Respond_Text(response, "{}");
		}
		return;
	}

	if (strcmp(method, "POST") == 0) {
		LOG("Manual trigger fired\n");
		ACAP_EVENTS_Fire("trigger");
		ACAP_EVENTS_Fire_State("state", 1);
		ACAP_STATUS_SetString("trigger", "lastTriggered", ACAP_DEVICE_ISOTime());
		g_usleep(100000);
		ACAP_EVENTS_Fire_State("state", 0);
		ACAP_HTTP_Respond_Text(response, "Trigger fired");
		return;
	}

	ACAP_HTTP_Respond_Error(response, 405, "Method not allowed");
}

static gboolean
signal_handler(gpointer user_data) {
	LOG("Received SIGTERM, initiating shutdown\n");
	if (main_loop && g_main_loop_is_running(main_loop)) {
		g_main_loop_quit(main_loop);
	}
	return G_SOURCE_REMOVE;
}

int main(void) {
	openlog(APP_PACKAGE, LOG_PID|LOG_CONS, LOG_USER);
	LOG("------ Starting ACAP Service ------\n");
	ACAP_STATUS_SetString("trigger", "status", "Starting");

	ACAP_Init(APP_PACKAGE, Settings_Updated_Callback);
	ACAP_HTTP_Node("trigger", HTTP_Endpoint_trigger);
	ACAP_EVENTS_SetCallback(My_Event_Callback);

	// Apply initial trigger configuration from settings
	Apply_Trigger();

	LOG("Entering main loop\n");
	main_loop = g_main_loop_new(NULL, FALSE);
	GSource *signal_source = g_unix_signal_source_new(SIGTERM);
	if (signal_source) {
		g_source_set_callback(signal_source, signal_handler, NULL, NULL);
		g_source_attach(signal_source, NULL);
	} else {
		LOG_WARN("Signal detection failed");
	}

	g_main_loop_run(main_loop);

	LOG("Terminating and cleaning up %s\n", APP_PACKAGE);
	if (timerId > 0) {
		g_source_remove(timerId);
		timerId = 0;
	}
	ACAP_Cleanup();
	closelog();
	return 0;
}
