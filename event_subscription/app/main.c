#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <glib.h>
#include <signal.h>
#include "ACAP.h"
#include "cJSON.h"

#define APP_PACKAGE	"event_subscription"

#define LOG(fmt, args...)    { syslog(LOG_INFO, fmt, ## args); printf(fmt, ## args);}
#define LOG_WARN(fmt, args...)    { syslog(LOG_WARNING, fmt, ## args); printf(fmt, ## args);}
#define LOG_TRACE(fmt, args...)    { syslog(LOG_INFO, fmt, ## args); printf(fmt, ## args); }
//#define LOG_TRACE(fmt, args...)    {}

static GMainLoop *main_loop = NULL;
static volatile sig_atomic_t shutdown_flag = 0;

void term_handler(int signum) {
    if (signum == SIGTERM) {
        shutdown_flag = 1;
        if (main_loop) {
            g_main_loop_quit(main_loop);
        }
    }
}

void cleanup_resources(void) {
    LOG("Performing cleanup before shutdown\n");
    ACAP_Cleanup();
    closelog();
}

void My_Event_Callback(cJSON *event) {
	char* json = cJSON_PrintUnformatted(event);
	LOG("%s: %s\n", __func__,json);
	free(json);
}

int main(void) {
    struct sigaction action;
    
    // Initialize signal handling
    memset(&action, 0, sizeof(struct sigaction));
    action.sa_handler = term_handler;
    sigaction(SIGTERM, &action, NULL);
	
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
	
	g_idle_add(ACAP_HTTP_Process, NULL);

    main_loop = g_main_loop_new(NULL, FALSE);
    
    // Register cleanup function to be called at normal program termination
    atexit(cleanup_resources);
    
    g_main_loop_run(main_loop);
    
    if (shutdown_flag) {
        LOG("Received SIGTERM signal, shutting down gracefully\n");
    }
    
    if (main_loop) {
        g_main_loop_unref(main_loop);
    }
    
    return 0;
}
