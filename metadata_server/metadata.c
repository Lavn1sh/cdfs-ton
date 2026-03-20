#include "dfs.h"
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include "metadata.h"

static file_metadata_t files[MAX_FILES];
static int32_t file_count = 0;

int32_t register_file(const uint8_t *filename, const chunk_info_t* chunks, int32_t chunk_count) {
    
    if(file_count >= MAX_FILES){
        return -1;
    }

    if(!filename || !chunks){
        return -1;
    }

    if(chunk_count < 0 || chunk_count > MAX_CHUNKS){
        return -1;
    }

    for(int32_t i = 0; i < file_count; i++){
        if(strcmp((char *)files[i].filename, (const char *)filename) == 0) return -1;
    }

    file_metadata_t *file = &files[file_count];
    strncpy((char *)file->filename, (const char *)filename, MAX_FILENAME - 1);
    file->filename[MAX_FILENAME - 1] = '\0';

    file->chunk_count = chunk_count;

    for(int32_t i = 0; i < chunk_count; i++){
        file->chunks[i] = chunks[i];
    }
    file_count++;
    save_fsimage();

    return 0;
}

int32_t get_file_metadata(const uint8_t *filename, file_metadata_t *out){
    
    if(!filename || !out){
        return -1;
    }

    for(int32_t i = 0; i < file_count; i++){
        if(strcmp((char *)files[i].filename, (const char *)filename) == 0){
            
            file_metadata_t *src = &files[i];
            
            strncpy((char *)out->filename, (const char *)src->filename, MAX_FILENAME - 1);
            out->filename[MAX_FILENAME - 1] = '\0';
            out->chunk_count = src->chunk_count;
            for (int32_t j = 0; j < src->chunk_count && j < MAX_CHUNKS; j++) {
                out->chunks[j] = src->chunks[j];
            }
            return 0;
        }
    }
    return -1;
}

void save_fsimage(void) {
    FILE *fp = fopen("fsimage.dat", "wb");
    if (!fp) {
        perror("Failed to open fsimage.dat for writing");
        return;
    }
    fwrite(&file_count, sizeof(file_count), 1, fp);
    fwrite(files, sizeof(file_metadata_t), file_count, fp);
    fclose(fp);
}

void load_fsimage(void) {
    FILE *fp = fopen("fsimage.dat", "rb");
    if (!fp) {
        printf("No fsimage.dat found, starting fresh.\n");
        return; // normal on first boot
    }
    if (fread(&file_count, sizeof(file_count), 1, fp) != 1) {
        file_count = 0;
        fclose(fp);
        return;
    }
    if (file_count > MAX_FILES) file_count = MAX_FILES;
    fread(files, sizeof(file_metadata_t), file_count, fp);
    fclose(fp);
    printf("Loaded fsimage.dat containing %d files.\n", file_count);
}
