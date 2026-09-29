#include <bits/stdc++.h>
#include <mutex>
#include <atomic>
#include <condition_variable>
using namespace std;

string peer_ip;
int peer_port = 0;
static const size_t chunkSize = 512 * 1024; 

struct Tracker
{
    string ip;
    int port;
};

vector<Tracker> trackers;
int sock = -1;
int currentTracker = 0;

mutex coutMutex;

map<string, string> filePathMap;
mutex filePathMutex;

struct DownloadTask {
    size_t piece_index;
    string peer_address;
    bool completed;
    bool failed;
};

vector<pair<string, string>> downloads;