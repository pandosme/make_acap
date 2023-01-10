/*------------------------------------------------------------------
 *  Fred Juhlin (2023)
 *------------------------------------------------------------------*/
 
#ifndef CLASSIFICATION_H
#define CLASSIFICATION_H

#include "cJSON.h"

typedef void (*CLASSIFICATION_ObjectCallback)( double timestamp, cJSON *list  );
//Consumer needs delete list;

int CLASSIFICATION_Init( CLASSIFICATION_ObjectCallback callback );
cJSON* CLASSIFICATION_Settings();

#endif
