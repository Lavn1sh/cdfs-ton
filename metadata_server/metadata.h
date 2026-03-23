#ifndef METADATA_H
#define METADATA_H

void load_fsimage(void);
void save_fsimage(void);
int32_t list_files_in_dir(const uint8_t *dir, uint8_t (*out_files)[MAX_FILENAME]);

#endif
