// THIS IS CURRENTLY A FAKE SIMULATION, NEEDS TO BE CHANGED LATER

#include "dfs.h"
#include <stdint.h>
#include <stdio.h>

int32_t store_chunk(int32_t chunk_id, const uint8_t *data, size_t size, uint32_t checksum) {
    uint8_t filename[64];
    snprintf((char *)filename, sizeof(filename), "chunk_%d.dat", chunk_id);

    FILE *fp = fopen((char *)filename, "wb");
    if (!fp)
        return -1;

    fwrite(data, 1, size, fp);
    fclose(fp);

    snprintf((char *)filename, sizeof(filename), "chunk_%d.crc", chunk_id);
    fp = fopen((char *)filename, "wb");
    if (fp) {
        fwrite(&checksum, sizeof(uint32_t), 1, fp);
        fclose(fp);
    }

    return 0;
}

int32_t load_chunk(int32_t chunk_id, uint8_t *buffer, size_t buffer_size,
                   size_t *bytes_read, uint32_t *checksum) {
    uint8_t filename[64];
    snprintf((char *)filename, sizeof(filename), "chunk_%d.dat", chunk_id);

    FILE *fp = fopen((char *)filename, "rb");
    if (!fp)
        return -1;

    *bytes_read = fread(buffer, 1, buffer_size, fp);
    fclose(fp);

    snprintf((char *)filename, sizeof(filename), "chunk_%d.crc", chunk_id);
    fp = fopen((char *)filename, "rb");
    if (fp) {
        if (fread(checksum, sizeof(uint32_t), 1, fp) != 1) *checksum = 0;
        fclose(fp);
    } else {
        *checksum = 0;
    }

    return 0;
}
