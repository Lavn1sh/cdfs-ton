# CDFS (C Distributed File System)

CDFS is a lightweight, high-performance distributed file system written in C. Designed with an architecture similar to HDFS/GFS, it focuses on fault-tolerance, multi-threaded high-concurrency request handling, structured data replication, and real-time operational telemetry.

## Features
- **High Concurrency:** Utilizes detached `pthread` implementations on both Metadata and Storage nodes.
- **Large Chunk Streaming:** Supports production-grade data fragmentation (16MB chunks) dynamically streamed via small socket buffers to prevent stack overflow.
- **Auto-Garbage Collection & Editing:** Unused/orphaned segments are gracefully collected, and write logging is smartly compacted in the background periodically.
- **Real-time Diagnostics:** Provides telemetry APIs querying cluster health locally.

## Documentation Index

Please consult the following manuals in the `docs/` directory for detailed setup instructions and architectural overviews:

- [Setup & Basic Usage](docs/setup.md)
  *How to configure `cdfs.conf`, compile the client/node binaries, and immediately execute file transactions locally.*
- [Cluster Simulation (Single Machine)](docs/single_device_simulation.md)
  *Learn how to spin up a mock distributed environment (1 Metadata Server & N Storage Nodes) entirely onto your local workstation using multiple ports.*
- [Multi-Device Deployment](docs/multi_device.md)
  *How to configure and secure node communication across entirely different physical Linux machines on a network.*
- [Architecture & Design Notes](docs/architecture.md)
  *A high-level view of how chunks, replication, thread-locking, streaming, and metadata work.*

## Quick Start
```bash
# Clean and compile entirely isolated binaries
make clean && make

# Edit configuration
nano cdfs.conf 

# Launch components manually
./build/bin/cdfs_metadata
./build/bin/cdfs_storage 8081
```
