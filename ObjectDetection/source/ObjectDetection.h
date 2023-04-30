/*------------------------------------------------------------------
 * Fred Juhlin (2023)
 *	
 * ObjectDetection callback data
 * [
 *		{
 *			"id": number,		UID
 *			"x": number,		0-1000
 *			"y": number,		0-1000
 *			"w": number,		0-1000
 *			"h": number,		0-1000
 *			"confidenc": number,0-
 *			"classID":number,	0-8
 *			"class":string,		Human, Car, Truck, Bus, Motorcycle, Vehicle, Face,Licese plate, Other
 *			"type":string		Person, Vehicle, Other
 *		},
 *		...
 *	]
 *------------------------------------------------------------------*/
 
#ifndef ObjectDetection_H
#define ObjectDetection_H

#include "cJSON.h"

typedef void (*ObjectDetection_Callback)( double timestamp, cJSON *list  );
//Consumer needs delete list;

cJSON* ObjectDetection( ObjectDetection_Callback callback );

#endif
