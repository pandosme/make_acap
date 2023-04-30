/*------------------------------------------------------------------
 *  Fred Juhlin (2023)
 *  Access Axis Camera Motion analytics engine
 *	Detection comes in a list	
 *	[
 *		{
 *			"id": Unique object ID,
 *			"x": 0-1000,
 *			"y": 0-1000,
 *			"w": 0-1000,
 *			"h": 0-1000,
 *			"cx": 0-1000,  Center of gravity.  Placement depends on configuration
 *			"cy": 0-1000   Either in the center of the box or bottom-middle (default)
 *		},
 *		...
 *	]
*/

 
#ifndef MotionDetection_H
#define MotionDetection_H

#include "cJSON.h"

typedef void (*MotionDetection_Callback)( double timestamp, cJSON* list);

cJSON* MotionDetection( MotionDetection_Callback callback );  

#endif
