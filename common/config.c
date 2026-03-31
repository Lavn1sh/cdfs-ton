#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int32_t load_config(const uint8_t *filename, cdfs_config_t *out_config) {
    // Default values if missing
    strncpy((char *)out_config->meta_ip, "127.0.0.1", MAX_IP_LEN - 1);
    out_config->meta_ip[MAX_IP_LEN - 1] = '\0';
    out_config->meta_port = 8080;
    strncpy((char *)out_config->storage_dir, "./storage_data", sizeof(out_config->storage_dir) - 1);
    out_config->storage_dir[sizeof(out_config->storage_dir) - 1] = '\0';

    FILE *fp = fopen((const char *)filename, "r");
    if (!fp) {
        printf("Could not open config file %s, using defaults: %s:%d\n",
               (const char *)filename, (const char *)out_config->meta_ip, out_config->meta_port);
        return 0; // Succeed with defaults
    }

    uint8_t line[256];
    while (fgets((char *)line, sizeof(line), fp)) {
        uint8_t *key = (uint8_t *)strtok((char *)line, "=");
        uint8_t *val = (uint8_t *)strtok(NULL, "\n");
        if (key && val) {
            if (strcmp((char *)key, "META_IP") == 0) {
                strncpy((char *)out_config->meta_ip, (char *)val, MAX_IP_LEN - 1);
                out_config->meta_ip[MAX_IP_LEN - 1] = '\0';
            } else if (strcmp((char *)key, "META_PORT") == 0) {
                out_config->meta_port = atoi((char *)val);
            } else if (strcmp((char *)key, "STORAGE_DIR") == 0) {
                strncpy((char *)out_config->storage_dir, (char *)val, sizeof(out_config->storage_dir) - 1);
                out_config->storage_dir[sizeof(out_config->storage_dir) - 1] = '\0';
            }
        }
    }
    fclose(fp);
    return 0;
}
