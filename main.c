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

    int cmd_idx = 1;
    if (strcmp(argv[1], "--config") == 0 && argc >= 4) {
        cdfs_set_config(argv[2]);
        cmd_idx = 3;
    }

    if (strcmp(argv[cmd_idx], "put") == 0) {
        if (argc - cmd_idx != 3) { printf("Usage: cdfs_client [--config path] put <local_path> <cdfs_path>\n"); return 1; }
        int32_t status = cdfs_put((const uint8_t *)argv[cmd_idx + 1], (const uint8_t *)argv[cmd_idx + 2]);
        if (status == 0) printf("Successfully put '%s' -> '%s'\n", argv[cmd_idx + 1], argv[cmd_idx + 2]);
        else             printf("Failed to put file.\n");

    } else if (strcmp(argv[cmd_idx], "get") == 0) {
        if (argc - cmd_idx != 3) { printf("Usage: cdfs_client [--config path] get <cdfs_path> <local_path>\n"); return 1; }
        int32_t status = cdfs_get((const uint8_t *)argv[cmd_idx + 1], (const uint8_t *)argv[cmd_idx + 2]);
        if (status == 0) printf("Successfully retrieved '%s' -> '%s'\n", argv[cmd_idx + 1], argv[cmd_idx + 2]);
        else             printf("Failed to get file.\n");

    } else if (strcmp(argv[cmd_idx], "ls") == 0) {
        if (argc - cmd_idx != 2) { printf("Usage: cdfs_client [--config path] ls <cdfs_path>\n"); return 1; }
        cdfs_ls((const uint8_t *)argv[cmd_idx + 1]);

    } else if (strcmp(argv[cmd_idx], "rm") == 0) {
        if (argc - cmd_idx != 2) { printf("Usage: cdfs_client [--config path] rm <cdfs_path>\n"); return 1; }
        int32_t status = cdfs_rm((const uint8_t *)argv[cmd_idx + 1]);
        if (status == 0) printf("Deleted '%s'\n", argv[cmd_idx + 1]);
        else             printf("Failed to delete '%s' (file not found?)\n", argv[cmd_idx + 1]);

    } else if (strcmp(argv[cmd_idx], "status") == 0) {
        if (argc - cmd_idx != 1) { printf("Usage: cdfs_client [--config path] status\n"); return 1; }
        int32_t status = cdfs_status();
        if (status != 0) printf("Failed to get cluster status.\n");

    } else {
        printf("Unknown command: %s\n", argv[cmd_idx]);
        print_usage();
        return 1;
    }

    return 0;
}
