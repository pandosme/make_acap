/*------------------------------------------------------------------
 *  Fred Juhlin (2022)
 *  MQTT Service that uses SIMQTT as MQTT Publish proxy
 *------------------------------------------------------------------*/
 
#ifndef _MQTTD_H_
#define _MQTTD_H_

#include "cJSON.h"

#ifdef  __cplusplus
extern "C" {
#endif

#define	MQTTD_OK		1  	//Message published
#define	MQTTD_ERROR		0   //Undefined Error
#define	MQTTD_NO_SIMQTT	-1  //SIMQTT is not installed or not running
#define	MQTTD_BROKER	-2  //SIMQTT broker not configured
#define	MQTTD_INVALID	-3  //Invalid message or payload

int MQTTD_Publish(const char* topic, const char* payload, int retained);
int MQTTD_Publish_JSON(const char* topic, cJSON* payload, int retained);

#ifdef  __cplusplus
}
#endif

#endif
