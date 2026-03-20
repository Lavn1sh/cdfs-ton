#include "../common/dfs.h"
#include "../common/protocol.h"
#include "../common/serialization.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>

#define METADATA_IP "127.0.0.1"
#define METADATA_PORT 8080

int32_t storage_port = 8081;

void* heartbeat_thread(void* arg) {
    (void)arg;
    while (1) {
        int32_t sock = socket(AF_INET, SOCK_STREAM, 0);
        if (sock >= 0) {
            struct sockaddr_in serv_addr;
            serv_addr.sin_family = AF_INET;
            serv_addr.sin_port = htons(METADATA_PORT);
            inet_pton(AF_INET, METADATA_IP, &serv_addr.sin_addr);
            
            if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) == 0) {
                req_heartbeat_t hb = { storage_port, 1024 }; // Report 1024 MB
                net_header_t hdr = { OP_HEARTBEAT, sizeof(hb) };
                send_exact(sock, &hdr, sizeof(hdr));
                send_exact(sock, &hb, sizeof(hb));
            }
            close(sock);
        }
        sleep(5);
    }
    return NULL;
}

void handle_client(int32_t client_sock) {
    net_header_t header;
    if (recv_exact(client_sock, &header, sizeof(header)) != 0) {
        close(client_sock);
        return;
    }

    if (header.op_code == OP_STORE_CHUNK) {
        req_store_chunk_t req;
        if (recv_exact(client_sock, &req, sizeof(req)) != 0) {
            close(client_sock);
            return;
        }

        uint8_t *data = malloc(req.size);
        if (!data || recv_exact(client_sock, data, req.size) != 0) {
            if (data) free(data);
            close(client_sock);
            return;
        }

        uint32_t calc_chk = calculate_checksum(data, req.size);
        int32_t status = 0;
        if (calc_chk != req.checksum) {
            printf("Checksum mismatch on receive! Expected %u, got %u\n", req.checksum, calc_chk);
            status = -1;
        } else {
            status = store_chunk(req.chunk_id, data, req.size, req.checksum);
        }
        free(data);

        resp_store_chunk_t resp = { status };
        net_header_t resp_header = { OP_STORE_CHUNK, sizeof(resp) };
        send_exact(client_sock, &resp_header, sizeof(resp_header));
        send_exact(client_sock, &resp, sizeof(resp));

    } else if (header.op_code == OP_LOAD_CHUNK) {
        req_load_chunk_t req;
        if (recv_exact(client_sock, &req, sizeof(req)) != 0) {
            close(client_sock);
            return;
        }

        uint8_t *buffer = malloc(req.size);
        size_t bytes_read = 0;
        int32_t status = -1;
        uint32_t checksum = 0;
        
        if (buffer) {
            status = load_chunk(req.chunk_id, buffer, req.size, &bytes_read, &checksum);
        }

        resp_load_chunk_t resp = { status, bytes_read, checksum };
        net_header_t resp_header = { OP_LOAD_CHUNK, sizeof(resp) + (status == 0 ? bytes_read : 0) };
        
        send_exact(client_sock, &resp_header, sizeof(resp_header));
        send_exact(client_sock, &resp, sizeof(resp));
        
        if (status == 0 && bytes_read > 0) {
            send_exact(client_sock, buffer, bytes_read);
        }
        
        if (buffer) free(buffer);
    }
    close(client_sock);
}

int main(int argc, char *argv[]) {
    if (argc > 1) {
        storage_port = atoi(argv[1]);
    }
    
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

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(storage_port);

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }
    
    if (listen(server_fd, 10) < 0) {
        perror("listen");
        exit(EXIT_FAILURE);
    }

    pthread_t tid;
    pthread_create(&tid, NULL, heartbeat_thread, NULL);

    printf("Storage node listening on port %d\n", storage_port);
    while (1) {
        int32_t client_sock = accept(server_fd, NULL, NULL);
        if (client_sock >= 0) {
            handle_client(client_sock);
        }
    }
    return 0;
}
