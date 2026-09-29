#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <openssl/sha.h>
#include <sstream>
#include <iomanip>
#include <cerrno>
#include <cstring>
#include <thread>
#include <mutex>
#include <map>
#include "clientUtils.h"
using namespace std;

vector<string> split(const string &s, char delim)
{
    vector<string> tokens;
    string t;
    for (char c : s)
    {
        if (c == delim)
        {
            if (!t.empty())
            {
                tokens.push_back(t);
                t.clear();
            }
        }
        else
            t.push_back(c);
    }
    if (!t.empty())
        tokens.push_back(t);
    return tokens;
}

bool connectToTracker(int startIndex = 0)
{
    if (sock >= 0)
    {
        close(sock);
        sock = -1;
    }

    size_t n = trackers.size();
    for (size_t i = 0; i < n; i++)
    {
        size_t idx = (startIndex + i) % n;

        sock = socket(AF_INET, SOCK_STREAM, 0);
        if (sock < 0)
        {
            cerr << "Socket creation failed\n";
            continue;
        }

        sockaddr_in servAddr{};
        servAddr.sin_family = AF_INET;
        servAddr.sin_port = htons(trackers[idx].port);
        inet_pton(AF_INET, trackers[idx].ip.c_str(), &servAddr.sin_addr);

        if (connect(sock, (sockaddr *)&servAddr, sizeof(servAddr)) == 0)
        {
            currentTracker = idx;
            cout << "Connected to Tracker " << (idx + 1)
                 << " (" << trackers[idx].ip << ":" << trackers[idx].port << ")\n";
            return true;
        }
        close(sock);
        sock = -1;
    }
    return false;
}

bool sendCommand(const string &command)
{
    while (true)
    {
        if (sock < 0)
        {
            int next = (currentTracker + 1) % trackers.size();
            if (!connectToTracker(next))
            {
                sleep(1);
                continue;
            }
        }
        if (send(sock, command.c_str(), command.size(), 0) > 0)
            return true;
        close(sock);
        sock = -1;
    }
}

bool receiveResponse(string &out)
{
    if (sock < 0)
        return false;
    char buf[8192];
    int bytes = recv(sock, buf, sizeof(buf) - 1, 0);
    if (bytes <= 0)
    {
        close(sock);
        sock = -1;
        return false;
    }
    buf[bytes] = '\0';
    out = buf;
    return true;
}

bool client_upload_file(const string &group, const string &filePath)
{
    vector<string> allChunkHashes;
    string completeFileHash;
    uint64_t fileSize;
    int fileDescriptor = getFileDescriptor(filePath.c_str());
    struct stat fileInfo;
    if (fstat(fileDescriptor, &fileInfo) < 0)
    {
        cout << "Error: Cannot get file stats" << endl;
        close(fileDescriptor);
        return false;
    }
    fileSize = fileInfo.st_size;

    size_t totalChunks = fileSize / chunkSize;
    if (fileSize % chunkSize != 0)
    {
        totalChunks++;
    }
    bool hashSuccess = compute_file_piece_hashes(allChunkHashes, completeFileHash, fileDescriptor, totalChunks);
    if (!hashSuccess)
    {
        cout << "Failed to calculate file hashes" << endl;
        return false;
    }

    filePathMutex.lock();
    filePathMap[completeFileHash] = filePath;
    filePathMutex.unlock();

    string fileName = filePath;
    int lastSlashPos = filePath.find_last_of('/');
    if (lastSlashPos != string::npos)
    {
        fileName = filePath.substr(lastSlashPos + 1);
    }

    string uploadCommand = "upload_file ";
    uploadCommand += group + " ";
    uploadCommand += fileName + " ";
    uploadCommand += to_string(fileSize) + " ";
    uploadCommand += completeFileHash + " ";
    uploadCommand += to_string(peer_port);

    if (!sendCommand(uploadCommand))
    {
        cout << "Command could not be sent" << endl;
        return false;
    }
    string response;
    receiveResponse(response);

    for (size_t i = 0; i < allChunkHashes.size(); i++)
    {
        string chunkHashCommand = "add_piece_hash ";
        chunkHashCommand += completeFileHash + " ";
        chunkHashCommand += to_string(i) + " ";
        chunkHashCommand += allChunkHashes[i];

        sendCommand(chunkHashCommand);

        string response;
        receiveResponse(response);
    }

    sendCommand("upload_done " + completeFileHash);
    receiveResponse(response);
    cout << response << endl;
    return true;
}

void serve_peer()
{
    int listenfd = socket(AF_INET, SOCK_STREAM, 0);
    int opt = 1;
    setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(peer_port);
    addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(listenfd, (sockaddr *)&addr, sizeof(addr)) < 0)
    {
        perror("bind failed");
        return;
    }
    listen(listenfd, 5);
    coutMutex.lock();
    cout << "Peer server listening on port " << peer_port << "\n";
    coutMutex.unlock();

    while (true)
    {
        int conn = accept(listenfd, nullptr, nullptr);
        if (conn < 0)
            continue;

        char buf[1024];
        int n = recv(conn, buf, sizeof(buf) - 1, 0);
        if (n <= 0)
        {
            close(conn);
            continue;
        }
        buf[n] = '\0';
        string cmd(buf);
        auto tokens = split(cmd, ' ');
        if (tokens[0] == "get_piece")
        {
            string filehash = tokens[1];
            int piece = stoi(tokens[2]);

            string filepath;
            filePathMutex.lock();
            if (filePathMap.count(filehash))
            {
                filepath = filePathMap[filehash];
            }
            filePathMutex.unlock();
            if (filepath.empty())
            {
                cerr << " No file path found for hash " << filehash << "\n";
                close(conn);
                continue;
            }
            int fd = open(filepath.c_str(), O_RDONLY);
            if (fd < 0)
            {
                close(conn);
                continue;
            }
            vector<char> piecebuf(chunkSize);
            int r = pread(fd, piecebuf.data(), chunkSize, piece * chunkSize);
            close(fd);
            if (r > 0)
                send(conn, piecebuf.data(), r, 0);
        }
        close(conn);
    }
}

bool client_download_file(const string &group, const string &fileName, const string &destPath)
{

    string requestMsg = "download_file " + group + " " + fileName + " " + destPath;
    sendCommand(requestMsg);
    string trackerResponse;
    receiveResponse(trackerResponse);
    // Example resp format: "OK <file_hash> <filesize> peer1:port1 peer2:port2 ..."
    auto tokens = split(trackerResponse, ' ');
    if (tokens.size() < 4 || tokens[0] != "OK")
    {
        cerr << " Tracker error: " << trackerResponse << "\n";
        return false;
    }

    string filehash = tokens[1];
    uint64_t fileSize = stoull(tokens[2]);

    vector<string> peers(tokens.begin() + 3, tokens.end());
    if (peers.empty())
    {
        cerr << " No peers found for file\n";
        return false;
    }

    // Open destination file for writing
    int fd = open(destPath.c_str(), O_CREAT | O_WRONLY | O_TRUNC, 0644);
    if (fd < 0)
    {
        perror("open");
        return false;
    }

    // Download sequentially from first peer
    size_t totalChunks = fileSize / chunkSize;
    if (fileSize % chunkSize != 0)
        totalChunks++;

    size_t totalPeers = peers.size();

    vector<vector<size_t>> peer_pieces(totalPeers);
    for (size_t i = 0; i < totalChunks; i++)
    {
        peer_pieces[i % totalPeers].push_back(i);
    }

    // Storage for downloaded pieces
    vector<vector<char>> piece_buffers(totalChunks);
    vector<atomic<bool>> piece_success(totalChunks);

    // Initialize buffers
    for (size_t i = 0; i < totalChunks; i++)
    {
        size_t this_size = min<uint64_t>(chunkSize, fileSize - i * chunkSize);
        piece_buffers[i].resize(this_size);
        piece_success[i] = false;
    }

    const size_t MAX_CONCURRENT_THREADS = 10;
    size_t current_piece = 0;

    while (current_piece < totalChunks)
    {
        vector<thread> batch_threads;

        // Launch up to MAX_CONCURRENT_THREADS
        for (size_t i = 0; i < MAX_CONCURRENT_THREADS && current_piece < totalChunks; i++, current_piece++)
        {
            size_t peer_idx = current_piece % totalPeers;
            auto parts = split(peers[peer_idx], ':');
            if (parts.size() != 2)
                continue;

            string peer_ip = parts[0];
            int peer_port = stoi(parts[1]);

            batch_threads.emplace_back(
                download_piece_thread,
                filehash,
                current_piece,
                peer_ip,
                peer_port,
                ref(piece_buffers[current_piece]),
                ref(piece_success[current_piece]));
        }

        // Wait for this batch to complete
        for (auto &t : batch_threads)
        {
            if (t.joinable())
                t.join();
        }
    }
    // Check if all pieces downloaded successfully
    bool all_success = true;
    for (size_t i = 0; i < totalChunks; i++) {
        if (!piece_success[i]) {
            cout << "Failed to download piece " << i << endl;
            all_success = false;
        }
    }
    
    if (!all_success) {
        close(fd);
        return false;
    }

    for (size_t i = 0; i < totalChunks; i++) {
        size_t offset = i * chunkSize;
        pwrite(fd, piece_buffers[i].data(), piece_buffers[i].size(), offset);
    }
    close(fd);

    // 6. Verify hash
    vector<string> piece_hashes;
    string new_hash, err;
    uint64_t check_size;

    int newFd = getFileDescriptor(destPath.c_str());
    
    if (!compute_file_piece_hashes(piece_hashes, new_hash, newFd, totalChunks)) {
        cout << "Failed to recompute hash: " << err << "\n";
        return false;
    }
    
    if (new_hash != filehash) {
        cerr << "Hash mismatch! File corrupted.\n";
        return false;
    }

    // Register file in map for re-sharing
    {
        lock_guard<mutex> lg(filePathMutex);
        filePathMap[filehash] = destPath;
    }
    
    cout << "Download complete and uploading for sharing" << endl;
    downloads.push_back({group, fileName});
    client_upload_file(group, destPath);
    return true;
}

void handleStopShare(const string &group, const string &fileName){
    string command = "stop_share ";
    command += group + " ";
    command += fileName + " ";
    command += to_string(peer_port);
    
    // Send to tracker
    bool sent = sendCommand(command);
    if (!sent)
    {
        cout << "Failed to send stop_share command" << endl;
        return;
    }
    
    // Get response
    string response;
    bool received = receiveResponse(response);
    if (!received)
    {
        cout << "No response from tracker" << endl;
        return;
    }
    
    cout << response << endl;
    return;
}

int main(int argc, char *argv[])
{
    if (argc < 3)
    {
        cerr << "Usage: ./client <IP>:<PORT> tracker_info.txt\n";
        return 1;
    }

    string ipport = argv[1];
    auto parts = split(ipport, ':');
    if (parts.size() != 2)
    {
        cerr << "Invalid <IP>:<PORT>\n";
        return 1;
    }
    peer_ip = parts[0];
    peer_port = stoi(parts[1]);

    auto [tracker1Ip, tracker1Port] = readTrackerInfoInClient(argv[2], 1);
    auto [tracker2Ip, tracker2Port] = readTrackerInfoInClient(argv[2], 2);
    
    trackers.push_back({tracker1Ip, tracker1Port});
    trackers.push_back({tracker2Ip, tracker2Port});

    if (trackers.empty())
    {
        cerr << "No trackers in file\n";
        return 1;
    }

    if (!connectToTracker())
    {
        cerr << "Could not connect to any tracker\n";
        return 1;
    }

    // start peer server
    thread(serve_peer).detach();

    string line;
    while (true)
    {
        cout << "> ";
        if (!getline(cin, line))
            break;
        if (line == "quit")
            break;

        vector<string> tokens = split(line, ' ');
        if (tokens.empty())
            continue;

        if (tokens[0] == "upload_file")
        {
            if (tokens.size() < 3)
            {
                cout << "Usage: upload_file <group_id> <file_path>\n";
                continue;
            }
            client_upload_file(tokens[1], tokens[2]);
            continue;
        }
        if (tokens[0] == "download_file")
        {
            if (tokens.size() < 4)
            {
                cout << "Usage: download_file <group_id> <filename> <destpath>\n";
                continue;
            }
            client_download_file(tokens[1], tokens[2], tokens[3]);
            continue;
        }
        if (tokens[0] == "show_downloads")
        {
            for (pair<string, string> d: downloads){
                cout<<"[C] "<<d.first<<" "<<d.second<<endl;
            }
            continue;
        }
        if (tokens[0] == "stop_share"){
            if (tokens.size() < 3)
            {
                cout << "Usage: stop_share <group_id> <fileName>\n";
                continue;
            }
            handleStopShare(tokens[1], tokens[2]);
            continue;
        }

        sendCommand(line);
        string resp;
        if (receiveResponse(resp))
            cout << resp << "\n";
    }
    if (sock >= 0)
        close(sock);
}
