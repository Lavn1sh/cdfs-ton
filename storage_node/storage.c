#include "../common/dfs.h"
#include "../common/config.h"
#include "../common/serialization.h"
#include "../common/log.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <stdlib.h>

static uint8_t g_storage_dir[256] = "./storage_data";

void storage_init(const uint8_t *dir) {
    strncpy((char *)g_storage_dir, (const char *)dir, sizeof(g_storage_dir) - 1);
    g_storage_dir[sizeof(g_storage_dir) - 1] = '\0';
    mkdir((const char *)g_storage_dir, 0755);
}

int32_t store_chunk_stream(int32_t chunk_id, int32_t sockfd, size_t size, uint32_t expected_checksum) {
    char path[512];
    snprintf(path, sizeof(path), "%s/chunk_%d.dat", (char *)g_storage_dir, chunk_id);

    FILE *fp = fopen(path, "wb");
    if (!fp) return -1;
    
    uint8_t buf[8192];
    size_t remaining = size;
    uint32_t calc_chk = CHKSUM_INIT;
    
    while (remaining > 0) {
        size_t to_read = remaining < sizeof(buf) ? remaining : sizeof(buf);
        if (recv_exact(sockfd, buf, to_read) != 0) {
            fclose(fp);
            remove(path);
            return -1;
        }
        fwrite(buf, 1, to_read, fp);
        calc_chk = update_checksum(calc_chk, buf, to_read);
        remaining -= to_read;
    }
    fclose(fp);

    if (calc_chk != expected_checksum) {
        LOG_WARN("SN", "Checksum mismatch! Expected %u, got %u\n", expected_checksum, calc_chk);
        remove(path);
        return -1;
    }

    snprintf(path, sizeof(path), "%s/chunk_%d.crc", (char *)g_storage_dir, chunk_id);
    fp = fopen(path, "wb");
    if (fp) {
        fwrite(&expected_checksum, sizeof(uint32_t), 1, fp);
        fclose(fp);
    }
    return 0;
}

int32_t load_chunk_stream(int32_t chunk_id, int32_t sockfd, size_t file_size, uint32_t *out_checksum) {
    char path[512];
    
    // Read checksum
    snprintf(path, sizeof(path), "%s/chunk_%d.crc", (char *)g_storage_dir, chunk_id);
    FILE *fp = fopen(path, "rb");
    if (fp) {
        if (fread(out_checksum, sizeof(uint32_t), 1, fp) != 1) *out_checksum = 0;
        fclose(fp);
    } else {
        *out_checksum = 0;
    }

    // Stream dat to socket
    snprintf(path, sizeof(path), "%s/chunk_%d.dat", (char *)g_storage_dir, chunk_id);
    fp = fopen(path, "rb");
    if (!fp) return -1;
    
    uint8_t buf[8192];
    size_t remaining = file_size;
    while (remaining > 0) {
        size_t to_read = remaining < sizeof(buf) ? remaining : sizeof(buf);
        size_t bytes = fread(buf, 1, to_read, fp);
        if (bytes == 0) break; // Unexpected EOF
        if (send_exact(sockfd, buf, bytes) != 0) {
            fclose(fp);
            return -1;
        }
        remaining -= bytes;
    }
    fclose(fp);

    if (remaining > 0) return -1; // File size on disk was smaller than metadata
    return 0;
}

int32_t delete_chunk_files(int32_t chunk_id) {
    char path[512];
    snprintf(path, sizeof(path), "%s/chunk_%d.dat", (char *)g_storage_dir, chunk_id);
    remove(path);
    snprintf(path, sizeof(path), "%s/chunk_%d.crc", (char *)g_storage_dir, chunk_id);
    remove(path);
    return 0;
}

/* Scan the storage dir and fill out_ids with chunk IDs found.
 * Returns the number of chunks found (up to max_count). */
int32_t scan_chunks(int32_t *out_ids, int32_t max_count) {
    DIR *dir = opendir((char *)g_storage_dir);
    if (!dir) return 0;

    int32_t count = 0;
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL && count < max_count) {
        int32_t chunk_id;
        if (sscanf(entry->d_name, "chunk_%d.dat", &chunk_id) == 1) {
            out_ids[count++] = chunk_id;
        }
    }
    closedir(dir);
    return count;
}
