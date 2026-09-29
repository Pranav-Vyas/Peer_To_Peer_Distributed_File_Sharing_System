#include "performCommand.h"
#include "data_structure.h"
#include "utils.h"
#include "commandUtils.h"
#include "sync.h"
#include <vector>
#include <string>
#include <sstream>
#include <iostream>
#include <iomanip>
using namespace std;

string handleCommand(const string &command, string &loggedInUser)
{
    vector<string> tokens = split(command);
    if (tokens.empty())
        return "Error: empty command";

    string cmd = tokens[0];
    if (cmd == "SYNC")
    {
        syncingFromPeer = true;
        handleSyncMessage(command);
        syncingFromPeer = false;
        return "OK";
    }

    if (cmd == "create_user")
        return createUser(command, tokens);
    if (cmd == "login")
        return login(command, tokens, loggedInUser);
    if (cmd == "logout")
        return logout(command, loggedInUser);
    if (cmd == "create_group")
        return createGroup(command, tokens, loggedInUser);
    if (cmd == "join_group")
        return joinGroup(command, tokens, loggedInUser);
    if (cmd == "list_groups")
        return listGroups();
    if (cmd == "list_requests")
        return listRequest(tokens, loggedInUser);
    if (cmd == "accept_request")
        return acceptRequest(command, tokens, loggedInUser);
    if (cmd == "leave_group")
    {
        return leaveGroup(tokens, loggedInUser);
    }

    if (cmd == "upload_file")
    {
        return uploadFile(tokens, loggedInUser);
    }

    else if (cmd == "add_piece_hash")
    {

        return addPieceHash(tokens);
    }

    else if (cmd == "upload_done")
    {
        return uploadDone(tokens);
    }

    if (cmd == "list_files")
    {
        return listFiles(tokens);
    }

    if (cmd == "get_file")
    {
        return getFile(tokens);
    }
    else if (cmd == "download_file")
    {
        return downloadFile(tokens);
    }
    if (cmd == "stop_share")
    {
        if (tokens.size() < 4)
            return "Error: usage stop_share <group_id> <fileName> <peer_port>";

        if (loggedInUser.empty())
            return "Error: please login first";

        string groupId = tokens[1];
        string fileName = tokens[2];
        string peerPort = tokens[3];

        // Build the owner token (userId:port)
        string ownerToken = loggedInUser + ":" + peerPort;

        lock_guard<mutex> lk(filesMutex);

        // Find the file in the registry
        bool fileFound = false;
        string fileHashToRemove = "";

        for (auto &entry : files)
        {
            FileInfo &fileInfo = entry.second;

            // Check if this is the right file in the right group
            if (fileInfo.group == groupId && fileInfo.filename == fileName)
            {
                fileFound = true;

                // Check if this user is actually sharing this file
                if (fileInfo.owners.count(ownerToken) == 0)
                {
                    return "Error: you are not sharing this file";
                }

                // Remove this owner from the seeders list
                fileInfo.owners.erase(ownerToken);

                // If no more seeders, remove file completely
                if (fileInfo.owners.empty())
                {
                    fileHashToRemove = entry.first;
                    break;
                }

                return "Successfully stopped sharing " + fileName;
            }
        }

        // Remove file entry if no seeders left
        if (!fileHashToRemove.empty())
        {
            files.erase(fileHashToRemove);
            return "Stopped sharing " + fileName + " (no more seeders, file removed from tracker)";
        }

        if (!fileFound)
        {
            return "Error: file not found in group " + groupId;
        }

        return "Error: unknown error";
    }

    return "Error: unknown command";
}
