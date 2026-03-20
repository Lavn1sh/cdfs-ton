CC=gcc
CFLAGS=-Wall -Wextra -D_POSIX_C_SOURCE=200809L -pthread -g -Icommon

all: cdfs_client cdfs_metadata cdfs_storage

common/serialization.o: common/serialization.c common/serialization.h
	$(CC) $(CFLAGS) -c common/serialization.c -o common/serialization.o

metadata_server/metadata.o: metadata_server/metadata.c
	$(CC) $(CFLAGS) -c metadata_server/metadata.c -o metadata_server/metadata.o

cdfs_metadata: common/serialization.o metadata_server/metadata.o metadata_server/metadata_server.c
	$(CC) $(CFLAGS) common/serialization.o metadata_server/metadata.o metadata_server/metadata_server.c -o cdfs_metadata

storage_node/storage.o: storage_node/storage.c
	$(CC) $(CFLAGS) -c storage_node/storage.c -o storage_node/storage.o

cdfs_storage: common/serialization.o storage_node/storage.o storage_node/storage_node.c
	$(CC) $(CFLAGS) common/serialization.o storage_node/storage.o storage_node/storage_node.c -o cdfs_storage

cdfs_client: common/serialization.o client/dfs_client.c main.c
	$(CC) $(CFLAGS) common/serialization.o client/dfs_client.c main.c -o cdfs_client

clean:
	rm -f cdfs_client cdfs_metadata cdfs_storage common/*.o client/*.o metadata_server/*.o storage_node/*.o chunk_*.dat
