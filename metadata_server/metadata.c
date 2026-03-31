#include "dfs.h"
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <pthread.h>
#include "metadata.h"
#include "../common/log.h"

static file_metadata_t files[MAX_FILES];
static int32_t file_count = 0;
static int32_t next_chunk_id = 1;

// Mutex protecting 'files' and 'file_count'
static pthread_mutex_t namespace_mutex = PTHREAD_MUTEX_INITIALIZER;

void append_edit_log(file_metadata_t *file, uint8_t op) {
    FILE *fp = fopen("edits.log", "ab");
    if (fp) {
        fwrite(&op, sizeof(op), 1, fp);
        fwrite(file, sizeof(file_metadata_t), 1, fp);
        fclose(fp);
    }
}

void append_allocate_log(int32_t new_next_chunk_id) {
    FILE *fp = fopen("edits.log", "ab");
    if (fp) {
        uint8_t op = 3;
        fwrite(&op, sizeof(op), 1, fp);
        fwrite(&new_next_chunk_id, sizeof(new_next_chunk_id), 1, fp);
        fclose(fp);
    }
}

int32_t allocate_chunks(int32_t count, int32_t *out_start_chunk_id) {
    if (count <= 0) return -1;
    pthread_mutex_lock(&namespace_mutex);
    *out_start_chunk_id = next_chunk_id;
    next_chunk_id += count;
    append_allocate_log(next_chunk_id);
    pthread_mutex_unlock(&namespace_mutex);
    return 0;
}

int32_t register_file(const uint8_t *filename, const chunk_info_t* chunks, int32_t chunk_count) {
    if(!filename || !chunks) return -1;
    if(chunk_count < 0 || chunk_count > MAX_CHUNKS) return -1;

    pthread_mutex_lock(&namespace_mutex);

    if(file_count >= MAX_FILES){
        pthread_mutex_unlock(&namespace_mutex);
        return -1;
    }

    for(int32_t i = 0; i < file_count; i++){
        if(strcmp((char *)files[i].filename, (const char *)filename) == 0) {
            pthread_mutex_unlock(&namespace_mutex);
            return -1;
        }
    }

    file_metadata_t *file = &files[file_count];
    strncpy((char *)file->filename, (const char *)filename, MAX_FILENAME - 1);
    file->filename[MAX_FILENAME - 1] = '\0';

    file->chunk_count = chunk_count;

    for(int32_t i = 0; i < chunk_count; i++){
        file->chunks[i] = chunks[i];
    }
    file_count++;
    
    append_edit_log(file, 1); // op=1 means PUT

    pthread_mutex_unlock(&namespace_mutex);
    return 0;
}

int32_t get_file_metadata(const uint8_t *filename, file_metadata_t *out){
    if(!filename || !out) return -1;

    pthread_mutex_lock(&namespace_mutex);

    for(int32_t i = 0; i < file_count; i++){
        if(strcmp((char *)files[i].filename, (const char *)filename) == 0){
            
            file_metadata_t *src = &files[i];
            
            strncpy((char *)out->filename, (const char *)src->filename, MAX_FILENAME - 1);
            out->filename[MAX_FILENAME - 1] = '\0';
            out->chunk_count = src->chunk_count;
            for (int32_t j = 0; j < src->chunk_count && j < MAX_CHUNKS; j++) {
                out->chunks[j] = src->chunks[j];
            }
            pthread_mutex_unlock(&namespace_mutex);
            return 0;
        }
    }
    pthread_mutex_unlock(&namespace_mutex);
    return -1;
}

int32_t list_files_in_dir(const uint8_t *dir, uint8_t (*out_files)[MAX_FILENAME]) {
    int32_t count = 0;
    size_t dirlen = strlen((const char *)dir);

    pthread_mutex_lock(&namespace_mutex);
    for (int32_t i = 0; i < file_count; i++) {
        if (strncmp((const char *)files[i].filename, (const char *)dir, dirlen) == 0) {
            strncpy((char *)out_files[count], (const char *)files[i].filename, MAX_FILENAME - 1);
            out_files[count][MAX_FILENAME - 1] = '\0';
            count++;
        }
    }
    pthread_mutex_unlock(&namespace_mutex);
    return count;
}

/* Delete a file record. Fills out_chunks/out_count so the caller can
 * instruct storage nodes to clean up the actual chunk files.
 * Returns 0 on success, -1 if not found. */
int32_t delete_file(const uint8_t *filename, chunk_info_t *out_chunks, int32_t *out_count) {
    pthread_mutex_lock(&namespace_mutex);
    for (int32_t i = 0; i < file_count; i++) {
        if (strcmp((char *)files[i].filename, (const char *)filename) == 0) {
            // Copy chunk info for the caller
            if (out_chunks && out_count) {
                *out_count = files[i].chunk_count;
                for (int32_t j = 0; j < files[i].chunk_count; j++) {
                    out_chunks[j] = files[i].chunks[j];
                }
            }
            // Log deletion (op=2 means DELETE)
            append_edit_log(&files[i], 2);
            // Shift remaining entries
            for (int32_t j = i; j < file_count - 1; j++) {
                files[j] = files[j + 1];
            }
            file_count--;
            pthread_mutex_unlock(&namespace_mutex);
            return 0;
        }
    }
    pthread_mutex_unlock(&namespace_mutex);
    return -1;
}

/* Add a new replica location to every chunk matching chunk_id across all files. */
void update_chunk_replica(int32_t chunk_id, const uint8_t *new_ip, int32_t new_port) {
    pthread_mutex_lock(&namespace_mutex);
    for (int32_t i = 0; i < file_count; i++) {
        for (int32_t j = 0; j < files[i].chunk_count; j++) {
            if (files[i].chunks[j].chunk_id == chunk_id) {
                chunk_info_t *ci = &files[i].chunks[j];
                if (ci->node_count < REPLICATION_FACTOR) {
                    strncpy((char *)ci->node_ips[ci->node_count], (const char *)new_ip, 15);
                    ci->node_ips[ci->node_count][15] = '\0';
                    ci->node_ports[ci->node_count] = new_port;
                    ci->node_count++;
                }
            }
        }
    }
    pthread_mutex_unlock(&namespace_mutex);
}

/* Used by the replication monitor to safely find the chunk size. */
size_t get_chunk_size_from_metadata(int32_t chunk_id) {
    size_t chunk_sz = 0;
    pthread_mutex_lock(&namespace_mutex);
    for (int32_t fi = 0; fi < file_count && chunk_sz == 0; fi++) {
        for (int32_t ci = 0; ci < files[fi].chunk_count; ci++) {
            if (files[fi].chunks[ci].chunk_id == chunk_id) {
                chunk_sz = files[fi].chunks[ci].chunk_size;
                break;
            }
        }
    }
    pthread_mutex_unlock(&namespace_mutex);
    return chunk_sz;
}

int32_t is_chunk_orphaned(int32_t chunk_id) {
    pthread_mutex_lock(&namespace_mutex);
    
    // Check if it's in the namespace
    for (int32_t fi = 0; fi < file_count; fi++) {
        for (int32_t ci = 0; ci < files[fi].chunk_count; ci++) {
            if (files[fi].chunks[ci].chunk_id == chunk_id) {
                pthread_mutex_unlock(&namespace_mutex);
                return 0; // Not an orphan
            }
        }
    }
    
    // Check if it was recently allocated (heuristic: within the last 500 allocations)
    if (chunk_id >= next_chunk_id - 500) {
        pthread_mutex_unlock(&namespace_mutex);
        return 0; // Possibly still being uploaded, immune to GC
    }
    
    pthread_mutex_unlock(&namespace_mutex);
    return 1; // Orphan
}

void get_metrics_data(int32_t *out_files, int32_t *out_chunks, int32_t *out_next_chunk_id) {
    pthread_mutex_lock(&namespace_mutex);
    *out_files = file_count;
    *out_next_chunk_id = next_chunk_id;
    int32_t chunk_count = 0;
    for (int32_t i = 0; i < file_count; i++) {
        chunk_count += files[i].chunk_count;
    }
    *out_chunks = chunk_count;
    pthread_mutex_unlock(&namespace_mutex);
}

void save_fsimage(void) {
    pthread_mutex_lock(&namespace_mutex);
    FILE *fp = fopen("fsimage.dat", "wb");
    if (!fp) {
        perror("Failed to open fsimage.dat for writing");
        pthread_mutex_unlock(&namespace_mutex);
        return;
    }
    fwrite(&file_count, sizeof(file_count), 1, fp);
    fwrite(&next_chunk_id, sizeof(next_chunk_id), 1, fp);
    fwrite(files, sizeof(file_metadata_t), file_count, fp);
    fclose(fp);
    pthread_mutex_unlock(&namespace_mutex);
}

void compact_edit_log(void) {
    LOG_INFO("MD", "Starting metadata compaction...\n");
    // 1. Save fsimage (which captures current namespace including all edits)
    save_fsimage();
    
    // 2. Truncate edits.log to 0 since everything is now safely in fsimage
    pthread_mutex_lock(&namespace_mutex);
    FILE *fp = fopen("edits.log", "wb");
    if (fp) {
        fclose(fp);
        LOG_INFO("MD", "Compacted edits.log successfully.\n");
    } else {
        LOG_ERR("MD", "Failed to truncate edits.log during compaction.\n");
    }
    pthread_mutex_unlock(&namespace_mutex);
}

void load_fsimage(void) {
    pthread_mutex_lock(&namespace_mutex);
    FILE *fp = fopen("fsimage.dat", "rb");
    if (!fp) {
        LOG_INFO("MD", "No fsimage.dat found, starting fresh.\n");
        pthread_mutex_unlock(&namespace_mutex);
        return; // normal on first boot
    }
    if (fread(&file_count, sizeof(file_count), 1, fp) != 1) {
        file_count = 0;
        fclose(fp);
        pthread_mutex_unlock(&namespace_mutex);
        return;
    }
    if (fread(&next_chunk_id, sizeof(next_chunk_id), 1, fp) != 1) {
        next_chunk_id = 1;
    }
    if (file_count > MAX_FILES) file_count = MAX_FILES;
    fread(files, sizeof(file_metadata_t), file_count, fp);
    fclose(fp);
    LOG_INFO("MD", "Loaded fsimage.dat containing %d files. Next chunk ID: %d\n", file_count, next_chunk_id);
    
    FILE *edits = fopen("edits.log", "rb");
    if (edits) {
        file_metadata_t file;
        uint8_t op;
        int32_t puts_loaded = 0, dels_loaded = 0;
        while (fread(&op, sizeof(op), 1, edits) == 1) {
            if (op == 3) {
                // ALLOCATE
                int32_t saved_next_id;
                if (fread(&saved_next_id, sizeof(saved_next_id), 1, edits) == 1) {
                    next_chunk_id = saved_next_id;
                }
            } else if (fread(&file, sizeof(file_metadata_t), 1, edits) == 1) {
                if (op == 1) {
                    // PUT: register if not already present
                    int32_t dup = 0;
                    for (int32_t i = 0; i < file_count; i++) {
                        if (strcmp((char *)files[i].filename, (char *)file.filename) == 0) {
                            dup = 1; break;
                        }
                    }
                    if (!dup && file_count < MAX_FILES) {
                        files[file_count++] = file;
                        puts_loaded++;
                    }
                } else if (op == 2) {
                    // DELETE: remove if present
                    for (int32_t i = 0; i < file_count; i++) {
                        if (strcmp((char *)files[i].filename, (char *)file.filename) == 0) {
                            for (int32_t j = i; j < file_count - 1; j++) files[j] = files[j+1];
                            file_count--;
                            dels_loaded++;
                            break;
                        }
                    }
                }
            }
        }
        fclose(edits);
        LOG_INFO("MD", "Replayed edit log: +%d puts, -%d deletes. Next chunk ID: %d\n", puts_loaded, dels_loaded, next_chunk_id);
    }
    pthread_mutex_unlock(&namespace_mutex);
}
