#ifndef PROTOCOL_H
#define PROTOCOL_H

#include "dfs.h"
#include <stdint.h>

// Operation codes
typedef enum {
    OP_REGISTER_FILE = 1,
    OP_GET_METADATA  = 2,
    OP_STORE_CHUNK   = 3,
    OP_LOAD_CHUNK    = 4,
    OP_HEARTBEAT     = 5,
    OP_GET_ACTIVE_NODES = 6,
    OP_ERROR         = 99
} op_code_t;

// Common request/response header
typedef struct {
    uint32_t op_code;
    uint32_t payload_size;
} net_header_t;

// --- Payload Structures (sent after the header) ---

// OP_REGISTER_FILE
typedef struct {
    uint8_t filename[MAX_FILENAME];
    int32_t chunk_count;
    // Followed by an array of `chunk_count` chunk_info_t structs
} req_register_file_t;

// OP_GET_METADATA
typedef struct {
    uint8_t filename[MAX_FILENAME];
} req_get_metadata_t;

typedef struct {
    int32_t status; // 0 for success, -1 for error
    int32_t chunk_count;
    // Followed by an array of `chunk_count` chunk_info_t structs
} resp_get_metadata_t;

// OP_STORE_CHUNK
typedef struct {
    int32_t chunk_id;
    size_t size;
    uint32_t checksum;
    // Followed by exactly `size` bytes of raw chunk data
} req_store_chunk_t;

typedef struct {
    int32_t status; // Returned from storage node, 0 on success, -1 on error
} resp_store_chunk_t;

// OP_LOAD_CHUNK
typedef struct {
    int32_t chunk_id;
    size_t size;
} req_load_chunk_t;

typedef struct {
    int32_t status;
    size_t size;
    uint32_t checksum;
    // Followed by exactly `size` bytes of raw chunk data
} resp_load_chunk_t;

// OP_HEARTBEAT
typedef struct {
    int32_t storage_port; // the port this storage node is listening on
    int32_t available_space_mb;
} req_heartbeat_t;

// OP_GET_ACTIVE_NODES
typedef struct {
    uint8_t dummy; // To satisfy sizeof() in client payload
} req_get_active_nodes_t;

typedef struct {
    int32_t node_count;
    uint8_t node_ips[REPLICATION_FACTOR][16];
    int32_t node_ports[REPLICATION_FACTOR];
} resp_get_active_nodes_t;

#endif
