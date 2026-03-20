#include "../common/dfs.h"
#include "../common/protocol.h"
#include "../common/serialization.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <time.h>
#include <string.h>
#include "metadata.h"

#define PORT 8080

typedef struct {
    uint8_t ip[16];
    int32_t port;
    time_t last_seen;
} storage_node_t;

storage_node_t active_nodes[32];
int32_t active_node_count = 0;

void handle_client(int32_t client_sock) {
    net_header_t header;
    if (recv_exact(client_sock, &header, sizeof(header)) != 0) {
        close(client_sock);
        return;
    }

    if (header.op_code == OP_REGISTER_FILE) {
        req_register_file_t req;
        if (recv_exact(client_sock, &req, sizeof(req)) != 0) {
            close(client_sock);
            return;
        }

        chunk_info_t *chunks = NULL;
        if (req.chunk_count > 0) {
            chunks = malloc(req.chunk_count * sizeof(chunk_info_t));
            if (chunks == NULL || recv_exact(client_sock, chunks, req.chunk_count * sizeof(chunk_info_t)) != 0) {
                if(chunks) free(chunks);
                close(client_sock);
                return;
            }
        }

        int32_t status = register_file(req.filename, chunks, req.chunk_count);
        if(chunks) free(chunks);
        
        // Respond
        net_header_t resp_header = { OP_REGISTER_FILE, sizeof(int32_t) };
        send_exact(client_sock, &resp_header, sizeof(resp_header));
        send_exact(client_sock, &status, sizeof(status));

    } else if (header.op_code == OP_GET_METADATA) {
        req_get_metadata_t req;
        if (recv_exact(client_sock, &req, sizeof(req)) != 0) {
            close(client_sock);
            return;
        }

        file_metadata_t meta;
        int32_t status = get_file_metadata(req.filename, &meta);
        
        resp_get_metadata_t resp;
        resp.status = status;
        resp.chunk_count = (status == 0) ? meta.chunk_count : 0;
        
        net_header_t resp_header = { OP_GET_METADATA, sizeof(resp) + resp.chunk_count * sizeof(chunk_info_t) };
        send_exact(client_sock, &resp_header, sizeof(resp_header));
        send_exact(client_sock, &resp, sizeof(resp));
        
        if (resp.chunk_count > 0) {
            send_exact(client_sock, meta.chunks, resp.chunk_count * sizeof(chunk_info_t));
        }
    } else if (header.op_code == OP_HEARTBEAT) {
        req_heartbeat_t req;
        if (recv_exact(client_sock, &req, sizeof(req)) == 0) {
            struct sockaddr_in peer_addr;
            socklen_t peer_len = sizeof(peer_addr);
            getpeername(client_sock, (struct sockaddr*)&peer_addr, &peer_len);
            uint8_t *ip = (uint8_t *)inet_ntoa(peer_addr.sin_addr);
            
            int32_t found = 0;
            for(int32_t i = 0; i < active_node_count; i++) {
                if(strcmp((char *)active_nodes[i].ip, (char *)ip) == 0 && active_nodes[i].port == req.storage_port) {
                    active_nodes[i].last_seen = time(NULL);
                    found = 1; 
                    break;
                }
            }
            if(!found && active_node_count < 32) {
                strncpy((char *)active_nodes[active_node_count].ip, (const char *)ip, 15);
                active_nodes[active_node_count].port = req.storage_port;
                active_nodes[active_node_count].last_seen = time(NULL);
                active_node_count++;
                printf("Registered new storage node %s:%d\n", ip, req.storage_port);
            }
        }
    } else if (header.op_code == OP_GET_ACTIVE_NODES) {
        resp_get_active_nodes_t resp;
        memset(&resp, 0, sizeof(resp));
        resp.node_count = 0;
        time_t now = time(NULL);
        
        for(int32_t i = 0; i < active_node_count && resp.node_count < REPLICATION_FACTOR; i++) {
            if (now - active_nodes[i].last_seen <= 15) { // heartbeat threshold
                strncpy((char *)resp.node_ips[resp.node_count], (const char *)active_nodes[i].ip, 15);
                resp.node_ports[resp.node_count] = active_nodes[i].port;
                resp.node_count++;
            }
        }
        
        net_header_t resp_header = { OP_GET_ACTIVE_NODES, sizeof(resp) };
        send_exact(client_sock, &resp_header, sizeof(resp_header));
        send_exact(client_sock, &resp, sizeof(resp));
    }
    close(client_sock);
}

int main() {
    load_fsimage();
    
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
    address.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }
    
    if (listen(server_fd, 10) < 0) {
        perror("listen");
        exit(EXIT_FAILURE);
    }

    printf("Metadata server listening on port %d\n", PORT);
    while (1) {
        int32_t client_sock = accept(server_fd, NULL, NULL);
        if (client_sock >= 0) {
            handle_client(client_sock);
        }
    }
    return 0;
}
