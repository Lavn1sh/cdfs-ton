#include "dfs.h"
#include <stdio.h>

int main() {
    cdfs_put((const uint8_t *)"input.txt", (const uint8_t *)"/dfs/input.txt");
    cdfs_get((const uint8_t *)"/dfs/input.txt", (const uint8_t *)"output.txt");

    printf("DFS test completed\n");
    return 0;
}
