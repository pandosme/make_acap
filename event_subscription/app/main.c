#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <glib.h>
#include <glib-unix.h>
#include <signal.h>

#include "ACAP.h"
#include "cJSON.h"

#define APP_PACKAGE	"event_subscription"

#define LOG(fmt, args...)    { syslog(LOG_INFO, fmt, ## args); printf(fmt, ## args);}
#define LOG_WARN(fmt, args...)    { syslog(LOG_WARNING, fmt, ## args); printf(fmt, ## args);}
#define LOG_TRACE(fmt, args...)    { syslog(LOG_INFO, fmt, ## args); printf(fmt, ## args); }
//#define LOG_TRACE(fmt, args...)    {}

void My_Event_Callback(cJSON *event) {
	char* json = cJSON_PrintUnformatted(event);
	LOG("%s: %s\n", __func__,json);
	free(json);
}

static GMainLoop *main_loop = NULL;

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
	
	ACAP( APP_PACKAGE, NULL );  //No configuration parameters

	ACAP_EVENTS_SetCallback( My_Event_Callback );
	cJSON* list = ACAP_FILE_Read( "html/config/subscriptions.json" );
	cJSON* event = list->child;
	while(event){
		ACAP_EVENTS_Subscribe(event);
		event = event->next;
	}
	cJSON_Delete(list);
	
	g_idle_add(ACAP_Process, NULL);

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
	LOG("Terminating and cleaning up %s\n",APP_PACKAGE);
    ACAP_Cleanup();
	closelog();
	return 0;
}
