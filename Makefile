CC=gcc
CFLAGS=-Wall -Wextra -D_POSIX_C_SOURCE=200809L -pthread -g -Icommon
BIN_DIR=bin

all: $(BIN_DIR) $(BIN_DIR)/cdfs_client $(BIN_DIR)/cdfs_metadata $(BIN_DIR)/cdfs_storage

$(BIN_DIR):
	mkdir -p $(BIN_DIR)

common/config.o: common/config.c common/config.h
	$(CC) $(CFLAGS) -c common/config.c -o common/config.o

common/serialization.o: common/serialization.c common/serialization.h
	$(CC) $(CFLAGS) -c common/serialization.c -o common/serialization.o

metadata_server/metadata.o: metadata_server/metadata.c
	$(CC) $(CFLAGS) -c metadata_server/metadata.c -o metadata_server/metadata.o

$(BIN_DIR)/cdfs_metadata: common/config.o common/serialization.o metadata_server/metadata.o metadata_server/metadata_server.c
	$(CC) $(CFLAGS) common/config.o common/serialization.o metadata_server/metadata.o metadata_server/metadata_server.c -o $(BIN_DIR)/cdfs_metadata

storage_node/storage.o: storage_node/storage.c
	$(CC) $(CFLAGS) -c storage_node/storage.c -o storage_node/storage.o

$(BIN_DIR)/cdfs_storage: common/config.o common/serialization.o storage_node/storage.o storage_node/storage_node.c
	$(CC) $(CFLAGS) common/config.o common/serialization.o storage_node/storage.o storage_node/storage_node.c -o $(BIN_DIR)/cdfs_storage

$(BIN_DIR)/cdfs_client: common/config.o common/serialization.o client/dfs_client.c main.c
	$(CC) $(CFLAGS) common/config.o common/serialization.o client/dfs_client.c main.c -o $(BIN_DIR)/cdfs_client

clean:
	rm -rf $(BIN_DIR) common/*.o client/*.o metadata_server/*.o storage_node/*.o chunk_*.dat fsimage.dat cdfs_client cdfs_metadata cdfs_storage
