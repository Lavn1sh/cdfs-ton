# Multi-Device Deployment

Deploying CDFS across multiple physical or virtual machines implies separating the Metadata node from the Storage nodes over a real Local Area Network (LAN) or Wide Area Network (WAN).

## Requirements
- Multiple networked Linux machines (e.g., connected over an AWS VPC, Docker bridge, or home lab Ethernet).
- Standard TCP communication paths opened via the respective firewalls.

## Networking

### Machine Roles:
- **Server 1 (`10.0.0.100`)**: Expected to host the `cdfs_metadata` master-record layer.
- **Server 2 (`10.0.0.201`)**: Expected to host Storage Chunk Node 1 (`cdfs_storage` listener).
- **Server 3 (`10.0.0.202`)**: Expected to host Storage Chunk Node 2.

### Step 1: Open Ports

CDFS operates efficiently using unencrypted raw TCP buffers. It defaults to listening on TCP endpoints globally via `INADDR_ANY`. Your IPTables/UFW parameters must allow standard internal subnet traffic dynamically! 
- *Metadata Server*: Needs standard `8080` opened horizontally.
- *Storage Nodes*: Need standard custom ports open (e.g., `8081`).

#### Ubuntu Example Firewall (UFW)
```bash
# On the Metadata API
sudo ufw allow 8080/tcp

# On Storage Nodes 
sudo ufw allow 8081/tcp
```

---

### Step 2: Configure Nodes

CDFS utilizes `cdfs.conf` extensively. Because the Metadata API IP address is no longer `127.0.0.1` statically on independent machines, you MUST declare the proper IP schema.

On **Server 2 & 3 (Storage Layers):**
Update their internal `cdfs.conf` targets.
```ini
# Config file for CDFS connecting back to metadata
meta_ip = 10.0.0.100
meta_port = 8080
storage_dir = ./storage_data
```

### Step 3: Run the Nodes Properly
On **Server 1**:
```bash
# It listens globally immediately on instantiation!
./build/bin/cdfs_metadata
```

On **Servers 2 & 3**:
```bash
# Launch Storage logic and observe the standard replication loops
./build/bin/cdfs_storage 8081
```

### Step 4: Configure Client Software

On **any consumer machine** attached to the network, copy out the binaries compiled from `make` locally along with an environment `cdfs.conf`.

Point `cdfs.conf` statically to the Metadata listener:
```ini
meta_ip = 10.0.0.100
meta_port = 8080
```

Upload a file directly to the network structure!
```bash
$ ./build/bin/cdfs_client put my_database.sql /root_db.sql
{2026} [INFO] Streamed 3 16MB logical nodes successfully!
```
