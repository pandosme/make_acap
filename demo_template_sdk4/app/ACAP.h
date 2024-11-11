/*------------------------------------------------------------------
 *  Fred Juhlin (2024)
 *------------------------------------------------------------------*/
 
#ifndef _ACAP_H_
#define _ACAP_H_

#include "fcgi_stdio.h"
#include "cJSON.h"

#ifdef  __cplusplus
extern "C" {
#endif

/*-----------------------------------------------------
	ACAP
  -----------------------------------------------------*/

typedef void (*ACAP_Config_Update) ( const char *service, cJSON* data);

cJSON*		ACAP( const char *package, ACAP_Config_Update updateCallback );
const char* ACAP_Package();
const char* ACAP_Name();
cJSON*  	ACAP_Service(const char* service);
int			ACAP_Register(const char* service, cJSON* serviceSettings );

/*-----------------------------------------------------
	EVENTS
  -----------------------------------------------------*/

typedef void (*ACAP_EVENTS_Callback) (cJSON *event);

cJSON*	ACAP_EVENTS();

//Declarations
int		ACAP_EVENTS_Add_Event( const char* Id, const char* NiceName, int state );
int		ACAP_EVENTS_Add_Event_JSON( cJSON* event );
int		ACAP_EVENTS_Remove_Event( const char* Id );
int		ACAP_EVENTS_Fire_State( const char* Id, int value );
int		ACAP_EVENTS_Fire( const char* Id );
int		ACAP_EVENTS_Fire_JSON( const char* Id, cJSON* data );

//Subscriptions
int		ACAP_EVENTS_SetCallback( ACAP_EVENTS_Callback callback );
int		ACAP_EVENTS_Subscribe( cJSON* eventDeclaration );


/*-----------------------------------------------------
	FILE
  -----------------------------------------------------*/

int    		ACAP_FILE_Init();
const char *ACAP_FILE_AppPath();
FILE*  		ACAP_FILE_Open( const char *filepath, const char *mode );
int    		ACAP_FILE_Delete( const char *filepath );
cJSON* 		ACAP_FILE_Read( const char *filepath );
int    		ACAP_FILE_Write( const char *filepath,  cJSON* object );
int			ACAP_FILE_WriteData( const char *filepath, const char *data );
int    		ACAP_FILE_exists( const char *filepath );


/*-----------------------------------------------------
	HTTP
-------------------------------------------------------*/

typedef FCGX_Request ACAP_HTTP_Request;
typedef FCGX_Request* ACAP_HTTP_Response;

typedef void (*ACAP_HTTP_Callback) (ACAP_HTTP_Response response,const ACAP_HTTP_Request request);

void        ACAP_HTTP();
int         ACAP_HTTP_Process();
void        ACAP_HTTP_Close();
int         ACAP_HTTP_Node( const char *nodename, ACAP_HTTP_Callback callback );
const char *ACAP_HTTP_Request_Param( const ACAP_HTTP_Request request, const char *param);
cJSON      *ACAP_HTTP_Request_JSON( const ACAP_HTTP_Request request, const char *param ); //JSON Object allocated.  Don't forget cJSON_Delet(object)
int         ACAP_HTTP_Header_XML( ACAP_HTTP_Response response );
int         ACAP_HTTP_Header_JSON( ACAP_HTTP_Response response ); 
int         ACAP_HTTP_Header_TEXT( ACAP_HTTP_Response response );
int         ACAP_HTTP_Header_FILE( ACAP_HTTP_Response response, const char *filename, const char *contenttype, unsigned filelength );
int         ACAP_HTTP_Respond_String( ACAP_HTTP_Response response,const char *fmt, ...);
int         ACAP_HTTP_Respond_JSON(  ACAP_HTTP_Response response, cJSON *object);
int         ACAP_HTTP_Respond_Data( ACAP_HTTP_Response response, size_t count, void *data );
int         ACAP_HTTP_Respond_Error( ACAP_HTTP_Response response, int code, const char *message );
int         ACAP_HTTP_Respond_Text( ACAP_HTTP_Response response, const char *message );

/*-----------------------------------------------------
	DEVICE
  -----------------------------------------------------*/

cJSON*		  ACAP_DEVICE();
const char   *ACAP_DEVICE_Prop( const char *name );
int           ACAP_DEVICE_Prop_Int( const char *name );
cJSON        *ACAP_DEVICE_JSON( const char *name );
int			  ACAP_DEVICE_Seconds_Since_Midnight();
double        ACAP_DEVICE_Timestamp();  //UTC in milliseconds
const char*   ACAP_DEVICE_Local_Time(); //YYYY-MM-DD HH:MM:SS
const char*   ACAP_DEVICE_ISOTime(); //YYYY-MM-DDTHH:MM:SS+0000
const char*   ACAP_DEVICE_Date(); //YYYY-MM-DD
const char*   ACAP_DEVICE_Time(); //HH:MM:SS
double		  ACAP_DEVICE_Uptime();
double		  ACAP_DEVICE_CPU_Average();
double		  ACAP_DEVICE_Network_Average();


/*-----------------------------------------------------
	STATUS
-------------------------------------------------------*/

cJSON*       ACAP_STATUS();  //Initialize
cJSON*       ACAP_STATUS_Group( const char *name ); //Get a status group
int          ACAP_STATUS_Bool( const char *group, const char *name );
int          ACAP_STATUS_Int( const char *group, const char *name );
double       ACAP_STATUS_Double( const char *group, const char *name );
char*        ACAP_STATUS_String( const char *group, const char *name );
cJSON*       ACAP_STATUS_Object( const char *group, const char *name );

//Set Data
void ACAP_STATUS_SetBool( const char *group, const char *name, int state );
void ACAP_STATUS_SetNumber(  const char *group, const char *name, double value );
void ACAP_STATUS_SetString(  const char *group, const char *name, const char *string );
void ACAP_STATUS_SetObject(  const char *group, const char *name, cJSON* data );
void ACAP_STATUS_SetNull( const char *group, const char *name );


#ifdef  __cplusplus
}
#endif

#endif
