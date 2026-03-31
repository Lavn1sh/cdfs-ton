# Architecture & Design Notes

This document describes the high-level architecture of the C Distributed File System (`CDFS`), roughly modeled after production paradigms like HDFS and GFS but built entirely in ISO C.

## 1. System Components 
CDFS separates duties between a solitary **Metadata Node** and horizontal **Storage Nodes**.

### Metadata Server (`cdfs_metadata`)
The "brain" of the filesystem layer. 
- Serves as the primary master namespace map (maintains all filename -> node pointer references).
- Handles chunk ID distributions atomically (protects `__next_chunk_id__` centrally across nodes securely utilizing POSIX mutices). 
- Responsible natively for block operations (listing arrays, registering uploaded blobs recursively, triggering asynchronous cluster GC).

### Storage Nodes (`cdfs_storage`)
The "muscles" of the system processing streaming binary segments concurrently! 
- Independent daemons listening individually. They persist unformatted binary `.dat` blobs of logical information directly to their configured mount point directory (`storage_data/`).
- Stream block-by-block buffers directly through Linux socket descriptors completely avoiding volatile RAM loading. 
- Report state internally by submitting TCP `OP_HEARTBEAT`/`OP_BLOCK_REPORT` intervals automatically.

### Client Wrapper (`cdfs_client`)
Provides users a POSIX-esque wrapper command tool over the cluster. Calculates Checksums dynamically on `put` streams, pushes replicas intelligently onto concurrent node arrays dynamically chosen by `cdfs_metadata` load requests!

## 2. Replication Mechanism
To protect arrays against data drops natively resulting from hardware node failures:
- The `cdfs_client` caches target nodes internally using configuration maps returned dynamically under `OP_GET_ACTIVE_NODES`.
- While writing logically the client multiplexes sequentially streams of `16MB` blocks over active socket lists concurrently pushing to three disparate nodes simultaneously!
- If the master connection tracker (`cdfs_metadata -> replication_monitor()`) spots a missing chunk, it sends `OP_REPLICATE_CHUNK` forcing an isolated local clone.

## 3. Persistent State 
- The master namespace tracks logical directories in volatile memory. 
- All metadata modifications output to `edits.log` continuously (`W-A-L pattern`), permitting fast boot loading routines to safely ingest states retroactively instantly upon a power loss string. 
- To avoid massive bloat, an asynchronous trigger periodically collapses `edits.log` linearly over native configurations into a unified `fsimage.dat` payload.

## 4. Concurrent Thread Model
Each subcomponent utilizes native concurrency!
- `pthread_create()` isolates all networking handlers allowing non-blocking I/O.
- Variables like global active lists and cluster maps leverage robust POSIX Mutex implementations natively guarding memory maps asynchronously!
