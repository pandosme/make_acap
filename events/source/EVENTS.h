/*------------------------------------------------------------------
 *  Fred Juhlin (2022)
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


int		EVENTS_Init(const char* acapID, const char* acapNiceName);

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
