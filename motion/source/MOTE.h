/*------------------------------------------------------------------
 *  (C) Fred Juhlin (2019)
 *------------------------------------------------------------------*/
 
#ifndef MOTE_H
#define MOTE_H

#include "cJSON.h"

typedef void (*MOTE_Callback)( double timestamp, cJSON* list);

int MOTE_Init();  
int MOTE_Close();  
int MOTE_Subscribe( MOTE_Callback callback );  
cJSON* MOTE_Settings();

#endif
