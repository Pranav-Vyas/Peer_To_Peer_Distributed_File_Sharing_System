#include "sync.h"
#include "data_structure.h" // for users, groups, etc.
#include "utils.h"          // if you have splitCommand()
#include <iostream>
#include <vector>
#include <map>
#include <set>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
using namespace std;

// ---------------- Handle sync ----------------
void handleSyncMessage(const string &msg)
{
    vector<string> tokens = split(msg);
    if (tokens.size() < 2)
        return;

    string type = tokens[0];
    if (type != "SYNC")
        return;

    string action = tokens[1];

    if (action == "USER_CREATE" && tokens.size() >= 4)
    {
        string userId = tokens[2];
        string password = tokens[3];
        users[userId] = {password, false};
    }
    else if (action == "GROUP_CREATE" && tokens.size() >= 4)
    {
        string groupId = tokens[2];
        string owner = tokens[3];
        Group g;
        g.owner = owner;
        g.members.insert(owner);
        groups[groupId] = g;
    }
    else if (action == "GROUP_JOIN_REQ" && tokens.size() >= 4)
    {
        groups[tokens[2]].pendingRequests.insert(tokens[3]);
    }
    else if (action == "GROUP_ACCEPT" && tokens.size() >= 4)
    {
        groups[tokens[2]].pendingRequests.erase(tokens[3]);
        groups[tokens[2]].members.insert(tokens[3]);
    }
    else if (action == "LEAVE_GROUP" && tokens.size() >= 4)
    {
        string groupId = tokens[1];
        string userId = tokens[2];

        auto it = groups.find(groupId);
        if (it == groups.end())
            return;

        Group &group = it->second;
        if (group.members.find(userId) == group.members.end())
            return;

        group.members.erase(userId);

        if (group.owner == userId)
        {
            if (!group.members.empty())
            {
                group.owner = *group.members.begin();
            }
            else
            {
                groups.erase(it);
                return;
            }
        }
    }
    else if (action == "UPLOAD_FILE_1" && tokens.size() >= 5)
    {
        string filehash = tokens[2];
        string user = tokens[3];
        string peer_port = tokens[4];
        files[filehash].owners.insert(user + ":" + peer_port);
    }
    else if (action == "UPLOAD_FILE_2" && tokens.size() >= 8)
    {
        uint64_t filesize;
        try
        {
            filesize = stoull(tokens[5]);
        }
        catch (...)
        {
            cout << "Error: invalid filesize" << endl;
            return;
        }
        string filename = tokens[2];
        string group = tokens[3];
        string filehash = tokens[4];
        string user = tokens[6];
        string peer_port = tokens[7];
        FileInfo fi;
        fi.filename = filename;
        fi.group = group;
        fi.file_hash = filehash;
        fi.file_size = filesize;
        fi.num_pieces = (filesize + 512 * 1024 - 1) / (512 * 1024);
        fi.piece_hashes.resize(fi.num_pieces);
        if (user != "-")
        {
            fi.owners.insert(user + ":" + peer_port);
        }
        else
        {
            cout << "[DEBUG] loggedInUser empty while creating new file\n";
        }
        files[filehash] = move(fi);
    }
    else if (action == "ADD_PIECE_HASH" && tokens.size() >= 5)
    {
        string filehash = tokens[2];
        size_t idx = stoul(tokens[3]);
        string hex = tokens[4];

        lock_guard<mutex> lk(filesMutex);

        if (!files.count(filehash))
            return;

        FileInfo &fi = files[filehash];

        if (idx >= fi.piece_hashes.size())
        {
            return;
        }

        fi.piece_hashes[idx] = hex;
    }
}
