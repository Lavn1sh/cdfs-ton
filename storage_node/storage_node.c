#include "../common/dfs.h"
#include "../common/protocol.h"
#include "../common/serialization.h"
#include "../common/config.h"
#include "../common/log.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <sys/stat.h>

// Declared in storage.c
void    storage_init(const uint8_t *dir);
int32_t store_chunk(int32_t chunk_id, const uint8_t *data, size_t size, uint32_t checksum);
int32_t load_chunk(int32_t chunk_id, uint8_t *buffer, size_t buffer_size, size_t *bytes_read, uint32_t *checksum);
int32_t delete_chunk_files(int32_t chunk_id);
int32_t scan_chunks(int32_t *out_ids, int32_t max_count);

cdfs_config_t g_config;
int32_t storage_port = 8081;

// Helper: open a TCP connection

static int32_t connect_to(const uint8_t *ip, int32_t port) {
    int32_t sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) return -1;
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port   = htons(port);
    inet_pton(AF_INET, (const char *)ip, &addr.sin_addr);
    if (connect(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        close(sock);
        return -1;
    }
    return sock;
}

// Heartbeat + block report thread

void* heartbeat_thread(void* arg) {
    (void)arg;
    while (1) {
        // --- Heartbeat ---
        int32_t sock = connect_to(g_config.meta_ip, g_config.meta_port);
        if (sock >= 0) {
            req_heartbeat_t hb = { storage_port, 1024 };
            net_header_t hdr   = { OP_HEARTBEAT, sizeof(hb) };
            send_exact(sock, &hdr, sizeof(hdr));
            send_exact(sock, &hb, sizeof(hb));
            close(sock);
        }

        // --- Block report ---
        sock = connect_to(g_config.meta_ip, g_config.meta_port);
        if (sock >= 0) {
            req_block_report_t report;
            report.storage_port = storage_port;
            report.chunk_count  = scan_chunks(report.chunk_ids, MAX_CHUNKS);

            net_header_t hdr = { OP_BLOCK_REPORT, sizeof(report) };
            send_exact(sock, &hdr, sizeof(hdr));
            send_exact(sock, &report, sizeof(report));
            close(sock);
        }

        sleep(5);
    }
    return NULL;
}

// Client request handler

void handle_client(int32_t client_sock) {
    net_header_t header;
    if (recv_exact(client_sock, &header, sizeof(header)) != 0) {
        close(client_sock);
        return;
    }

    // OP_STORE_CHUNK 

    if (header.op_code == OP_STORE_CHUNK) {
        req_store_chunk_t req;
        if (recv_exact(client_sock, &req, sizeof(req)) != 0) { close(client_sock); return; }

        int32_t status = store_chunk_stream(req.chunk_id, client_sock, req.size, req.checksum);

        resp_store_chunk_t resp      = { status };
        net_header_t       resp_hdr  = { OP_STORE_CHUNK, sizeof(resp) };
        send_exact(client_sock, &resp_hdr, sizeof(resp_hdr));
        send_exact(client_sock, &resp, sizeof(resp));

    // OP_LOAD_CHUNK 

    } else if (header.op_code == OP_LOAD_CHUNK) {
        req_load_chunk_t req;
        if (recv_exact(client_sock, &req, sizeof(req)) != 0) { close(client_sock); return; }

        int32_t status = -1;
        uint32_t checksum = 0;
        char path[512];
        snprintf(path, sizeof(path), "%s/chunk_%d.dat", (char*)g_config.storage_dir, req.chunk_id);
        struct stat st;
        size_t bytes_to_read = 0;

        if (stat(path, &st) == 0) {
            status = 0;
            bytes_to_read = st.st_size;
            snprintf(path, sizeof(path), "%s/chunk_%d.crc", (char*)g_config.storage_dir, req.chunk_id);
            FILE *f_crc = fopen(path, "rb");
            if (f_crc) { fread(&checksum, sizeof(uint32_t), 1, f_crc); fclose(f_crc); }
        }

        resp_load_chunk_t resp     = { status, bytes_to_read, checksum };
        net_header_t      resp_hdr = { OP_LOAD_CHUNK, (uint32_t)(sizeof(resp) + (status == 0 ? bytes_to_read : 0)) };
        send_exact(client_sock, &resp_hdr, sizeof(resp_hdr));
        send_exact(client_sock, &resp, sizeof(resp));
        if (status == 0 && bytes_to_read > 0) {
            load_chunk_stream(req.chunk_id, client_sock, bytes_to_read, &checksum);
        }

    // OP_REPLICATE_CHUNK 
    // MD instructs this node to fetch a chunk locally and push it to dst node.

    } else if (header.op_code == OP_REPLICATE_CHUNK) {
        req_replicate_chunk_t req;
        if (recv_exact(client_sock, &req, sizeof(req)) != 0) { close(client_sock); return; }

        int32_t status = -1;
        char path[512];
        snprintf(path, sizeof(path), "%s/chunk_%d.dat", (char*)g_config.storage_dir, req.chunk_id);
        struct stat st;

        if (stat(path, &st) == 0) {
            uint32_t checksum = 0;
            snprintf(path, sizeof(path), "%s/chunk_%d.crc", (char*)g_config.storage_dir, req.chunk_id);
            FILE *f_crc = fopen(path, "rb");
            if (f_crc) { fread(&checksum, sizeof(uint32_t), 1, f_crc); fclose(f_crc); }

            int32_t dst_sock = connect_to(req.dst_ip, req.dst_port);
            if (dst_sock >= 0) {
                req_store_chunk_t sreq  = { req.chunk_id, st.st_size, checksum };
                net_header_t      shdr  = { OP_STORE_CHUNK, (uint32_t)(sizeof(sreq) + st.st_size) };
                send_exact(dst_sock, &shdr,  sizeof(shdr));
                send_exact(dst_sock, &sreq,  sizeof(sreq));

                int32_t push_status = load_chunk_stream(req.chunk_id, dst_sock, st.st_size, &checksum);
                
                if (push_status == 0) {
                    net_header_t      ack_hdr;
                    resp_store_chunk_t ack;
                    if (recv_exact(dst_sock, &ack_hdr, sizeof(ack_hdr)) == 0 &&
                        recv_exact(dst_sock, &ack,     sizeof(ack))     == 0 &&
                        ack.status == 0) {
                        status = 0;
                        LOG_INFO("SN", "Replicated chunk %d to %s:%d\n",
                               req.chunk_id, req.dst_ip, req.dst_port);
                    }
                }
                close(dst_sock);
            }
        }

        resp_replicate_chunk_t resp    = { status };
        net_header_t           resp_hdr = { OP_REPLICATE_CHUNK, sizeof(resp) };
        send_exact(client_sock, &resp_hdr, sizeof(resp_hdr));
        send_exact(client_sock, &resp, sizeof(resp));

    // OP_DELETE_CHUNK 

    } else if (header.op_code == OP_DELETE_CHUNK) {
        req_delete_chunk_t req;
        if (recv_exact(client_sock, &req, sizeof(req)) != 0) { close(client_sock); return; }

        int32_t status = delete_chunk_files(req.chunk_id);
        LOG_INFO("SN", "Deleted chunk %d: status=%d\n", req.chunk_id, status);

        resp_delete_chunk_t resp    = { status };
        net_header_t        resp_hdr = { OP_DELETE_CHUNK, sizeof(resp) };
        send_exact(client_sock, &resp_hdr, sizeof(resp_hdr));
        send_exact(client_sock, &resp, sizeof(resp));
    }

    close(client_sock);
}

// client_thread helper

void *client_thread(void *arg) {
    int32_t client_sock = (int32_t)(intptr_t)arg;
    handle_client(client_sock);
    return NULL;
}

// main

int main(int argc, char *argv[]) {
    load_config((const uint8_t *)"cdfs.conf", &g_config);
    if (argc > 1) {
        storage_port = atoi(argv[1]);
    }

    storage_init((const uint8_t *)g_config.storage_dir);

    int32_t server_fd;
    struct sockaddr_in address;

    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    int32_t opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }

    address.sin_family      = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port        = htons(storage_port);

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }

    if (listen(server_fd, 10) < 0) {
        perror("listen");
        exit(EXIT_FAILURE);
    }

    pthread_t hb_tid;
    pthread_create(&hb_tid, NULL, heartbeat_thread, NULL);

    LOG_INFO("SN", "Storage node listening on port %d (data dir: %s)\n",
           storage_port, g_config.storage_dir);
    while (1) {
        int32_t client_sock = accept(server_fd, NULL, NULL);
        if (client_sock >= 0) {
            pthread_t tid;
            pthread_create(&tid, NULL, client_thread, (void *)(intptr_t)client_sock);
            pthread_detach(tid);
        }
    }
    return 0;
}
