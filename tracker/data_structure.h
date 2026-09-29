#pragma once
#include <string>
#include <map>
#include <set>
#include <mutex>
#include <vector>
using namespace std;

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
    vector<string> piece_hashes; // hex strings
    set<string> owners;          // store owner tokens like "alice:6001"
};

extern map<string, FileInfo> files; // keyed by file_hash
extern mutex filesMutex;
extern map<string, User> users;
extern map<string, Group> groups;
extern string peerIp;
extern int peerPort;
extern bool syncingFromPeer;
