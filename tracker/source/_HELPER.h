/*------------------------------------------------------------------
 *  Copyright Fred Juhlin (2021)
 *  License:
 *  All rights reserved.  This code may not be used in commercial
 *  products without permission from Fred Juhlin.
 *------------------------------------------------------------------*/

#ifndef _HELPER_H_
#define _HELPER_H_

#ifdef __cplusplus
extern "C"
{
#endif

/*
	Split a string to array
	
	Usage:
	
	char   theStringToSolit[] = "The string to split
	char **stringArray;

	stringArray = str_split( theStringToSplit, '\n');
	if( stringArray ) {
		int i;
		for (i = 0; *(stringArray + i); i++) {
	        printf("%s\n",*(stringArray + i));
			free(*(stringArray + i));
		}
		free(stringArray);
	}
*/
char** helper_split( char* someString,  char token);

#ifdef __cplusplus
}
#endif

#endif
