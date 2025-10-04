/*------------------------------------------------------------------
 *  Copyright Fred Juhlin (2025)
 *------------------------------------------------------------------*/
 
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include "ACAP.h"

#define LOG(fmt, args...)    { syslog(LOG_INFO, fmt, ## args); printf(fmt, ## args); }
#define LOG_WARN(fmt, args...)    { syslog(LOG_WARNING, fmt, ## args); printf(fmt, ## args); }
//#define LOG_TRACE(fmt, args...)    { syslog(LOG_INFO, fmt, ## args); printf(fmt, ## args); }
#define LOG_TRACE(fmt, args...)    {}

cJSON *CERTS_SETTINGS = 0;
char CERTS_CA_STORE[] = "/etc/ssl/certs/ca-certificates.crt";
char CERTS_LOCAL_PATH[256];

char*
CERTS_Get_CA() {
	if( !CERTS_SETTINGS ) {
		LOG_WARN("CERTS: Not initialized");
		return CERTS_CA_STORE;
	}
	cJSON* path = cJSON_GetObjectItem(CERTS_SETTINGS,"cafile");
	if( !path ) {
		LOG_TRACE("%s: No CA File\n", __func__ );
		return CERTS_CA_STORE;
	}
	LOG_TRACE("%s: %s\n", __func__, path->valuestring);
	return path->valuestring;
}

char*
CERTS_Get_Cert() {
	if( !CERTS_SETTINGS ) {
		LOG_WARN("%s: CERTS is not initiailiaized\n",__func__);
		return 0;
	}
	cJSON* cert = cJSON_GetObjectItem(CERTS_SETTINGS,"certfile");
	cJSON* key = cJSON_GetObjectItem(CERTS_SETTINGS,"keyfile");
	if( !cert || !key ) {
		LOG_TRACE("%s: CERTS_CertFile: 0\n", __func__);
		return 0;
	}
	LOG_TRACE("%s: %s\n", __func__, cert->valuestring);
	return cert->valuestring;
}

char*
CERTS_Get_Key() {
	if( !CERTS_SETTINGS ) {
		LOG_WARN("%s: CERTS is not initiailiaized\n",__func__);
		return 0;
	}
	cJSON* cert = cJSON_GetObjectItem(CERTS_SETTINGS,"certfile");
	cJSON* key = cJSON_GetObjectItem(CERTS_SETTINGS,"keyfile");
	if( !cert || !key ) {
		LOG_TRACE("%s: 0\n",__func__);
		return 0;
	}
	LOG_TRACE("%s: %s\n",__func__, key->valuestring);
	return key->valuestring;
}

char*
CERTS_Get_Password() {
	if( !CERTS_SETTINGS ) {
		LOG_WARN("%s: CERTS is not initiailiaized\n", __func__);
		return 0;
	}
	cJSON* password = cJSON_GetObjectItem(CERTS_SETTINGS,"password");
	if( !password ) {
		LOG_TRACE("%s: 0\n", __func__);
		return 0;
	}
	LOG_TRACE("%s: %s\n",__func__, password->valuestring);
	return password->valuestring;
}

void
CERTS_HTTP (const  ACAP_HTTP_Response response,const  ACAP_HTTP_Request request) {
    LOG_TRACE("%s: Entry\n", __func__);

    // Check if service is initialized
    if (!CERTS_SETTINGS) {
        LOG_WARN("CERTS: Not initialized\n");
        ACAP_HTTP_Respond_Error(response, 400, "Certificate service is not initialized");
        return;
    }

    // Determine request method
    const char *method = ACAP_HTTP_Get_Method(request);
    const char *contentType = ACAP_HTTP_Get_Content_Type(request);

	if( !method || strcmp(method,"GET") == 0 ) {
		cJSON* copy = cJSON_Duplicate( CERTS_SETTINGS,1 );
		if( cJSON_GetObjectItem( copy,"password") ) {
			cJSON_ReplaceItemInObject( copy,"password",cJSON_CreateString("") );
		}
		 ACAP_HTTP_Respond_JSON(response, copy );
		 cJSON_Delete(copy);
		 return;
	}

    // If POST and JSON, parse request->postData
    cJSON *data = NULL;
    if (method && strcmp(method, "POST") == 0 && contentType &&
        strstr(contentType, "application/json")) {
        if (request->postData && request->postDataLength > 0) {
            // Parse POST body as JSON
            data = cJSON_Parse(request->postData);
            if (!data) {
                ACAP_HTTP_Respond_Error(response, 400, "JSON Parse error");
                return;
            }
        }
    } else {
        // For legacy GET with ?json=..., fall back to query string
        const char *jsonData = ACAP_HTTP_Request_Param(request, "json");
        if (jsonData) {
            data = cJSON_Parse(jsonData);
            if (!data) {
                ACAP_HTTP_Respond_Error(response, 400, "JSON Parse error");
                return;
            }
        }
    }

	if(!data) {
		ACAP_HTTP_Respond_Error( response, 400, "JSON Parse error" );
		return;
	}
		
	char* type = cJSON_GetObjectItem(data,"type")?cJSON_GetObjectItem(data,"type")->valuestring:0;
	char* pem = cJSON_GetObjectItem(data,"pem")?cJSON_GetObjectItem(data,"pem")->valuestring:0;
	char* password = cJSON_GetObjectItem(data,"password")?cJSON_GetObjectItem(data,"password")->valuestring:0;

	if(!type) {
		LOG_WARN("CERT: Type is missing\n");
		 ACAP_HTTP_Respond_Error( response, 400, "Missing type" );
		return;
	}
	
	if(!pem) {
		LOG_WARN("CERT: PEM is missing\n");
		 ACAP_HTTP_Respond_Error( response, 400, "Missing pem" );
		return;
	}

	if( !(strcmp(type,"ca")==0 || strcmp(type,"cert")==0 || strcmp(type,"key")==0) ) {
		LOG_WARN("CERTS: Invalid type %s\n", type);
		 ACAP_HTTP_Respond_Error( response, 400, "Missing type" );
		return;
	}

	if( strcmp(type,"ca") == 0 ) {
		if( strlen(pem) < 500 ) {
			if( !cJSON_GetObjectItem(CERTS_SETTINGS,"cafile") ) {
				 ACAP_HTTP_Respond_Text(response,"OK");
				return;
			}
			if(  ACAP_FILE_Delete( "localdata/ca.pem" ) ) {
				cJSON_DeleteItemFromObject(CERTS_SETTINGS,"cafile");
				 ACAP_STATUS_SetBool("certificate","CA",0);
				 ACAP_HTTP_Respond_Text(response,"OK");
			} else {
				LOG_WARN("Unable to remove CA file\n");
				 ACAP_HTTP_Respond_Error(response,500,"Failed to remove");
				return;
			}
		}
	}

	if( strcmp(type,"cert") == 0 ) {
		if( strlen(pem) < 500 ) {
			if( !cJSON_GetObjectItem(CERTS_SETTINGS,"certfile") ) {
				 ACAP_HTTP_Respond_Text(response,"OK");
				return;
			}
			if(  ACAP_FILE_Delete( "localdata/cert.pem" ) ) {
				cJSON_DeleteItemFromObject(CERTS_SETTINGS,"certfile");
				if(  ACAP_FILE_Delete( "localdata/key.pem" ) ) {
					cJSON_DeleteItemFromObject(CERTS_SETTINGS,"keyfile");
					if( cJSON_GetObjectItem(CERTS_SETTINGS,"password") ) {
						 ACAP_FILE_Delete( "localdata/ph.txt" );
						cJSON_DeleteItemFromObject(CERTS_SETTINGS,"password");
					}
				}				
				 ACAP_STATUS_SetBool("certificate","cert", 0);
				 ACAP_HTTP_Respond_Text(response,"OK");
			} else {
				LOG_WARN("Unable to remove certificate file\n");
				 ACAP_HTTP_Respond_Error(response,500,"Failed to remove");
				return;
			}
		}
	}

	if( strcmp(type,"key")==0 ) {
		if( strlen(pem) < 500 ) {
			if( !cJSON_GetObjectItem(CERTS_SETTINGS,"keyfile") ) {
				 ACAP_HTTP_Respond_Text(response,"OK");
				return;
			}
			if(  ACAP_FILE_Delete( "localdata/key.pem" ) ) {
				 ACAP_FILE_Delete( "localdata/ph.txt" );
				cJSON_DeleteItemFromObject(CERTS_SETTINGS,"keyfile");
				if( cJSON_GetObjectItem(CERTS_SETTINGS,"password" ) ) {
					cJSON_DeleteItemFromObject(CERTS_SETTINGS,"password");
					 ACAP_STATUS_SetBool("certificate","password",0);
				}
				 ACAP_STATUS_SetBool("certificate","key",0);
				 ACAP_HTTP_Respond_Text(response,"OK");
			} else {
				LOG_WARN("Unable to remove key file\n");
				 ACAP_HTTP_Respond_Error(response,500,"Failed to remove");
				return;
			}
		}
	}
	
	LOG_TRACE("%s: Updating %s\n",__func__,type);

	char filepath[128];
	sprintf(filepath,"localdata/%s.pem",type);
	LOG_TRACE("%s: Opening %s for writing\n",__func__,filepath);
	FILE* file =  ACAP_FILE_Open(filepath, "w" );
	if(!file) {
		LOG_WARN("%s: Cannot open %s for writing\n",__func__,filepath);
		 ACAP_HTTP_Respond_Error( response, 500, "Failed saving data");
		return;
	}
	size_t pemSize = strlen(pem);
	LOG_TRACE("%s: Saving data\n",__func__);
	size_t length = fwrite( pem, pemSize, 1, file );
	fclose(file);

	if( length < 1 ) {
		LOG_WARN("CERTS: Could not save cert data\n");
		 ACAP_HTTP_Respond_Error( response, 500, "Failed saving data" );
		return;
	} else {
		LOG_TRACE("%s: Data saveed in %s\n",__func__,filepath);
	}
	char fullpath[256]="";
	sprintf(fullpath,"%s%s", ACAP_FILE_AppPath(),filepath);
	LOG_TRACE("%s: %s",type,fullpath);
	if( strcmp(type,"cert") == 0 ) {
		if( cJSON_GetObjectItem(CERTS_SETTINGS,"certfile") )
			cJSON_ReplaceItemInObject(CERTS_SETTINGS,"certfile",cJSON_CreateString(fullpath));
		else
			cJSON_AddStringToObject(CERTS_SETTINGS,"certfile",fullpath);
		 ACAP_STATUS_SetBool("certificate","cert",1);
	}

	if( strcmp(type,"key") == 0 ) {
		if( cJSON_GetObjectItem(CERTS_SETTINGS,"keyfile") )
			cJSON_ReplaceItemInObject(CERTS_SETTINGS,"keyfile",cJSON_CreateString(fullpath));
		else
			cJSON_AddStringToObject(CERTS_SETTINGS,"keyfile",fullpath);
		
		if( cJSON_GetObjectItem(CERTS_SETTINGS,"password") )
			cJSON_DeleteItemFromObject(CERTS_SETTINGS,"password");
		if( password ) {
			LOG_TRACE("%s: Key with password\n",__func__);
			cJSON_AddStringToObject(CERTS_SETTINGS,"password",password);
			 ACAP_STATUS_SetBool("certificate","password",1);
			
			FILE* file =  ACAP_FILE_Open("localdata/ph.txt", "w" );
			if(file) {
				LOG_TRACE("%s: Saving password\n",__func__);
				size_t length = fwrite( password, sizeof(char), strlen(password), file );
				if( length < 1 )
					LOG_WARN("%s: Could not save password\n",__func__);
				fclose(file);
			}
		} else {
			 ACAP_STATUS_SetBool("certificate","password",0);
			LOG_TRACE("%s: No password set for key file\n",__func__);
		}
		 ACAP_STATUS_SetBool("certificate","key",1);
	}

	if( strcmp(type,"ca") == 0 ) {
		if( cJSON_GetObjectItem(CERTS_SETTINGS,"cafile") )
			cJSON_ReplaceItemInObject(CERTS_SETTINGS,"cafile",cJSON_CreateString(fullpath));
		else
			cJSON_AddStringToObject(CERTS_SETTINGS,"cafile",fullpath);
		 ACAP_STATUS_SetBool("certificate","ca",1);
	}
	 ACAP_HTTP_Respond_Text( response, "OK" );
}

int
CERTS_Init(){
	CERTS_SETTINGS = cJSON_CreateObject();

	char * buffer;
	size_t length;

	LOG_TRACE("%s: Entry\n", __func__);

	 ACAP_STATUS_SetBool("certificate","ca",0);
	 ACAP_STATUS_SetBool("certificate","cert",0);
	 ACAP_STATUS_SetBool("certificate","key",0);
	 ACAP_STATUS_SetBool("certificate","password",0);

	FILE* file =  ACAP_FILE_Open("localdata/cert.pem", "r" );
	if(file) {
		LOG_TRACE("%s: Loading cert.pem\n", __func__);
		fseek(file , 0 , SEEK_END);
		long lSize = ftell(file);
		if( lSize < 9000 ) {
			buffer = (char*) malloc (sizeof(char)*lSize);
			if(!buffer) {
				fclose(file);
				LOG_WARN("CERT:  Memory allocation error\n");
				return 0;
			}	
			rewind(file);
			length = fread ( buffer, sizeof(char), lSize, file );
			if( length > 50 ) {
				if( strstr( buffer, "-----BEGIN CERTIFICATE-----") > 0 ) {
					char filepath[256];
					sprintf(filepath,"%s%s", ACAP_FILE_AppPath(),"localdata/cert.pem");
					cJSON_AddStringToObject(CERTS_SETTINGS, "certfile", filepath );
					LOG_TRACE("%s: Client certificate loaded\n", __func__);
					 ACAP_STATUS_SetBool("certificate","cert",0);
				} else {
					LOG_WARN("Could not load certificate file.  File corrupt or empty\n");
				}
			} else {
				LOG_WARN("Could not load certificate file.  File corrupt or empty\n");
			}
			free(buffer);
		} else {
			LOG_WARN("Could not load certificate file.  File corrupt or empty\n");
		}
		fclose(file);
	} else {
		LOG_TRACE("%s: No client certificate loaded\n",__func__);
	}

	file =  ACAP_FILE_Open("localdata/key.pem", "r" );
	if(file) {
		LOG_TRACE("%s:Loading key.pem\n",__func__);
		fseek(file , 0 , SEEK_END);
		long lSize = ftell(file);
		if( lSize < 9000 ) {
			buffer = (char*) malloc (sizeof(char)*lSize);
			if(!buffer) {
				fclose(file);
				LOG_WARN("CERT:  Memory allocation error\n");
				return 0;
			}	
			rewind(file);
			length = fread ( buffer, sizeof(char), lSize, file );
			if( length > 50 ) {
				char filepath[256];
				sprintf(filepath,"%s%s", ACAP_FILE_AppPath(),"localdata/key.pem");
				cJSON_AddStringToObject(CERTS_SETTINGS, "keyfile", filepath );
				LOG_TRACE("%s: Private certificate key loaded", __func__);
				ACAP_STATUS_SetBool("certificate","key",1);
			} else {
				LOG_WARN("Could not load key file.  File corrupt or empty\n");
			}
			free(buffer);
		} else {
			LOG_WARN("Could not load key file.  File corrupt or empty\n");
		}
		fclose(file);
	} else {
		LOG_TRACE("%s: No client certificate private loaded\n", __func__);
	}

	file =  ACAP_FILE_Open("localdata/ca.pem", "r" );
	if(file) {
		LOG_TRACE("%s: Loading ca.pem\n", __func__);
		fseek(file , 0 , SEEK_END);
		long lSize = ftell(file);
		if( lSize < 9000 ) {
			buffer = (char*) malloc (sizeof(char)*lSize);
			if(!buffer) {
				fclose(file);
				LOG_WARN("Could not load CA file.  Memory allocation\n");
				return 0;
			}	
			rewind(file);
			length = fread ( buffer, sizeof(char), lSize, file );
			if( length > 50 ) {
				if( strstr( buffer, "-----BEGIN CERTIFICATE-----") > 0 ) {
					char filepath[256];
					sprintf(filepath,"%s%s", ACAP_FILE_AppPath(),"localdata/ca.pem");
					cJSON_AddStringToObject(CERTS_SETTINGS, "cafile", filepath );
					LOG_TRACE("%s: CA Certificate loaded", __func__);
					 ACAP_STATUS_SetBool("certificate","ca",1);
				} else {
					LOG_WARN("Could not load CA file.  Invalid or corrupt\n");
				}
			} else {
				LOG_WARN("Could not load CA file.  Invalid or corrupt\n");
			}
			free(buffer);
		} else {
			LOG_WARN("Could not load CA file.  Invalid or corrupt\n");
		}
		fclose(file);
	} else {
		LOG_TRACE("%s: CA file not avaialable\n", __func__);
	}

	 ACAP_STATUS_SetBool("certificate","password",0);
	file =  ACAP_FILE_Open("localdata/ph.txt", "r" );
	if(file) {
		fseek(file , 0 , SEEK_END);
		long lSize = ftell(file);
		if( lSize < 100 ) {
			buffer = (char*) malloc (sizeof(char)*lSize);
			if(!buffer) {
				fclose(file);
				LOG_WARN("CERT:  Memory allocation error\n");
				return 0;
			}	
			rewind(file);
			length = fread ( buffer, sizeof(char), lSize, file );
			if( length > 3 ) {
				cJSON_AddStringToObject(CERTS_SETTINGS, "password", buffer );
				 ACAP_STATUS_SetBool("certificate","password",0);
			} else {
				LOG_WARN("CERT: Invalid or currupt file\n");
			}
			free(buffer);
		}
		fclose(file);
	}
	LOG_TRACE("%s: setting http node\n",__func__);
	 ACAP_HTTP_Node( "certs", CERTS_HTTP );
	return 1;
}