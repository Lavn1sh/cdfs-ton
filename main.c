#include "dfs.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

void print_usage() {
    printf("Usage: cdfs_client <command> [args]\n");
    printf("Commands:\n");
    printf("  put <local_path> <cdfs_path>\n");
    printf("  get <cdfs_path> <local_path>\n");
    printf("  ls <cdfs_dir_path>\n");
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        print_usage();
        return 1;
    }

    if (strcmp(argv[1], "put") == 0) {
        if (argc != 4) {
            printf("Usage: cdfs_client put <local_path> <cdfs_path>\n");
            return 1;
        }
        int32_t status = cdfs_put((const uint8_t *)argv[2], (const uint8_t *)argv[3]);
        if (status == 0) {
            printf("Successfully put %s to %s\n", argv[2], argv[3]);
        } else {
            printf("Failed to put file.\n");
        }
    } else if (strcmp(argv[1], "get") == 0) {
        if (argc != 4) {
            printf("Usage: cdfs_client get <cdfs_path> <local_path>\n");
            return 1;
        }
        int32_t status = cdfs_get((const uint8_t *)argv[2], (const uint8_t *)argv[3]);
        if (status == 0) {
            printf("Successfully got %s to %s\n", argv[2], argv[3]);
        } else {
            printf("Failed to get file.\n");
        }
    } else if (strcmp(argv[1], "ls") == 0) {
        if (argc != 3) {
            printf("Usage: cdfs_client ls <cdfs_path>\n");
            return 1;
        }
        cdfs_ls((const uint8_t *)argv[2]);
    } else {
        printf("Unknown command: %s\n", argv[1]);
        print_usage();
        return 1;
    }

    return 0;
}
