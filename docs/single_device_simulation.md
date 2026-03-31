# Cluster Simulation (Single Machine)

It is highly advantageous to test the fault-tolerance, chunk distribution, and replication capacities of CDFS locally before deploying it across an array of cloud instances.

Because CDFS employs **independent, detached `pthread` execution patterns**, a multi-node cluster can trivially sit behind several network ports on `127.0.0.1` locally with zero modification to the codebase!

## Booting a Mock Cluster

### 1. Launch Metadata
The central authority must boot first on the root configuration constraints.
```bash
./build/bin/cdfs_metadata
```

### 2. Launch Storage Ring
Storage nodes register sequentially to the active node listener loop running in `cdfs_metadata` via unique ports on `localhost`. Because each node stores 16MB `.dat` buffers directly in `--storage-dir`, you MUST configure each node sequentially using a different folder parameter when initializing them.

Instead of hardcoding, CDFS honors custom configuration files. 

Let's boot 3 virtual shards! For simplicity, generate independent `.conf` files:
```bash
# Node 1
echo -e "meta_ip=127.0.0.1\nmeta_port=8080\nstorage_dir=./data_1\n" > cfg_1.conf
# Node 2
echo -e "meta_ip=127.0.0.1\nmeta_port=8080\nstorage_dir=./data_2\n" > cfg_2.conf
# Node 3
echo -e "meta_ip=127.0.0.1\nmeta_port=8080\nstorage_dir=./data_3\n" > cfg_3.conf
```

Start the engines concurrently with unique TCP bind ports:
```bash
# Terminal 1
./build/bin/cdfs_storage 8081 --config cfg_1.conf
# Terminal 2
./build/bin/cdfs_storage 8082 --config cfg_2.conf
# Terminal 3
./build/bin/cdfs_storage 8083 --config cfg_3.conf
```
*Note: Make sure your implementation of `main` passes `--config` otherwise the default `cdfs.conf` is used; alternatively, copy directories or configure environments according to your build structure.*

*(If `cdfs_storage` does not accept a `--config` argument in your variant, simply spawn each binary from different subdirectories on the local machine each containing an isolated `cdfs.conf` and `storage_data/`!)*

### 3. Verify Heartbeats
Monitor the output in the Metadata Server `stdout`:
```text
[2026-03-31T08:12:35] [INFO ] [MD] Registered new storage node 127.0.0.1:8081
[2026-03-31T08:12:38] [INFO ] [MD] Registered new storage node 127.0.0.1:8082
[2026-03-31T08:12:44] [INFO ] [MD] Registered new storage node 127.0.0.1:8083
```

Check telemetry natively via your single client:
```bash
$ ./build/bin/cdfs_client status
{
  "active_nodes": 3,
  ...
}
```

## Simulating Disasters
- **Crash Testing**: Send a `CTRL+C` kill signal to one of your 3 terminals.
- **Auto-Replication Tracking**: Observe `metadata_server` output during its next standard `replication_monitor` daemon loop (roughly 30s-300s configurable). It will trace broken checksums/orphaned files globally and re-clone the dead node's shards horizontally over any remaining active clusters!
