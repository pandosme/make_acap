#ifndef IMGUTIL_H
#define IMGUTIL_H

#include <stddef.h>
#include "ACAP.h"

/*
 * Save a JPEG buffer to a file.
 * Returns 1 on success, 0 on failure.
 */
int imgutil_save_jpeg(const char* filepath, unsigned char* data, unsigned int size);

/*
 * Stream a STORE-only ZIP archive to an HTTP response.
 * base_dir: directory containing the files
 * filenames: array of filename strings (just basenames, not full paths)
 * count: number of filenames
 * zip_filename: the Content-Disposition filename for the download
 * Returns 1 on success, 0 on failure.
 */
int imgutil_send_zip(ACAP_HTTP_Response response,
                     const char* base_dir,
                     char** filenames,
                     int count,
                     const char* zip_filename);

#endif /* IMGUTIL_H */
