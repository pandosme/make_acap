/*------------------------------------------------------------------
 *  Fred Juhlin 2024
 *------------------------------------------------------------------*/

#ifndef _MQTT_Service_H_
#define _MQTT_Service_H_

#include "cJSON.h"

#ifdef  __cplusplus
extern "C" {
#endif

#define MQTT_CONNECT  1
#define MQTT_DISCONNECT  2
#define MQTT_RECONNECT  3
#define MQTT_MESSAGE  4
#define MQTT_PUBLISH  5

typedef void (*MQTT_Callback_Connection) (int state);
typedef void (*MQTT_Callback_Message) (const char *topic, const char *payload);

int    MQTT_Init( const char* acapname,MQTT_Callback_Connection callback );
void   MQTT_Cleanup();
int    MQTT_Connect();
int    MQTT_isConnected();
int    MQTT_Disconnect();
int    MQTT_Publish( const char *topic, const char *payload, int qos, int retained );
int    MQTT_Publish_JSON( const char *topic, cJSON *payload, int qos, int retained );
int    MQTT_Publish_Binary( const char *topic, int payloadlen, void *payload, int qos, int retained );
int    MQTT_Subscribe( const char *topic, MQTT_Callback_Message callback );
int    MQTT_Unsubscribe( const char *topic );
int    MQTT_Set( cJSON* settings);
cJSON* MQTT_Settings();

#ifdef  __cplusplus
  }
#endif

#endif

