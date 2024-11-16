/*------------------------------------------------------------------
 * Copyright Fred Juhlin (2024)
 *
 * Certificate Management Module
 * Manages CA, client certificates and private keys for CURL and MQTT services
 *
 * API Endpoints:
 * GET /certs
 *   Returns current certificate configuration
 * 
 * POST /certs
 *   Parameters:
 *   - type: "ca", "cert", or "key"
 *   - pem: Certificate/key data in PEM format
 *   - password: (optional) Password for private key
 *------------------------------------------------------------------*/

#ifndef _CERTS_H_
#define _CERTS_H_

#include "cJSON.h"

#ifdef __cplusplus
extern "C" {
#endif

// Constants
#define CERTS_MAX_PEM_SIZE 8192
#define CERTS_MIN_PEM_SIZE 500
#define CERTS_MAX_PATH_LENGTH 256
#define CERTS_MAX_PASSWORD_LENGTH 128

// Return types
typedef enum {
    CERTS_SUCCESS = 0,
    CERTS_ERROR_INIT = -1,
    CERTS_ERROR_INVALID_PARAM = -2,
    CERTS_ERROR_FILE_IO = -3,
    CERTS_ERROR_MEMORY = -4,
    CERTS_ERROR_INVALID_PEM = -5
} CERTS_Status;

// Initialize certificate management
CERTS_Status CERTS_Init(void);

// Certificate path getters
const char* CERTS_Get_CA(void);      // Returns path to CA store or local CA
const char* CERTS_Get_Cert(void);    // Returns path to client certificate
const char* CERTS_Get_Key(void);     // Returns path to client private key
const char* CERTS_Get_Password(void); // Returns private key password if exists

// Certificate validation
CERTS_Status CERTS_Validate_CA(const char* pem);
CERTS_Status CERTS_Validate_Cert(const char* pem);
CERTS_Status CERTS_Validate_Key(const char* pem);

// Cleanup
void CERTS_Cleanup(void);

#ifdef __cplusplus
}
#endif

#endif // _CERTS_H_
