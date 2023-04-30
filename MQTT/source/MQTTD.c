/*------------------------------------------------------------------
 *  Fred Juhlin (2022)
 *------------------------------------------------------------------*/

#include <syslog.h>
#define _GNU_SOURCE
#include <string.h>
#include <stdio.h>
#include <gio/gio.h>
#include <unistd.h>
#include <glib.h>
#include <glib-object.h>
#include <syslog.h>
#include "MQTTD.h"


#define LOG(fmt, args...)		{ syslog(LOG_INFO, fmt, ## args); printf(fmt, ## args);}
#define LOG_WARN(fmt, args...)	{ syslog(LOG_WARNING, fmt, ## args); printf(fmt, ## args);}
#define LOG_TRACE(fmt, args...)	{ syslog(LOG_INFO, fmt, ## args); printf(fmt, ## args); }
//#define LOG_TRACE(fmt, args...) {}


#define DBUS_SERVICE_NAME "com.axis.MQTTD"
#define DBUS_OBJECT_PATH "/"
#define DBUS_INTERFACE_NAME "com.axis.MQTTD"


/* dbus parameter */
int axMQTTCD_get_dbus_proxy1(GDBusProxy **proxy) {
	GError *error = NULL;
LOG_TRACE("%s: Debug 1\n",__func__);	
	*proxy = g_dbus_proxy_new_for_bus_sync (G_BUS_TYPE_SYSTEM,
										  G_DBUS_PROXY_FLAGS_DO_NOT_LOAD_PROPERTIES,
										  NULL, /* GDBusInterfaceInfo */
										  DBUS_SERVICE_NAME,
										  DBUS_OBJECT_PATH,
										  DBUS_INTERFACE_NAME,
										  NULL, /* GCancellable */
										  &error);
	if (*proxy == NULL) {
		LOG_TRACE("%s: %s", __func__,error->message);
		g_error_free(error);
		return MQTTD_NO_SIMQTT;
	}
LOG_TRACE("%s: Debug 2\n",__func__);	

	return MQTTD_OK;
}

int
MQTTD_Publish(const char *topic, const char *payload, int retained) { 
	LOG_TRACE("%s: %s %s\n",__func__,topic,payload);
	
	if( !topic || strlen(topic) < 2 || !payload )
		return MQTTD_INVALID;
LOG_TRACE("%s: Debug 1\n",__func__);	
	GDBusProxy *proxy = NULL;
	if (axMQTTCD_get_dbus_proxy1(&proxy) != MQTTD_OK ) {
LOG_TRACE("%s: Debug 2\n",__func__);	
		return MQTTD_NO_SIMQTT;
	}
LOG_TRACE("%s: Debug 3\n",__func__);

	GError *error = NULL;
	GVariant *dbusmessage = NULL;
	dbusmessage = g_dbus_proxy_call_sync (proxy,
										"publish",
										g_variant_new ("(ssi)",topic,payload,retained),
										G_DBUS_CALL_FLAGS_NONE,
										5000, NULL, &error);

	gint32 res = MQTTD_NO_SIMQTT;
	if(error) {
	  LOG_TRACE("%s: %s", __func__,error->message);
	  g_error_free (error);
	} else {
		LOG_TRACE("%s: DBus respone %d\n",__func__,res);
		g_variant_get(dbusmessage,"(i)",&res);
	}
LOG_TRACE("%s: Debug 3\n",__func__);	

	g_variant_unref(dbusmessage);  
	g_object_unref(proxy);
	LOG_TRACE("%s: Exit %d\n",__func__,res);
	return res;
}


