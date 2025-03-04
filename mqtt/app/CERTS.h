/*------------------------------------------------------------------
 *  Copyright Fred Juhlin (2020)
 * 
 * 
 *  Manage device CA Cert, Client cert and client key
 *  for service such as CURL and MQTT
 * 
 * 
 *  API
 *  GET certs
 *	Info: Responds JSON with certificate paths.
 *  { 
 *    "certfile": string   //If set
 *    "keyfile": string    //If set
 *    "cafile": string     //If set
 *    "password: ""        //If set it will always respond with empty string
 *  }
 *
 *  GET cert?type[ca|cert|key]&pem=[string]&password=*****
 *  Info: Sets CA, Certificate or password PEM (w/o) password
 * 
 *------------------------------------------------------------------*/
 
#ifndef _CERTS_H_
#define _CERTS_H_

#include "cJSON.h"

#ifdef  __cplusplus
extern "C" {
#endif

int    	CERTS_Init();
char*	CERTS_Get_CA();  //The filepath to device CA store or the local CA if loaded
char*  	CERTS_Get_Cert(); //The filepath to client certificate if loaded, else 0
char*  	CERTS_Get_Key(); //The filepath to client private key if loaded, else 0
char*  	CERTS_Get_Password(); //For private key, 0 if no exists;

#ifdef  __cplusplus
}
#endif

#endif