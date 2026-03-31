# Setup & Basic Usage

This manual details how to fully configure, build, and use the C Distributed File System (`CDFS`).

## 1. Prerequisites
- Linux OS
- GCC Compiler (with POSIX threads)
- Make 

## 2. Compilation
To build the binaries neatly (storing object files safely inside isolated `/build` directives and executable software under `/bin`):
```bash
make clean
make
```

After building, three CLI binaries will rest in the `./build/bin` sub-directory:
1. `cdfs_metadata`
2. `cdfs_storage`
3. `cdfs_client`

## 3. Configuration (`cdfs.conf`)
Every active node parses configuration constraints from `cdfs.conf`. You must ensure this file sits alongside the invocation context (for convenience, run the files from the project root!).

Example config:
```ini
meta_ip = 127.0.0.1
meta_port = 8080
storage_dir = ./storage_data
```

## 4. Run CDFS

Start the Metadata engine:
```bash
./build/bin/cdfs_metadata
```

Start the primary Storage node:
```bash
# Provide custom listening port!
./build/bin/cdfs_storage 8081
```

## 5. File System Operations

Interact with the data block layers using the `cdfs_client`:

**Check Server Cluster Health:**
```bash
./build/bin/cdfs_client status
```

**Upload a document (Client -> CDFS):**
```bash
./build/bin/cdfs_client put /tmp/local_image.png /cdfs/cluster_image.png
```

**Retrieve a document (CDFS -> Local Node):**
```bash
./build/bin/cdfs_client get /cdfs/cluster_image.png /tmp/restored_image.png
```

**List files:**
```bash
./build/bin/cdfs_client ls /
```

**Delete file & drop cluster allocations:**
```bash
./build/bin/cdfs_client rm /cdfs/cluster_image.png
```
