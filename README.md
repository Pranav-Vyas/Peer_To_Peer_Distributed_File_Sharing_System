
# Peer-2-Peer File Sharing

This assignment is an implementation of a Peer to peer file sharing system using c++ and system calls.  
Peers can upload, download, and share files within groups while a tracker coordinates metadata like group ownership, file hashes, and peer lists.
The design is inspired by **BitTorrent**, with simplified assumptions:
- A peer becomes a seeder **only when it has the complete file**.
- Round-robin piece selection strategy is used for parallel downloading.
- Multiple peers can serve file pieces concurrently to maximize throughput.

## Functionalities Implemented
- File upload with chunk-level SHA-1 integrity.
- File download with verification against tracker metadata.
- Peer becomes a seeder after download.
- Join and Leave groups (join_group, leave_group).
- Tracker assigns new owner if current owner leaves group.
- Listing files and peers in a group.

## Commands Usage
- create_user <user id> <password> - Register a new user account
- login <user id> <password> - Authenticate and start a session
- create_group <group id> - Create a new group (user becomes owner)
- join_group <group id> - Request to join an existing group
- leave_group <group id> - Leave a group you’re a member of
- list_groups - Display all available groups in the system
- list_requests <group id> - Show pending join requests (owner only)
- accept_request <group id> <user id> - Accept a join request (owner only)
- logout - End current session and stop sharing files
- upload_file <group_id> <file_path>
- list_files <group_id>
- download_file <group_id> <filename> <destination_path>
- show_downloads
- stop_share <group_id> <filename>


### Prerequisites
- **Linux/Unix** system
- **g++ (C++17 or later)**
- OpenSSL (for SHA-1 hashing)

## Implementation Approaches
- Trackers handle multiple clients simultaneously using threads
- Data is stored in-memory. Sync messages ensure changes are propagated immediately and consistently, without disk I/O overhead.
- Each client runs independently.
- Tracker Maintains metadata about users, groups, files, owners, and piece hashes.
- Client uploads its own files by computing piece hashes and registering with the tracker.
- Client downloads files by fetching peer list from tracker, then requesting chunks from peers.
- After download, peer registers itself as seeder with tracker.

## Key Algorithms & Concepts
1. File Chunking & Hashing
    - File is divided into fixed-size chunks (chunkSize).
    - Each chunk is hashed using SHA-1.
    - Tracker stores:
    - File hash (global SHA-1 of file).
    - Piece hashes (per-chunk SHA-1).
    - File owners.
2. Piece Selection Algorithm: Round robin

## Data Structures Used
```
struct Tracker
{
    string ip;
    int port;
};

vector<Tracker> trackers;

struct DownloadTask {
    size_t piece_index;
    string peer_address;
    bool completed;
    bool failed;
};

vector<pair<string, string>> downloads;

struct User
{
    string password;
    bool loggedIn;
};

struct Group
{
    string owner;
    set<string> members;
    set<string> pendingRequests;
};

struct FileInfo {
    string group;
    string filename;
    string file_hash;
    uint64_t file_size = 0;
    size_t num_pieces = 0;
    vector<string> piece_hashes; 
    set<string> owners;         
};

map<string, FileInfo> files; 
map<string, User> users;
map<string, Group> groups;
```

## Assumptions
- Tracker must run before any client joins.
- Peers get tracker's IP addresses from a file - tracker_info.txt.
- Files remain unchanged after upload; modification is not supported.
- File transfers are done over TCP sockets.
- Chunks are fixed size (512 KB).

## Limitations
- No synchronization for upload and download.
- Tracker is a single point of failure.

## File Structure
  2025202015_A3_Interim  
    |_ makefile  
    |_ tracker/  
    &nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;|_ commandUtils.cpp  
    &nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;|_ commandUtils.h  
    &nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;|_ utils.cpp  
    &nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;|_ utils.h  
    &nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;|_ data_structure.cpp  
    &nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;|_ data_structure.h  
    &nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;|_ main.cpp  
    &nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;|_ main.h  
    &nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;|_ performCommand.cpp  
    &nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;|_ performCommand.h  
    &nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;|_ sync.cpp  
    &nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;|_ sync.h  
|_ client/  
&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;|_ client.cpp
&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;|_ clientGlobalVariables.h
&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;|_ clientUtils.h
|_ README.md    
|_ tracker_info.txt

#### Note:
All files are compiled together using the provided `makefile`.

## Compilation and Execution

```bash
make          
./tracker_app tracker_info.txt 1
./tracker_app tracker_info.txt 2     
./client_app <IP>:<PORT> tracker info.txt
```

###### Author: Pranav Vyas
###### Project: P2P File Sharing (BitTorrent-inspired)
###### Tech: C++, Sockets, Multithreading, SHA-1





