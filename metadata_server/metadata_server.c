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
#include <pthread.h>
#include <pthread.h>
#include "metadata.h"
#include "../common/config.h"
#include "../common/log.h"

// Global state

cdfs_config_t g_config;

typedef struct {
    uint8_t ip[16];
    int32_t port;
    time_t  last_seen;
    int32_t dead;          // 1 if declared dead, waiting for clean-up
} storage_node_t;

#define MAX_NODES 32
static storage_node_t active_nodes[MAX_NODES];
static int32_t        active_node_count = 0;
static pthread_mutex_t nodes_mutex = PTHREAD_MUTEX_INITIALIZER;

// Per-node block report: store up to MAX_CHUNKS chunk ids per node
static int32_t node_chunks[MAX_NODES][MAX_CHUNKS];
static int32_t node_chunk_counts[MAX_NODES];

// Helpers

static int32_t connect_to_node(const uint8_t *ip, int32_t port) {
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

// handle_client: runs on the accept() loop thread (single-threaded accept)

void handle_client(int32_t client_sock) {
    net_header_t header;
    if (recv_exact(client_sock, &header, sizeof(header)) != 0) {
        close(client_sock);
        return;
    }

    // OP_REGISTER_FILE 

    if (header.op_code == OP_REGISTER_FILE) {
        req_register_file_t req;
        if (recv_exact(client_sock, &req, sizeof(req)) != 0) { close(client_sock); return; }

        chunk_info_t *chunks = NULL;
        if (req.chunk_count > 0) {
            chunks = malloc(req.chunk_count * sizeof(chunk_info_t));
            if (!chunks || recv_exact(client_sock, chunks, req.chunk_count * sizeof(chunk_info_t)) != 0) {
                free(chunks);
                close(client_sock); return;
            }
        }

        int32_t status = register_file(req.filename, chunks, req.chunk_count);
        free(chunks);

        net_header_t resp_header = { OP_REGISTER_FILE, sizeof(int32_t) };
        send_exact(client_sock, &resp_header, sizeof(resp_header));
        send_exact(client_sock, &status, sizeof(status));

    // OP_GET_METADATA 

    } else if (header.op_code == OP_GET_METADATA) {
        req_get_metadata_t req;
        if (recv_exact(client_sock, &req, sizeof(req)) != 0) { close(client_sock); return; }

        file_metadata_t meta;
        int32_t status = get_file_metadata(req.filename, &meta);

        resp_get_metadata_t resp;
        resp.status      = status;
        resp.chunk_count = (status == 0) ? meta.chunk_count : 0;

        net_header_t resp_header = { OP_GET_METADATA, sizeof(resp) + resp.chunk_count * sizeof(chunk_info_t) };
        send_exact(client_sock, &resp_header, sizeof(resp_header));
        send_exact(client_sock, &resp, sizeof(resp));
        if (resp.chunk_count > 0) {
            send_exact(client_sock, meta.chunks, resp.chunk_count * sizeof(chunk_info_t));
        }

    // OP_HEARTBEAT 

    } else if (header.op_code == OP_HEARTBEAT) {
        req_heartbeat_t req;
        if (recv_exact(client_sock, &req, sizeof(req)) != 0) { close(client_sock); return; }

        struct sockaddr_in peer_addr;
        socklen_t peer_len = sizeof(peer_addr);
        getpeername(client_sock, (struct sockaddr *)&peer_addr, &peer_len);
        const char *ip = inet_ntoa(peer_addr.sin_addr);

        pthread_mutex_lock(&nodes_mutex);
        int32_t found = 0;
        for (int32_t i = 0; i < active_node_count; i++) {
            if (strcmp((char *)active_nodes[i].ip, ip) == 0 &&
                active_nodes[i].port == req.storage_port) {
                active_nodes[i].last_seen = time(NULL);
                active_nodes[i].dead      = 0;
                found = 1;
                break;
            }
        }
        if (!found && active_node_count < MAX_NODES) {
            strncpy((char *)active_nodes[active_node_count].ip, ip, 15);
            active_nodes[active_node_count].port      = req.storage_port;
            active_nodes[active_node_count].last_seen = time(NULL);
            active_nodes[active_node_count].dead      = 0;
            active_node_count++;
            LOG_INFO("MD", "Registered new storage node %s:%d\n", ip, req.storage_port);
        }
        pthread_mutex_unlock(&nodes_mutex);

    // OP_BLOCK_REPORT 

    } else if (header.op_code == OP_BLOCK_REPORT) {
        req_block_report_t req;
        if (recv_exact(client_sock, &req, sizeof(req)) != 0) { close(client_sock); return; }

        struct sockaddr_in peer_addr;
        socklen_t peer_len = sizeof(peer_addr);
        getpeername(client_sock, (struct sockaddr *)&peer_addr, &peer_len);
        const char *ip = inet_ntoa(peer_addr.sin_addr);

        pthread_mutex_lock(&nodes_mutex);
        for (int32_t i = 0; i < active_node_count; i++) {
            if (strcmp((char *)active_nodes[i].ip, ip) == 0 &&
                active_nodes[i].port == req.storage_port) {
                int32_t cnt = req.chunk_count < MAX_CHUNKS ? req.chunk_count : MAX_CHUNKS;
                memcpy(node_chunks[i], req.chunk_ids, cnt * sizeof(int32_t));
                node_chunk_counts[i] = cnt;
                break;
            }
        }
        pthread_mutex_unlock(&nodes_mutex);
        /* 
         * LOG_INFO("MD", "Received block report from %s:%d (%d chunks)\n",
         *       ip, req.storage_port, req.chunk_count);
         */

    // OP_LIST_FILES 

    } else if (header.op_code == OP_LIST_FILES) {
        req_list_files_t req;
        if (recv_exact(client_sock, &req, sizeof(req)) != 0) { close(client_sock); return; }

        uint8_t matching_files[MAX_FILES][MAX_FILENAME];
        int32_t match_count = list_files_in_dir(req.directory, matching_files);

        resp_list_files_t resp = { match_count };
        net_header_t resp_header = { OP_LIST_FILES, sizeof(resp) + match_count * MAX_FILENAME };
        send_exact(client_sock, &resp_header, sizeof(resp_header));
        send_exact(client_sock, &resp, sizeof(resp));
        if (match_count > 0) {
            send_exact(client_sock, matching_files, match_count * MAX_FILENAME);
        }

    // OP_GET_ACTIVE_NODES 

    } else if (header.op_code == OP_GET_ACTIVE_NODES) {
        resp_get_active_nodes_t resp;
        memset(&resp, 0, sizeof(resp));
        time_t now = time(NULL);

        pthread_mutex_lock(&nodes_mutex);
        static int32_t rr_offset = 0;
        int32_t start_idx = rr_offset;

        for (int32_t count = 0; count < active_node_count && resp.node_count < REPLICATION_FACTOR; count++) {
            int32_t i = (start_idx + count) % active_node_count;
            if (!active_nodes[i].dead && (now - active_nodes[i].last_seen) <= 15) {
                strncpy((char *)resp.node_ips[resp.node_count], (char *)active_nodes[i].ip, 15);
                resp.node_ports[resp.node_count] = active_nodes[i].port;
                resp.node_count++;
            }
        }
        if (active_node_count > 0) rr_offset = (rr_offset + 1) % active_node_count;
        pthread_mutex_unlock(&nodes_mutex);

        net_header_t resp_header = { OP_GET_ACTIVE_NODES, sizeof(resp) };
        send_exact(client_sock, &resp_header, sizeof(resp_header));
        send_exact(client_sock, &resp, sizeof(resp));

    // OP_DELETE_FILE 

    } else if (header.op_code == OP_DELETE_FILE) {
        req_delete_file_t req;
        if (recv_exact(client_sock, &req, sizeof(req)) != 0) { close(client_sock); return; }

        chunk_info_t del_chunks[MAX_CHUNKS];
        int32_t del_count = 0;
        int32_t status = delete_file(req.filename, del_chunks, &del_count);

        // Tell each storage node holding the chunks to delete them
        if (status == 0) {
            for (int32_t i = 0; i < del_count; i++) {
                for (int32_t j = 0; j < del_chunks[i].node_count; j++) {
                    int32_t nsock = connect_to_node(del_chunks[i].node_ips[j], del_chunks[i].node_ports[j]);
                    if (nsock < 0) continue;
                    req_delete_chunk_t dreq = { del_chunks[i].chunk_id };
                    net_header_t dhdr = { OP_DELETE_CHUNK, sizeof(dreq) };
                    send_exact(nsock, &dhdr, sizeof(dhdr));
                    send_exact(nsock, &dreq, sizeof(dreq));
                    // Read ack (ignore errors)
                    net_header_t ack_hdr;
                    resp_delete_chunk_t ack;
                    recv_exact(nsock, &ack_hdr, sizeof(ack_hdr));
                    recv_exact(nsock, &ack, sizeof(ack));
                    close(nsock);
                }
            }
            save_fsimage();
            LOG_INFO("MD", "Deleted file %s (%d chunks cleaned up)\n", req.filename, del_count);
        }

        net_header_t resp_h = { OP_DELETE_FILE, sizeof(int32_t) };
        send_exact(client_sock, &resp_h, sizeof(resp_h));
        send_exact(client_sock, &status, sizeof(status));

    // OP_ALLOCATE_CHUNKS
    } else if (header.op_code == OP_ALLOCATE_CHUNKS) {
        req_allocate_chunks_t req;
        if (recv_exact(client_sock, &req, sizeof(req)) != 0) { close(client_sock); return; }

        int32_t start_chunk_id;
        int32_t status = allocate_chunks(req.count, &start_chunk_id);

        resp_allocate_chunks_t resp  = { status, start_chunk_id };
        net_header_t           resp_hdr = { OP_ALLOCATE_CHUNKS, sizeof(resp) };
        send_exact(client_sock, &resp_hdr, sizeof(resp_hdr));
        send_exact(client_sock, &resp, sizeof(resp));

    // OP_GET_METRICS
    } else if (header.op_code == OP_GET_METRICS) {
        int32_t total_files = 0, total_chunks = 0, target_next_id = 0;
        get_metrics_data(&total_files, &total_chunks, &target_next_id);
        
        int32_t active = 0;
        pthread_mutex_lock(&nodes_mutex);
        for (int32_t i = 0; i < active_node_count; i++) {
            if (!active_nodes[i].dead) active++;
        }
        pthread_mutex_unlock(&nodes_mutex);

        char json_buf[512];
        int json_len = snprintf(json_buf, sizeof(json_buf), 
            "{\n  \"active_nodes\": %d,\n  \"total_files\": %d,\n  \"total_chunks\": %d,\n  \"next_chunk_id\": %d\n}\n",
            active, total_files, total_chunks, target_next_id);

        net_header_t resp_hdr = { OP_GET_METRICS, (uint32_t)json_len };
        send_exact(client_sock, &resp_hdr, sizeof(resp_hdr));
        send_exact(client_sock, json_buf, json_len);

    } else {
        LOG_WARN("MD", "Unknown op_code: %d\n", header.op_code);
    }

    close(client_sock);
}

// Replication monitor thread — detects dead nodes & triggers re-replication

void* replication_monitor(void *arg) {
    int32_t loop_count = 0;
    (void)arg;
    while (1) {
        sleep(30);
        loop_count++;

        time_t now = time(NULL);
        pthread_mutex_lock(&nodes_mutex);

        for (int32_t i = 0; i < active_node_count; i++) {
            if (!active_nodes[i].dead && (now - active_nodes[i].last_seen) > 15) {
                LOG_WARN("MD", "Node %s:%d appears dead — checking for under-replicated chunks\n",
                       active_nodes[i].ip, active_nodes[i].port);
                active_nodes[i].dead = 1;

                // Find a surviving node to copy FROM
                int32_t src_idx = -1;
                for (int32_t s = 0; s < active_node_count; s++) {
                    if (!active_nodes[s].dead && (now - active_nodes[s].last_seen) <= 15) {
                        src_idx = s;
                        break;
                    }
                }
                // Find a target node to copy TO (different from src)
                int32_t dst_idx = -1;
                for (int32_t d = 0; d < active_node_count; d++) {
                    if (d != i && d != src_idx && !active_nodes[d].dead &&
                        (now - active_nodes[d].last_seen) <= 15) {
                        dst_idx = d;
                        break;
                    }
                }

                if (src_idx < 0 || dst_idx < 0) {
                    LOG_ERR("MD", "Not enough living nodes to re-replicate. src=%d dst=%d\n",
                           src_idx, dst_idx);
                    continue;
                }

                // For each chunk the dead node held, trigger replication
                for (int32_t c = 0; c < node_chunk_counts[i]; c++) {
                    int32_t cid = node_chunks[i][c];

                    size_t chunk_sz = get_chunk_size_from_metadata(cid);

                    if (chunk_sz == 0) continue; // unknown chunk, skip

                    // Instruct src to replicate to dst
                    int32_t nsock = connect_to_node(active_nodes[src_idx].ip,
                                                    active_nodes[src_idx].port);
                    if (nsock < 0) {
                        LOG_ERR("MD", "Cannot connect to src node for replication\n");
                        continue;
                    }

                    req_replicate_chunk_t rreq;
                    rreq.chunk_id   = cid;
                    rreq.chunk_size = chunk_sz;
                    strncpy((char *)rreq.src_ip, (char *)active_nodes[src_idx].ip, 15);
                    rreq.src_port   = active_nodes[src_idx].port;
                    strncpy((char *)rreq.dst_ip, (char *)active_nodes[dst_idx].ip, 15);
                    rreq.dst_port   = active_nodes[dst_idx].port;

                    net_header_t rhdr = { OP_REPLICATE_CHUNK, sizeof(rreq) };
                    send_exact(nsock, &rhdr, sizeof(rhdr));
                    send_exact(nsock, &rreq, sizeof(rreq));

                    net_header_t rack_hdr;
                    resp_replicate_chunk_t rack;
                    if (recv_exact(nsock, &rack_hdr, sizeof(rack_hdr)) == 0 &&
                        recv_exact(nsock, &rack, sizeof(rack)) == 0 && rack.status == 0) {
                        LOG_INFO("MD", "Re-replicated chunk %d to %s:%d\n",
                               cid, active_nodes[dst_idx].ip, active_nodes[dst_idx].port);
                        // Update metadata replica list
                        update_chunk_replica(cid, active_nodes[dst_idx].ip,
                                             active_nodes[dst_idx].port);
                    }
                    close(nsock);
                }

                // Persist updated chunk locations
                save_fsimage();
            }
        }

        // Garbage Collection: Delete orphaned chunks

        for (int32_t i = 0; i < active_node_count; i++) {
            if (active_nodes[i].dead) continue;

            for (int32_t c = 0; c < node_chunk_counts[i]; c++) {
                int32_t cid = node_chunks[i][c];
                
                if (is_chunk_orphaned(cid)) {
                    LOG_INFO("MD", "GC: Deleting orphaned chunk %d from node %s:%d\n", 
                           cid, active_nodes[i].ip, active_nodes[i].port);
                           
                    int32_t nsock = connect_to_node(active_nodes[i].ip, active_nodes[i].port);
                    if (nsock >= 0) {
                        req_delete_chunk_t dreq = { cid };
                        net_header_t dhdr = { OP_DELETE_CHUNK, sizeof(dreq) };
                        send_exact(nsock, &dhdr, sizeof(dhdr));
                        send_exact(nsock, &dreq, sizeof(dreq));
                        
                        net_header_t ack_hdr;
                        resp_delete_chunk_t ack;
                        // wait for ack
                        recv_exact(nsock, &ack_hdr, sizeof(ack_hdr));
                        recv_exact(nsock, &ack, sizeof(ack));
                        close(nsock);
                    }
                }
            }
        }

        pthread_mutex_unlock(&nodes_mutex);
        
        if (loop_count % 10 == 0) {
            compact_edit_log();
        }
    }
    return NULL;
}

// main

void *client_thread(void *arg) {
    int32_t client_sock = (int32_t)(intptr_t)arg;
    handle_client(client_sock);
    return NULL;
}

int main(int argc, char *argv[]) {
    const char *config_file = "cdfs.conf";
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--config") == 0 && i + 1 < argc) {
            config_file = argv[++i];
        }
    }
    load_config((const uint8_t *)config_file, &g_config);
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

    address.sin_family      = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port        = htons(g_config.meta_port);

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }

    if (listen(server_fd, 10) < 0) {
        perror("listen");
        exit(EXIT_FAILURE);
    }

    pthread_t monitor_tid;
    pthread_create(&monitor_tid, NULL, replication_monitor, NULL);

    LOG_INFO("MD", "Metadata server listening on port %d\n", g_config.meta_port);
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
