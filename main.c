#include "common/dfs.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

void print_usage(void) {
    printf("Usage: cdfs_client <command> [args]\n");
    printf("Commands:\n");
    printf("  put <local_path> <cdfs_path>   Upload a file\n");
    printf("  get <cdfs_path> <local_path>   Download a file\n");
    printf("  ls  <cdfs_dir_path>            List files under a prefix\n");
    printf("  rm  <cdfs_path>                Delete a file\n");
    printf("  status                         Show cluster metrics in JSON\n");
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        print_usage();
        return 1;
    }

    if (strcmp(argv[1], "put") == 0) {
        if (argc != 4) { printf("Usage: cdfs_client put <local_path> <cdfs_path>\n"); return 1; }
        int32_t status = cdfs_put((const uint8_t *)argv[2], (const uint8_t *)argv[3]);
        if (status == 0) printf("Successfully put '%s' -> '%s'\n", argv[2], argv[3]);
        else             printf("Failed to put file.\n");

    } else if (strcmp(argv[1], "get") == 0) {
        if (argc != 4) { printf("Usage: cdfs_client get <cdfs_path> <local_path>\n"); return 1; }
        int32_t status = cdfs_get((const uint8_t *)argv[2], (const uint8_t *)argv[3]);
        if (status == 0) printf("Successfully retrieved '%s' -> '%s'\n", argv[2], argv[3]);
        else             printf("Failed to get file.\n");

    } else if (strcmp(argv[1], "ls") == 0) {
        if (argc != 3) { printf("Usage: cdfs_client ls <cdfs_path>\n"); return 1; }
        cdfs_ls((const uint8_t *)argv[2]);

    } else if (strcmp(argv[1], "rm") == 0) {
        if (argc != 3) { printf("Usage: cdfs_client rm <cdfs_path>\n"); return 1; }
        int32_t status = cdfs_rm((const uint8_t *)argv[2]);
        if (status == 0) printf("Deleted '%s'\n", argv[2]);
        else             printf("Failed to delete '%s' (file not found?)\n", argv[2]);

    } else if (strcmp(argv[1], "status") == 0) {
        if (argc != 2) { printf("Usage: cdfs_client status\n"); return 1; }
        int32_t status = cdfs_status();
        if (status != 0) printf("Failed to get cluster status.\n");

    } else {
        printf("Unknown command: %s\n", argv[1]);
        print_usage();
        return 1;
    }

    return 0;
}
