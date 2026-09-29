#include "data_structure.h"
using namespace std;

map<string, User> users;
map<string, Group> groups;
string peerIp;
int peerPort = 0;
bool syncingFromPeer = false;
map<string, FileInfo> files;
mutex filesMutex;