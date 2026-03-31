CC=gcc
CFLAGS=-Wall -Wextra -D_POSIX_C_SOURCE=200809L -pthread -g -Icommon
BUILD_DIR=build
BIN_DIR=$(BUILD_DIR)/bin

# Create a list of all object files we need
COMMON_OBS = $(BUILD_DIR)/common/config.o $(BUILD_DIR)/common/serialization.o $(BUILD_DIR)/common/log.o
META_OBS = $(BUILD_DIR)/metadata_server/metadata.o
STORAGE_OBS = $(BUILD_DIR)/storage_node/storage.o
CLIENT_OBS = $(BUILD_DIR)/client/dfs_client.o $(BUILD_DIR)/main.o

all: $(BUILD_DIR) $(BIN_DIR) $(BIN_DIR)/cdfs_client $(BIN_DIR)/cdfs_metadata $(BIN_DIR)/cdfs_storage

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)/common $(BUILD_DIR)/client $(BUILD_DIR)/metadata_server $(BUILD_DIR)/storage_node

$(BIN_DIR):
	mkdir -p $(BIN_DIR)

# --- Common objects ---
$(BUILD_DIR)/common/config.o: common/config.c common/config.h
	$(CC) $(CFLAGS) -c common/config.c -o $@

$(BUILD_DIR)/common/serialization.o: common/serialization.c common/serialization.h
	$(CC) $(CFLAGS) -c common/serialization.c -o $@

$(BUILD_DIR)/common/log.o: common/log.c common/log.h
	$(CC) $(CFLAGS) -c common/log.c -o $@

# --- Metadata server ---
$(BUILD_DIR)/metadata_server/metadata.o: metadata_server/metadata.c metadata_server/metadata.h
	$(CC) $(CFLAGS) -c metadata_server/metadata.c -o $@

$(BIN_DIR)/cdfs_metadata: $(COMMON_OBS) $(META_OBS) metadata_server/metadata_server.c
	$(CC) $(CFLAGS) $(COMMON_OBS) $(META_OBS) metadata_server/metadata_server.c -o $@

# --- Storage node ---
$(BUILD_DIR)/storage_node/storage.o: storage_node/storage.c
	$(CC) $(CFLAGS) -c storage_node/storage.c -o $@

$(BIN_DIR)/cdfs_storage: $(COMMON_OBS) $(STORAGE_OBS) storage_node/storage_node.c
	$(CC) $(CFLAGS) $(COMMON_OBS) $(STORAGE_OBS) storage_node/storage_node.c -o $@

# --- Client ---
$(BUILD_DIR)/client/dfs_client.o: client/dfs_client.c
	$(CC) $(CFLAGS) -c client/dfs_client.c -o $@

$(BUILD_DIR)/main.o: main.c
	$(CC) $(CFLAGS) -c main.c -o $@

$(BIN_DIR)/cdfs_client: $(COMMON_OBS) $(CLIENT_OBS)
	$(CC) $(CFLAGS) $(COMMON_OBS) $(CLIENT_OBS) -o $@

clean:
	rm -rf $(BUILD_DIR) \
	       chunk_*.dat chunk_*.crc \
	       edits.log fsimage.dat \
	       storage_data/
