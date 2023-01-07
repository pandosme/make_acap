/*------------------------------------------------------------------
 *  (C) Fred Juhlin (2019)
 *------------------------------------------------------------------*/
 
#ifndef MOTE_H
#define MOTE_H

#include "cJSON.h"

typedef void (*MOTE_Callback)( double timestamp, cJSON* list);

cJSON* MOTE();
int MOTE_Settings( cJSON *settings );
int MOTE_Init();  
int MOTE_Close();  
int MOTE_Subscribe( MOTE_Callback callback );  

#endif
