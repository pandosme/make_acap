#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>
#include <syslog.h>
#include <sys/stat.h>
#include "imgutil.h"
#include "ACAP.h"

#define LOG_WARN(fmt, args...)  { syslog(LOG_WARNING, fmt, ## args); printf(fmt, ## args); }

/* ── CRC-32 ─────────────────────────────────────────────────────────────── */

static uint32_t crc32_table[256];
static int crc32_table_ready = 0;

static void crc32_init(void) {
    if (crc32_table_ready) return;
    for (uint32_t i = 0; i < 256; i++) {
        uint32_t c = i;
        for (int j = 0; j < 8; j++)
            c = (c & 1) ? (0xEDB88320u ^ (c >> 1)) : (c >> 1);
        crc32_table[i] = c;
    }
    crc32_table_ready = 1;
}

static uint32_t crc32_update(uint32_t crc, const unsigned char* data, size_t len) {
    crc = ~crc;
    for (size_t i = 0; i < len; i++)
        crc = crc32_table[(crc ^ data[i]) & 0xFF] ^ (crc >> 8);
    return ~crc;
}

/* ── JPEG save ──────────────────────────────────────────────────────────── */

int imgutil_save_jpeg(const char* filepath, unsigned char* data, unsigned int size) {
    FILE* f = fopen(filepath, "wb");
    if (!f) {
        LOG_WARN("imgutil: failed to open %s for write: %s\n", filepath, strerror(errno));
        return 0;
    }
    size_t written = fwrite(data, 1, size, f);
    fclose(f);
    if (written != size) {
        LOG_WARN("imgutil: short write on %s (%zu/%u)\n", filepath, written, size);
        return 0;
    }
    return 1;
}

/* ── ZIP helpers ────────────────────────────────────────────────────────── */

/* Write a little-endian 16-bit value into buf */
static void put_u16(unsigned char* buf, uint16_t v) {
    buf[0] = (unsigned char)(v & 0xFF);
    buf[1] = (unsigned char)((v >> 8) & 0xFF);
}

/* Write a little-endian 32-bit value into buf */
static void put_u32(unsigned char* buf, uint32_t v) {
    buf[0] = (unsigned char)(v & 0xFF);
    buf[1] = (unsigned char)((v >> 8) & 0xFF);
    buf[2] = (unsigned char)((v >> 16) & 0xFF);
    buf[3] = (unsigned char)((v >> 24) & 0xFF);
}

#define LFH_SIZE    30   /* Local File Header fixed part */
#define CDH_SIZE    46   /* Central Directory Header fixed part */
#define EOCD_SIZE   22   /* End of Central Directory fixed size */

/* ── ZIP send ───────────────────────────────────────────────────────────── */

typedef struct {
    char     name[256];
    uint32_t size;
    uint32_t crc;
    uint32_t offset;  /* offset of Local File Header in ZIP stream */
} ZipEntry;

int imgutil_send_zip(ACAP_HTTP_Response response,
                     const char* base_dir,
                     char** filenames,
                     int count,
                     const char* zip_filename) {
    if (count <= 0) return 0;
    crc32_init();

    ZipEntry* entries = calloc(count, sizeof(ZipEntry));
    if (!entries) return 0;

    /* ── Pass 1: stat files, compute CRCs, calculate total size ── */
    uint32_t stream_offset = 0;
    int valid = 0;

    for (int i = 0; i < count; i++) {
        char filepath[1024];
        snprintf(filepath, sizeof(filepath), "%s/%s", base_dir, filenames[i]);

        struct stat st;
        if (stat(filepath, &st) != 0) continue;

        /* Read file to compute CRC */
        FILE* f = fopen(filepath, "rb");
        if (!f) continue;

        uint32_t crc = 0;
        unsigned char buf[65536];
        size_t n;
        while ((n = fread(buf, 1, sizeof(buf), f)) > 0)
            crc = crc32_update(crc, buf, n);
        fclose(f);

        strncpy(entries[valid].name, filenames[i], 255);
        entries[valid].name[255] = '\0';
        entries[valid].size   = (uint32_t)st.st_size;
        entries[valid].crc    = crc;
        entries[valid].offset = stream_offset;

        uint16_t namelen = (uint16_t)strlen(entries[valid].name);
        stream_offset += LFH_SIZE + namelen + entries[valid].size;
        valid++;
    }

    if (valid == 0) {
        free(entries);
        return 0;
    }

    /* Central directory starts after all local file entries */
    uint32_t cd_offset = stream_offset;
    uint32_t cd_size   = 0;
    for (int i = 0; i < valid; i++)
        cd_size += CDH_SIZE + (uint16_t)strlen(entries[i].name);
    uint32_t total_size = cd_offset + cd_size + EOCD_SIZE;

    /* ── Send HTTP header ── */
    ACAP_HTTP_Header_FILE(response, zip_filename, "application/zip", total_size);

    /* ── Pass 2: stream local file headers + data ── */
    for (int i = 0; i < valid; i++) {
        uint16_t namelen = (uint16_t)strlen(entries[i].name);
        unsigned char lfh[LFH_SIZE];
        memset(lfh, 0, sizeof(lfh));

        /* Local file header signature */
        put_u32(lfh + 0,  0x04034B50u);
        put_u16(lfh + 4,  20);           /* version needed: 2.0 */
        put_u16(lfh + 6,  0);            /* general purpose flags */
        put_u16(lfh + 8,  0);            /* compression: STORE */
        put_u16(lfh + 10, 0);            /* mod time */
        put_u16(lfh + 12, 0);            /* mod date */
        put_u32(lfh + 14, entries[i].crc);
        put_u32(lfh + 18, entries[i].size);
        put_u32(lfh + 22, entries[i].size);
        put_u16(lfh + 26, namelen);
        put_u16(lfh + 28, 0);            /* extra field length */

        ACAP_HTTP_Respond_Data(response, LFH_SIZE, lfh);
        ACAP_HTTP_Respond_Data(response, namelen, entries[i].name);

        /* Stream file data */
        char filepath[1024];
        snprintf(filepath, sizeof(filepath), "%s/%s", base_dir, entries[i].name);
        FILE* f = fopen(filepath, "rb");
        if (f) {
            unsigned char buf[65536];
            size_t n;
            while ((n = fread(buf, 1, sizeof(buf), f)) > 0)
                ACAP_HTTP_Respond_Data(response, n, buf);
            fclose(f);
        }
    }

    /* ── Central directory ── */
    for (int i = 0; i < valid; i++) {
        uint16_t namelen = (uint16_t)strlen(entries[i].name);
        unsigned char cdh[CDH_SIZE];
        memset(cdh, 0, sizeof(cdh));

        put_u32(cdh + 0,  0x02014B50u);  /* central dir signature */
        put_u16(cdh + 4,  20);           /* version made by */
        put_u16(cdh + 6,  20);           /* version needed */
        put_u16(cdh + 8,  0);            /* flags */
        put_u16(cdh + 10, 0);            /* compression: STORE */
        put_u16(cdh + 12, 0);            /* mod time */
        put_u16(cdh + 14, 0);            /* mod date */
        put_u32(cdh + 16, entries[i].crc);
        put_u32(cdh + 20, entries[i].size);
        put_u32(cdh + 24, entries[i].size);
        put_u16(cdh + 28, namelen);
        put_u16(cdh + 30, 0);            /* extra field length */
        put_u16(cdh + 32, 0);            /* comment length */
        put_u16(cdh + 34, 0);            /* disk number start */
        put_u16(cdh + 36, 0);            /* internal attrs */
        put_u32(cdh + 38, 0);            /* external attrs */
        put_u32(cdh + 42, entries[i].offset);

        ACAP_HTTP_Respond_Data(response, CDH_SIZE, cdh);
        ACAP_HTTP_Respond_Data(response, namelen, entries[i].name);
    }

    /* ── End of central directory ── */
    unsigned char eocd[EOCD_SIZE];
    memset(eocd, 0, sizeof(eocd));
    put_u32(eocd + 0,  0x06054B50u);   /* EOCD signature */
    put_u16(eocd + 4,  0);             /* disk number */
    put_u16(eocd + 6,  0);             /* disk with CD start */
    put_u16(eocd + 8,  (uint16_t)valid);
    put_u16(eocd + 10, (uint16_t)valid);
    put_u32(eocd + 12, cd_size);
    put_u32(eocd + 16, cd_offset);
    put_u16(eocd + 20, 0);             /* comment length */
    ACAP_HTTP_Respond_Data(response, EOCD_SIZE, eocd);

    free(entries);
    return 1;
}
