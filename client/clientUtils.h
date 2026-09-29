#include <bits/stdc++.h>
#include <fcntl.h>
#include "clientGlobalVariables.h"
using namespace std;

string convertHexString(const unsigned char *buf, size_t len)
{
    static const char *hex_chars = "0123456789abcdef";
    std::string result;
    result.reserve(len * 2);

    for (size_t i = 0; i < len; ++i)
    {
        unsigned char byte = buf[i];
        int high = byte / 16;
        int low = byte % 16;
        result.push_back(hex_chars[high]);
        result.push_back(hex_chars[low]);
    }

    return result;
}

int getFileDescriptor(const char *path){
    int fileDescriptor = open(path, O_RDONLY);
    if (fileDescriptor < 0)
    {
        cout<<"Error in opening file"<<endl;
        return 0;
    }
    return fileDescriptor;
}

bool compute_file_piece_hashes(vector<string> &chunkHashes, string &file_hash, int fileDescriptor, size_t totalChunks)
{
    chunkHashes.clear();
    chunkHashes.reserve(totalChunks);

    SHA_CTX sha_context;
    SHA1_Init(&sha_context);

    unsigned char buffer[chunkSize];
    off_t currentChunkIndex = 0;

    while (currentChunkIndex < totalChunks)
    {
        off_t readPos = currentChunkIndex * chunkSize;
        ssize_t bytesRead = pread(fileDescriptor, buffer, chunkSize, readPos);
        if (bytesRead < 0)
        {
            cout<<"Error in reading chunk"<<endl;
            close(fileDescriptor);
            return false;
        }

        unsigned char chunkHash[SHA_DIGEST_LENGTH];
        SHA1(buffer, bytesRead, chunkHash);
        chunkHashes.push_back(convertHexString(chunkHash, SHA_DIGEST_LENGTH));
        SHA1_Update(&sha_context, buffer, bytesRead);
        currentChunkIndex++;
    }

    unsigned char finalHash[SHA_DIGEST_LENGTH];
    SHA1_Final(finalHash, &sha_context);
    file_hash = convertHexString(finalHash, SHA_DIGEST_LENGTH);
    close(fileDescriptor);
    return true;
}

vector<string> split(const string &input)
{
    vector<string> tokens;
    string token;
    for (char c : input)
    {
        if (c == ' ' || c == '\t')
        {
            if (!token.empty())
            {
                tokens.push_back(token);
                token.clear();
            }
        }
        else
        {
            token.push_back(c);
        }
    }
    if (!token.empty())
        tokens.push_back(token);
    return tokens;
}

pair<string, int> readTrackerInfoInClient(const char *filepath, int trackerIndex)
{
    int fd = open(filepath, O_RDONLY);
    if (fd < 0)
    {
        perror("open");
        exit(1);
    }

    char buf[1024];
    int bytesRead = read(fd, buf, sizeof(buf) - 1);
    if (bytesRead < 0)
    {
        perror("read");
        exit(1);
    }
    buf[bytesRead] = '\0';
    close(fd);

    // Split by newline
    vector<string> lines;
    string line;
    for (int i = 0; i < bytesRead; i++)
    {
        if (buf[i] == '\n')
        {
            if (!line.empty())
            {
                lines.push_back(line);
                line.clear();
            }
        }
        else
        {
            line.push_back(buf[i]);
        }
    }
    if (!line.empty())
        lines.push_back(line);

    if (trackerIndex < 1 || trackerIndex > (int)lines.size())
    {
        cerr << "Invalid tracker index\n";
        exit(1);
    }

    string selected = lines[trackerIndex - 1];
    vector<string> parts = split(selected);
    if (parts.size() != 2)
    {
        cerr << "Invalid format in tracker_info.txt\n";
        exit(1);
    }
    string ip = parts[0];
    int port = stoi(parts[1]);
    return {ip, port};
}

int connectToPeerClient(const string &ip, int port)
{
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0)
        return -1;

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    inet_pton(AF_INET, ip.c_str(), &addr.sin_addr);

    if (connect(sock, (sockaddr *)&addr, sizeof(addr)) < 0)
    {
        close(sock);
        return -1;
    }
    return sock;
}

bool fetchPiece(int peerSocket, const string &fileHash, int pieceIndex, vector<char> &outBuf, size_t bytesRequired)
{
    string pieceRequestCommand = "get_piece " + fileHash + " " + to_string(pieceIndex);
    send(peerSocket, pieceRequestCommand.c_str(), pieceRequestCommand.size(), 0);

    outBuf.resize(bytesRequired);  // allocate in caller’s vector

    size_t bytesReceived = 0;
    while (bytesReceived < bytesRequired)
    {
        int r = recv(peerSocket, outBuf.data() + bytesReceived, bytesRequired - bytesReceived, 0);
        if (r <= 0)
        {
            cerr << "Peer disconnected"<<endl;
            close(peerSocket);
            return false;
        }
        bytesReceived += r;
    }
    return true;
}

void download_piece_thread(const string& filehash, size_t piece_index, const string& peer_ip, int peer_port, vector<char>& piece_buffer, atomic<bool>& success) {
    success = false;
    
    int psock = socket(AF_INET, SOCK_STREAM, 0);
    if (psock < 0) {
        cerr << "Failed to create socket for piece " << piece_index << "\n";
        return;
    }
    
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(peer_port);
    inet_pton(AF_INET, peer_ip.c_str(), &addr.sin_addr);
    
    if (connect(psock, (sockaddr*)&addr, sizeof(addr)) < 0) {
        cerr << "Failed to connect to peer " << peer_ip << ":" << peer_port 
             << " for piece " << piece_index << "\n";
        close(psock);
        return;
    }
    
    // Send request
    string qstr = "get_piece " + filehash + " " + to_string(piece_index);
    
    if (send(psock, qstr.c_str(), qstr.size(), 0) <= 0) {
        cerr << "Failed to send request for piece " << piece_index << "\n";
        close(psock);
        return;
    }
    
    // Receive piece
    size_t expected_size = piece_buffer.size();
    size_t got = 0;
    
    while (got < expected_size) {
        int r = recv(psock, piece_buffer.data() + got, expected_size - got, 0);
        if (r <= 0) {
            cerr << "Connection lost while downloading piece " << piece_index << "\n";
            close(psock);
            return;
        }
        got += r;
    }
    
    close(psock);
    success = true;
    
    {
        lock_guard<mutex> lg(coutMutex);
        cout << "Downloaded piece " << piece_index << " from " << peer_ip << ":" << peer_port << "\n";
    }
}