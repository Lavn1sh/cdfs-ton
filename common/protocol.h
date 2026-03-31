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
    OP_LIST_FILES    = 7,
    OP_BLOCK_REPORT  = 8,  // Storage node -> MD: list of chunk IDs held
    OP_REPLICATE_CHUNK = 9, // MD -> Storage node: copy chunk from src to dst
    OP_DELETE_FILE   = 10, // Client -> MD: remove file and its chunks
    OP_DELETE_CHUNK  = 11, // MD -> Storage node: delete a specific chunk
    OP_ALLOCATE_CHUNKS = 12, // Client -> MD: request unused chunk IDs
    OP_GET_METRICS   = 13, // Client -> MD: request metrics JSON
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

// OP_ALLOCATE_CHUNKS
typedef struct {
    int32_t count;
} req_allocate_chunks_t;

typedef struct {
    int32_t status;
    int32_t start_chunk_id; // returns a sequential block of `count` chunk IDs
} resp_allocate_chunks_t;

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

// OP_LIST_FILES
typedef struct {
    uint8_t directory[MAX_FILENAME];
} req_list_files_t;

typedef struct {
    int32_t file_count;
    // Followed by `file_count` strings of MAX_FILENAME length
} resp_list_files_t;


// OP_BLOCK_REPORT
typedef struct {
    int32_t storage_port;    // Port this node listens on
    int32_t chunk_count;
    int32_t chunk_ids[MAX_CHUNKS];
} req_block_report_t;

// OP_REPLICATE_CHUNK: MD instructs a storage node to fetch & store a chunk
typedef struct {
    int32_t chunk_id;
    size_t  chunk_size;
    uint8_t src_ip[16];  // Where to fetch the chunk from
    int32_t src_port;
    uint8_t dst_ip[16];  // Where to push the copy to
    int32_t dst_port;
} req_replicate_chunk_t;

typedef struct {
    int32_t status;
} resp_replicate_chunk_t;

// OP_DELETE_FILE
typedef struct {
    uint8_t filename[MAX_FILENAME];
} req_delete_file_t;

// OP_DELETE_CHUNK (MD -> Storage node)
typedef struct {
    int32_t chunk_id;
} req_delete_chunk_t;

typedef struct {
    int32_t status;
} resp_delete_chunk_t;

#endif
