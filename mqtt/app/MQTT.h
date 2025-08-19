/**
 * MQTT.h
 * Fred Juhlin 2025
 * Version 2.0
 */
 
#ifndef _MQTT_Service_H_
#define _MQTT_Service_H_

#include "cJSON.h"

#ifdef  __cplusplus
extern "C" {
#endif

#define MQTT_INITIALIZING  1
#define MQTT_CONNECTING    2
#define MQTT_DISCONNECTING 3
#define MQTT_CONNECTED     4
#define MQTT_DISCONNECTED  5
#define MQTT_RECONNECTING  6
#define MQTT_RECONNECTED   7


typedef void (*MQTT_Callback_Connection) (int state);
typedef void (*MQTT_Callback_Message) (const char *topic, const char *payload);

int    MQTT_Init( MQTT_Callback_Connection stateCallback, MQTT_Callback_Message messageCallback );
void   MQTT_Cleanup();
cJSON* MQTT_Settings();
int    MQTT_Publish( const char *topic, const char *payload, int qos, int retained );
int    MQTT_Publish_JSON( const char *topic, cJSON *payload, int qos, int retained );
int    MQTT_Publish_Binary( const char *topic, int payloadlen, void *payload, int qos, int retained );
int    MQTT_Subscribe( const char *topic );
int    MQTT_Unsubscribe( const char *topic );

#ifdef  __cplusplus
  }
#endif

#endif

