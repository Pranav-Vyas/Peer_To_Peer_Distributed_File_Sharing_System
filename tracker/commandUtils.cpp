#include "performCommand.h"
#include "data_structure.h"
#include "utils.h"
#include <vector>
#include <string>

string downloadFile(vector<string> tokens)
{
    if (tokens.size() < 3)
        return "Error: usage download_file <group_id> <filename>";

    string group = tokens[1];
    string filename = tokens[2];

    lock_guard<mutex> lk(filesMutex);

    for (auto &p : files)
    {
        const FileInfo &f = p.second;
        if (f.group == group && f.filename == filename)
        {
            string result = "OK " + f.file_hash + " " + to_string(f.file_size);
            for (auto &o : f.owners)
                result += " " + o;
            return result;
        }
    }

    return "Error: file not found in group";
}

string getFile(vector<string> tokens)
{
    if (tokens.size() < 2)
        return "Error: usage get_file <filename>";
    string filename = tokens[1];

    lock_guard<mutex> lk(filesMutex);
    for (auto &kv : files)
    {
        const FileInfo &fi = kv.second;
        if (fi.filename == filename)
        {
            string result = "FILE " + fi.file_hash + " " + to_string(fi.file_size) + " " + to_string(fi.num_pieces) + "\n";

            result += "OWNERS";
            for (auto &o : fi.owners)
                result += " " + o;

            result += "\nPIECE_HASHES\n";
            for (size_t i = 0; i < fi.piece_hashes.size(); ++i)
                result += to_string(i) + " " + fi.piece_hashes[i] + "\n";

            result += "END\n";
            return result;
        }
    }
    return "Error: file not found";
}

string listFiles(vector<string> tokens)
{
    if (tokens.size() < 2)
        return "Error: usage list_files <group_id>";
    string group = tokens[1];
    string result;

    lock_guard<mutex> lk(filesMutex);
    for (const auto &kv : files)
    {
        const FileInfo &fi = kv.second;
        if (fi.group == group)
        {
            result += fi.filename + " " + to_string(fi.file_size) + " " + fi.file_hash + " owners:";
            for (const auto &o : fi.owners)
                result += " " + o;
            result += "\n";
        }
    }
    return result.empty() ? "No files available" : result;
}

string uploadDone(vector<string> tokens)
{
    if (tokens.size() < 2)
        return "Error: usage upload_done <filehash>";

    string filehash = tokens[1];
    lock_guard<mutex> lk(filesMutex);
    if (!files.count(filehash))
        return "Error: unknown filehash";

    FileInfo &fi = files[filehash];

    // Optionally verify no piece is empty
    for (size_t i = 0; i < fi.piece_hashes.size(); i++)
    {
        if (fi.piece_hashes[i].empty())
        {
            return "Error: missing piece hash at index " + to_string(i);
        }
    }
    return string("UPLOAD_DONE ") + filehash;
}

string addPieceHash(vector<string> tokens)
{
    if (tokens.size() < 4)
        return "Error: usage add_piece_hash <filehash> <index> <hexhash>";

    string filehash = tokens[1];
    size_t idx = stoul(tokens[2]);
    string hex = tokens[3];

    lock_guard<mutex> lk(filesMutex);

    if (!files.count(filehash))
        return "Error: unknown filehash";

    FileInfo &fi = files[filehash];

    if (idx >= fi.piece_hashes.size())
    {
        return "Error: invalid piece index";
    }

    fi.piece_hashes[idx] = hex;
    if (!syncingFromPeer)
                sendToPeer("SYNC ADD_PIECE_HASH " + filehash + " " + tokens[2] + " " + tokens[3]);

    return "OK";
}

string uploadFile(vector<string> tokens, string &loggedInUser)
{
    if (tokens.size() < 6)
        return "Error: usage upload_file <group> <filename> <filesize> <filehash> <peer_port>";

    string group = tokens[1];
    string filename = tokens[2];
    uint64_t filesize;
    try
    {
        filesize = stoull(tokens[3]);
    }
    catch (...)
    {
        return "Error: invalid filesize";
    }
    string filehash = tokens[4];
    string peer_port = tokens[5];
    lock_guard<mutex> lk(filesMutex);
    if (files.count(filehash))
    {
        if (!loggedInUser.empty())
        {
            files[filehash].owners.insert(loggedInUser + ":" + peer_port);
            if (!syncingFromPeer)
                sendToPeer("SYNC UPLOAD_FILE_1 " + filehash + " " + loggedInUser + " " + peer_port);
        }
    }
    else
    {
        FileInfo fi;
        fi.filename = filename;
        fi.group = group;
        fi.file_hash = filehash;
        fi.file_size = filesize;
        fi.num_pieces = (filesize + 512 * 1024 - 1) / (512 * 1024);
        fi.piece_hashes.resize(fi.num_pieces);
        if (!loggedInUser.empty())
        {
            fi.owners.insert(loggedInUser + ":" + peer_port);
            if (!syncingFromPeer)
                sendToPeer("SYNC UPLOAD_FILE_2 " + filename + " " + group + " " + filehash + " " + tokens[3] +" "+loggedInUser+" "+peer_port);
        }
        else
        {
            cout << "[DEBUG] loggedInUser empty while creating new file\n";
            if (!syncingFromPeer)
                sendToPeer("SYNC UPLOAD_FILE_2 " + filename + " " + group + " " + filehash + " " + tokens[3] +" "+"-"+" "+peer_port);
        }
        files[filehash] = move(fi);
    }
    return "UPLOAD_OK " + filehash;
}

string leaveGroup(const vector<string> &tokens, string &loggedInUser)
{
    if (tokens.size() < 2)
        return "ERROR: Usage: leave_group <groupId>";

    string groupId = tokens[1];
    string userId = loggedInUser;

    auto it = groups.find(groupId);
    if (it == groups.end())
        return "ERROR: Group not found";

    Group &group = it->second;
    if (group.members.find(userId) == group.members.end())
        return "ERROR: You are not a member of this group";

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
            return "OK: Group deleted (no members left)";
        }
    }

    if (!syncingFromPeer)
        sendToPeer("SYNC LEAVE_GROUP " + groupId + " " + loggedInUser);

    return "OK: Left group successfully";
}

string acceptRequest(const string &command, vector<string> tokens, string &loggedInUser)
{
    if (tokens.size() < 3)
        return "Error: usage accept_request <group_id> <user_id>";
    if (loggedInUser.empty())
        return "Error: please login first";
    string gid = tokens[1], uid = tokens[2];
    if (!groups.count(gid))
        return "Error: group does not exist";
    if (groups[gid].owner != loggedInUser)
        return "Error: only owner can accept requests";
    if (!groups[gid].pendingRequests.count(uid))
        return "Error: no such request";
    groups[gid].pendingRequests.erase(uid);
    groups[gid].members.insert(uid);

    if (!syncingFromPeer)
        sendToPeer("SYNC GROUP_ACCEPT " + gid + " " + uid);
    return "Request accepted";
}

string listRequest(vector<string> tokens, string &loggedInUser)
{
    if (tokens.size() < 2)
        return "Error: usage list_requests <group_id>";
    if (loggedInUser.empty())
        return "Error: please login first";
    string groupId = tokens[1];
    if (!groups.count(groupId))
        return "Error: group does not exist";
    if (groups[groupId].owner != loggedInUser)
        return "Error: only owner can list requests";
    if (groups[groupId].pendingRequests.empty())
        return "No pending requests";
    string response;
    for (auto &r : groups[groupId].pendingRequests)
    {
        response += r + "\n";
    }
    return response;
}

string listGroups()
{
    if (groups.empty())
        return "No groups available";
    string response;
    for (auto &kv : groups)
    {
        response += kv.first + " (owner: " + kv.second.owner + ")\n";
    }
    return response;
}

string joinGroup(const string &command, vector<string> tokens, string &loggedInUser)
{
    if (tokens.size() < 2)
        return "Error: usage join_group <group_id>";
    if (loggedInUser.empty())
        return "Error: please login first";
    string gid = tokens[1];
    if (!groups.count(gid))
        return "Error: group does not exist";
    if (groups[gid].members.count(loggedInUser))
        return "Error: already a member";
    groups[gid].pendingRequests.insert(loggedInUser);

    if (!syncingFromPeer)
        sendToPeer("SYNC GROUP_JOIN_REQ " + gid + " " + loggedInUser);
    return "Join request sent";
}

string createGroup(const string &command, vector<string> tokens, string &loggedInUser)
{
    if (tokens.size() < 2)
        return "Error: usage create_group <group_id>";
    if (loggedInUser.empty())
        return "Error: please login first";
    string gid = tokens[1];
    if (groups.count(gid))
        return "Error: group already exists";
    Group g;
    g.owner = loggedInUser;
    g.members.insert(loggedInUser);
    groups[gid] = g;

    if (!syncingFromPeer)
        sendToPeer("SYNC GROUP_CREATE " + gid + " " + loggedInUser);
    return "Group created successfully";
}

string logout(const string &command, string &loggedInUser)
{
    if (loggedInUser.empty())
        return "Error: no user logged in";

    if (users.count(loggedInUser))
    {
        users[loggedInUser].loggedIn = false;
    }
    if (!syncingFromPeer)
        sendToPeer("SYNC USER_LOGOUT " + loggedInUser);
    loggedInUser.clear();
    return "Logout successful";
}

string login(const string &command, vector<string> tokens, string &loggedInUser)
{
    if (tokens.size() < 3)
        return "Error: usage login <user_id> <password>";
    string userId = tokens[1];
    string password = tokens[2];

    if (!loggedInUser.empty())
        return "Error: already logged in on this connection";

    if (!users.count(userId))
        return "Error: user does not exist";

    if (users[userId].loggedIn)
        return "Error: user already logged in elsewhere";

    if (users[userId].password != password)
        return "Error: invalid password";

    users[userId].loggedIn = true;
    loggedInUser = userId;
    return "Login successful";
}

string createUser(const string &command, vector<string> tokens)
{
    if (tokens.size() < 3)
        return "Error: usage create_user <user_id> <password>";
    string userId = tokens[1], password = tokens[2];
    if (users.count(userId))
        return "Error: user already exists";
    users[userId] = {password, false};

    if (!syncingFromPeer)
        sendToPeer("SYNC USER_CREATE " + userId + " " + password);
    return "User created successfully";
}