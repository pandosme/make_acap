#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <syslog.h>
#include "CERTS.h"
#include "ACAP.h"

#define LOG(fmt, args...)    { syslog(LOG_INFO, fmt, ## args);  printf(fmt, ## args); }
#define LOG_WARN(fmt, args...)    { syslog(LOG_WARNING, fmt, ## args); printf(fmt, ## args);}
#define LOG_TRACE(fmt, args...)    { syslog(LOG_INFO, fmt, ## args); printf(fmt, ## args); }

static cJSON* CERTS_SETTINGS = NULL;
static const char CERTS_CA_STORE[] = "/etc/ssl/certs/ca-certificates.crt";

// Implementation of certificate validation functions
CERTS_Status CERTS_Validate_CA(const char* pem) {
    if (!pem) return CERTS_ERROR_INVALID_PARAM;
    return strstr(pem, "-----BEGIN CERTIFICATE-----") != NULL ? 
           CERTS_SUCCESS : CERTS_ERROR_INVALID_PEM;
}

CERTS_Status CERTS_Validate_Cert(const char* pem) {
    if (!pem) return CERTS_ERROR_INVALID_PARAM;
    return strstr(pem, "-----BEGIN CERTIFICATE-----") != NULL ? 
           CERTS_SUCCESS : CERTS_ERROR_INVALID_PEM;
}

CERTS_Status CERTS_Validate_Key(const char* pem) {
    if (!pem) return CERTS_ERROR_INVALID_PARAM;
    return strstr(pem, "-----BEGIN RSA PRIVATE KEY-----") != NULL ? 
           CERTS_SUCCESS : CERTS_ERROR_INVALID_PEM;
}

const char* CERTS_Get_CA(void) {
    if (!CERTS_SETTINGS) return CERTS_CA_STORE;
    cJSON* cafile = cJSON_GetObjectItem(CERTS_SETTINGS, "cafile");
    return cafile ? cafile->valuestring : CERTS_CA_STORE;
}

const char* CERTS_Get_Cert(void) {
    if (!CERTS_SETTINGS) return NULL;
    cJSON* certfile = cJSON_GetObjectItem(CERTS_SETTINGS, "certfile");
    cJSON* keyfile = cJSON_GetObjectItem(CERTS_SETTINGS, "keyfile");
    return (certfile && keyfile) ? certfile->valuestring : NULL;
}

const char* CERTS_Get_Key(void) {
    if (!CERTS_SETTINGS) return NULL;
    cJSON* certfile = cJSON_GetObjectItem(CERTS_SETTINGS, "certfile");
    cJSON* keyfile = cJSON_GetObjectItem(CERTS_SETTINGS, "keyfile");
    return (certfile && keyfile) ? keyfile->valuestring : NULL;
}

const char* CERTS_Get_Password(void) {
    if (!CERTS_SETTINGS) return NULL;
    cJSON* password = cJSON_GetObjectItem(CERTS_SETTINGS, "password");
    return password ? password->valuestring : NULL;
}

static int save_certificate(const char* type, const char* pem) {
    char filepath[CERTS_MAX_PATH_LENGTH];
    snprintf(filepath, sizeof(filepath), "localdata/%s.pem", type);
    
    FILE* file = ACAP_FILE_Open(filepath, "w");
    if (!file) {
        LOG_WARN("CERTS: Cannot open %s for writing\n", filepath);
        return 0;
    }

    size_t pemSize = strlen(pem);
    size_t written = fwrite((void*)pem, 1, pemSize, file);
    fclose(file);

    if (written != pemSize) {
        LOG_WARN("CERTS: Could not save cert data\n");
        return 0;
    }

    return 1;
}

void CERTS_HTTP_Callback(const ACAP_HTTP_Response response, const ACAP_HTTP_Request request) {
    if (!CERTS_SETTINGS) {
        LOG_WARN("CERTS is not initialized\n");
        ACAP_HTTP_Respond_Error(response, 400, "Certificate service is not initialized");
        return;
    }

    const char* method = request->method;
    if (!method) {
        ACAP_HTTP_Respond_Error(response, 400, "Invalid request method");
        return;
    }

    // Handle GET request
    if (strcmp(method, "GET") == 0) {
        LOG_TRACE("CERTS_HTTP: Respond with data\n");
        cJSON* copy = cJSON_Duplicate(CERTS_SETTINGS, 1);
        if (cJSON_GetObjectItem(copy, "password")) {
            cJSON_ReplaceItemInObject(copy, "password", cJSON_CreateString(""));
        }
        ACAP_HTTP_Respond_JSON(response, copy);
        cJSON_Delete(copy);
        return;
    }

    // Handle DELETE request
    if (strcmp(method, "DELETE") == 0) {
        const char* type = ACAP_HTTP_Request_Param(request, "type");
        if (!type) {
            ACAP_HTTP_Respond_Error(response, 400, "Missing type parameter");
            return;
        }

        if (strcmp(type, "ca") == 0) {
            if (!cJSON_GetObjectItem(CERTS_SETTINGS, "cafile")) {
                ACAP_HTTP_Respond_Text(response, "OK");
                return;
            }
            if (ACAP_FILE_Delete("localdata/ca.pem")) {
                cJSON_DeleteItemFromObject(CERTS_SETTINGS, "cafile");
                ACAP_STATUS_SetBool("certificate", "ca", 0);
                ACAP_HTTP_Respond_Text(response, "OK");
            } else {
                LOG_WARN("Unable to remove CA file\n");
                ACAP_HTTP_Respond_Error(response, 500, "Failed to remove");
            }
            return;
        }

        if (strcmp(type, "cert") == 0) {
            if (!cJSON_GetObjectItem(CERTS_SETTINGS, "certfile")) {
                ACAP_HTTP_Respond_Text(response, "OK");
                return;
            }
            if (ACAP_FILE_Delete("localdata/cert.pem")) {
                cJSON_DeleteItemFromObject(CERTS_SETTINGS, "certfile");
                if (ACAP_FILE_Delete("localdata/key.pem")) {
                    cJSON_DeleteItemFromObject(CERTS_SETTINGS, "keyfile");
                    if (cJSON_GetObjectItem(CERTS_SETTINGS, "password")) {
                        ACAP_FILE_Delete("localdata/ph.txt");
                        cJSON_DeleteItemFromObject(CERTS_SETTINGS, "password");
                    }
                }
                ACAP_STATUS_SetBool("certificate", "cert", 0);
                ACAP_HTTP_Respond_Text(response, "OK");
            } else {
                LOG_WARN("Unable to remove certificate file\n");
                ACAP_HTTP_Respond_Error(response, 500, "Failed to remove");
            }
            return;
        }

        if (strcmp(type, "key") == 0) {
            if (!cJSON_GetObjectItem(CERTS_SETTINGS, "keyfile")) {
                ACAP_HTTP_Respond_Text(response, "OK");
                return;
            }
            if (ACAP_FILE_Delete("localdata/key.pem")) {
                ACAP_FILE_Delete("localdata/ph.txt");
                cJSON_DeleteItemFromObject(CERTS_SETTINGS, "keyfile");
                if (cJSON_GetObjectItem(CERTS_SETTINGS, "password")) {
                    cJSON_DeleteItemFromObject(CERTS_SETTINGS, "password");
                    ACAP_STATUS_SetBool("certificate", "password", 0);
                }
                ACAP_STATUS_SetBool("certificate", "key", 0);
                ACAP_HTTP_Respond_Text(response, "OK");
            } else {
                LOG_WARN("Unable to remove key file\n");
                ACAP_HTTP_Respond_Error(response, 500, "Failed to remove");
            }
            return;
        }

        ACAP_HTTP_Respond_Error(response, 400, "Invalid certificate type");
        return;
    }

    // Handle POST request
    if (strcmp(method, "POST") != 0) {
        ACAP_HTTP_Respond_Error(response, 405, "Method Not Allowed - Use GET, POST, or DELETE");
        return;
    }

    // Parse POST data as JSON
    if (!request->postData || request->postDataLength == 0) {
        ACAP_HTTP_Respond_Error(response, 400, "Missing POST data");
        return;
    }

    cJSON* postJson = cJSON_Parse(request->postData);
    if (!postJson) {
        ACAP_HTTP_Respond_Error(response, 400, "Invalid JSON data");
        return;
    }

    // Get required fields
    cJSON* typeJson = cJSON_GetObjectItem(postJson, "type");
    cJSON* pemJson = cJSON_GetObjectItem(postJson, "pem");
    
    if (!typeJson || !pemJson || !typeJson->valuestring || !pemJson->valuestring) {
        ACAP_HTTP_Respond_Error(response, 400, "Missing required fields (type, pem)");
        cJSON_Delete(postJson);
        return;
    }

    const char* type = typeJson->valuestring;
    const char* pem = pemJson->valuestring;

    // Validate certificate type
    if (!(strcmp(type, "ca") == 0 || strcmp(type, "cert") == 0 || strcmp(type, "key") == 0)) {
        ACAP_HTTP_Respond_Error(response, 400, "Invalid certificate type");
        cJSON_Delete(postJson);
        return;
    }

    // Validate PEM data
    if ((strcmp(type, "ca") == 0 || strcmp(type, "cert") == 0) && 
        !strstr(pem, "-----BEGIN CERTIFICATE-----")) {
        ACAP_HTTP_Respond_Error(response, 400, "Invalid certificate data");
        cJSON_Delete(postJson);
        return;
    }

    if (strcmp(type, "key") == 0 && !strstr(pem, "-----BEGIN RSA PRIVATE KEY-----")) {
        ACAP_HTTP_Respond_Error(response, 400, "Invalid key data");
        cJSON_Delete(postJson);
        return;
    }

    // Save certificate
    if (!save_certificate(type, pem)) {
        ACAP_HTTP_Respond_Error(response, 500, "Failed to save certificate");
        cJSON_Delete(postJson);
        return;
    }

    // Update settings
    char fullpath[CERTS_MAX_PATH_LENGTH];
    snprintf(fullpath, sizeof(fullpath), "%s%s", ACAP_FILE_AppPath(), 
             type == "ca" ? "localdata/ca.pem" :
             type == "cert" ? "localdata/cert.pem" : "localdata/key.pem");

    if (strcmp(type, "ca") == 0) {
        if (cJSON_GetObjectItem(CERTS_SETTINGS, "cafile")) {
            cJSON_ReplaceItemInObject(CERTS_SETTINGS, "cafile", cJSON_CreateString(fullpath));
        } else {
            cJSON_AddStringToObject(CERTS_SETTINGS, "cafile", fullpath);
        }
        ACAP_STATUS_SetBool("certificate", "ca", 1);
    } else if (strcmp(type, "cert") == 0) {
        if (cJSON_GetObjectItem(CERTS_SETTINGS, "certfile")) {
            cJSON_ReplaceItemInObject(CERTS_SETTINGS, "certfile", cJSON_CreateString(fullpath));
        } else {
            cJSON_AddStringToObject(CERTS_SETTINGS, "certfile", fullpath);
        }
        ACAP_STATUS_SetBool("certificate", "cert", 1);
    } else if (strcmp(type, "key") == 0) {
        if (cJSON_GetObjectItem(CERTS_SETTINGS, "keyfile")) {
            cJSON_ReplaceItemInObject(CERTS_SETTINGS, "keyfile", cJSON_CreateString(fullpath));
        } else {
            cJSON_AddStringToObject(CERTS_SETTINGS, "keyfile", fullpath);
        }
        
        // Handle password
        if (cJSON_GetObjectItem(CERTS_SETTINGS, "password")) {
            cJSON_DeleteItemFromObject(CERTS_SETTINGS, "password");
        }
        
        cJSON* passwordJson = cJSON_GetObjectItem(postJson, "password");
        if (passwordJson && passwordJson->valuestring) {
            const char* password = passwordJson->valuestring;
            cJSON_AddStringToObject(CERTS_SETTINGS, "password", password);
            ACAP_STATUS_SetBool("certificate", "password", 1);
            
            FILE* pwfile = ACAP_FILE_Open("localdata/ph.txt", "w");
            if (pwfile) {
                if (fwrite((void*)password, 1, strlen(password), pwfile) != strlen(password)) {
                    LOG_WARN("Could not save password\n");
                }
                fclose(pwfile);
            }
        } else {
            ACAP_STATUS_SetBool("certificate", "password", 0);
        }
        ACAP_STATUS_SetBool("certificate", "key", 1);
    }

    cJSON_Delete(postJson);
    ACAP_HTTP_Respond_Text(response, "OK");
}

CERTS_Status CERTS_Init(void) {
	
    if (CERTS_SETTINGS) return CERTS_SUCCESS;
    
    CERTS_SETTINGS = cJSON_CreateObject();
    if (!CERTS_SETTINGS) return CERTS_ERROR_MEMORY;

	char * buffer;
	size_t length;

    // Initialize status
    ACAP_STATUS_SetBool("certificate", "ca", 0);
    ACAP_STATUS_SetBool("certificate", "cert", 0);
    ACAP_STATUS_SetBool("certificate", "key", 0);
    ACAP_STATUS_SetBool("certificate", "password", 0);

	FILE* file = ACAP_FILE_Open("localdata/cert.pem", "r" );
	if(file) {
		LOG_TRACE("Loading cert.pem\n");
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
					sprintf(filepath,"%s%s",ACAP_FILE_AppPath(),"localdata/cert.pem");
					cJSON_AddStringToObject(CERTS_SETTINGS, "certfile", filepath );
					LOG_TRACE("Client certificate loaded\n");
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
		LOG_TRACE("No client certificate loaded\n");
	}

	file = ACAP_FILE_Open("localdata/key.pem", "r" );
	if(file) {
		LOG_TRACE("Loading key.pem\n");
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
				if( strstr( buffer, "-----BEGIN RSA PRIVATE KEY-----") > 0 ) {
					char filepath[256];
					sprintf(filepath,"%s%s",ACAP_FILE_AppPath(),"localdata/key.pem");
					cJSON_AddStringToObject(CERTS_SETTINGS, "keyfile", filepath );
					LOG_TRACE("Private certificate key loaded");
					ACAP_STATUS_SetBool("certificate","key",1);
				} else {
					LOG_WARN("Could not load key file.  PEM file is corrupt\n");
				}
			} else {
				LOG_WARN("Could not load key file.  File corrupt or empty\n");
			}
			free(buffer);
		} else {
			LOG_WARN("Could not load key file.  File corrupt or empty\n");
		}
		fclose(file);
	} else {
		LOG_TRACE("No client certificate private loaded\n");
	}

	file = ACAP_FILE_Open("localdata/ca.pem", "r" );
	if(file) {
		LOG_TRACE("Loading ca.pem\n");
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
					sprintf(filepath,"%s%s",ACAP_FILE_AppPath(),"localdata/ca.pem");
					cJSON_AddStringToObject(CERTS_SETTINGS, "cafile", filepath );
					LOG_TRACE("CA Certificate loaded");
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
		LOG_TRACE("CA file not avaialable\n");
	}

	ACAP_STATUS_SetBool("certificate","password",0);
	file = ACAP_FILE_Open("localdata/ph.txt", "r" );
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

    // Register HTTP endpoint
    if (!ACAP_HTTP_Node("certs", CERTS_HTTP_Callback)) {
        cJSON_Delete(CERTS_SETTINGS);
        CERTS_SETTINGS = NULL;
        return CERTS_ERROR_INIT;
    }

    return CERTS_SUCCESS;
}

void CERTS_Cleanup(void) {
    if (CERTS_SETTINGS) {
        cJSON_Delete(CERTS_SETTINGS);
        CERTS_SETTINGS = NULL;
    }
}
