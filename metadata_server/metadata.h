#ifndef METADATA_H
#define METADATA_H

#include "../common/dfs.h"
#include <stdint.h>
#include <stddef.h>

void     load_fsimage(void);
void     save_fsimage(void);
int32_t  list_files_in_dir(const uint8_t *dir, uint8_t (*out_files)[MAX_FILENAME]);
int32_t  delete_file(const uint8_t *filename, chunk_info_t *out_chunks, int32_t *out_count);
void     update_chunk_replica(int32_t chunk_id, const uint8_t *new_ip, int32_t new_port);
size_t   get_chunk_size_from_metadata(int32_t chunk_id);
int32_t  allocate_chunks(int32_t count, int32_t *out_start_chunk_id);
int32_t  is_chunk_orphaned(int32_t chunk_id);
void     compact_edit_log(void);
void     get_metrics_data(int32_t *out_files, int32_t *out_chunks, int32_t *out_next_chunk_id);

#endif
