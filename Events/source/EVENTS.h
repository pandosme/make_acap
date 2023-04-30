/*------------------------------------------------------------------
 *  Fred Juhlin (2023)
 *  A wrapper around events
 *  Events are declared in /html/config/events.json
 *------------------------------------------------------------------*/
 
#ifndef _EVENTS_H_
#define _EVENTS_H_

#include <stdio.h>
#include <stdlib.h>
#include <axsdk/axevent.h>
#include "cJSON.h"

#ifdef  __cplusplus
extern "C" {
#endif

typedef void (*EVENTS_Callback) (cJSON *event);


cJSON*	EVENTS();

//Declarations
int		EVENTS_Add_Event( const char* Id, const char* NiceName, int state );
int		EVENTS_Add_Event_JSON( cJSON* event );
int		EVENTS_Remove_Event( const char* Id );
int		EVENTS_Fire_State( const char* Id, int value );
int		EVENTS_Fire( const char* Id );
int		EVENTS_Fire_JSON( const char* Id, cJSON* data );


//Subscriptions
int		EVENTS_SetCallback( EVENTS_Callback callback );
int		EVENTS_Subscribe( cJSON* eventDeclaration );
/*
Subscription eventDeclaration
{
	"topic0": {"theNameSpace":"theTopicName"},
	"topic1": {"theNameSpace":"theTopicName"},
	....
}

*/
int		EVENTS_Unsubscribe();



#ifdef  __cplusplus
}
#endif

#endif
