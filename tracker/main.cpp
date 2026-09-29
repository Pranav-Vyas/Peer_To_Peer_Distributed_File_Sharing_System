#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <set>
#include <netinet/in.h>
#include <unistd.h>
#include <fcntl.h>
#include <cstring>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <thread>
#include "data_structure.h"
#include "utils.h"
#include "performCommand.h"
#include <iomanip>
using namespace std;

atomic<bool> running(true);

void *clientHandler(void *arg)
{
    int clientSock = *(int *)arg;
    delete (int *)arg;

    char buffer[1024];
    string loggedInUser;

    while (true)
    {
        memset(buffer, 0, sizeof(buffer));
        int bytes = recv(clientSock, buffer, sizeof(buffer), 0);
        if (bytes <= 0)
            break;

        string command(buffer);
        string response = handleCommand(command, loggedInUser);
        send(clientSock, response.c_str(), response.size(), 0);
    }

    close(clientSock);
    return nullptr;
}

pair<string, int> readTrackerInfo(const char *filepath, int trackerIndex)
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

void commandLoop(int serverSock)
{
    string cmd;
    while (true)
    {
        cout << "tracker > ";
        getline(cin, cmd);
        if (cmd == "quit")
        {
            running = false;
            shutdown(serverSock, SHUT_RDWR);
            close(serverSock);
            break;
        }
    }
}

int main(int argc, char *argv[])
{
    if (argc < 3)
    {
        cerr << "Usage: ./tracker tracker_info.txt <index>\n";
        return 1;
    }

    int myIndex = stoi(argv[2]);
    auto [myIp, myPort] = readTrackerInfo(argv[1], myIndex);
    auto [peerIp_, peerPort_] = readTrackerInfo(argv[1], myIndex == 1 ? 2 : 1);
    peerIp = peerIp_;
    peerPort = peerPort_;

    int serverSock = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSock < 0)
    {
        perror("socket");
        return 1;
    }

    int opt = 1;
    setsockopt(serverSock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(myPort);
    addr.sin_addr.s_addr = INADDR_ANY;
    if (bind(serverSock, (sockaddr *)&addr, sizeof(addr)) < 0)
    {
        perror("bind");
        return 1;
    }
    if (listen(serverSock, 10) < 0)
    {
        perror("listen");
        return 1;
    }

    cout << "Tracker " << myIndex << " listening on port " << myPort << "\n";
    thread cliThread(commandLoop, serverSock);
    while (running)
    {
        sockaddr_in clientAddr{};
        socklen_t len = sizeof(clientAddr);
        int clientSock = accept(serverSock, (sockaddr *)&clientAddr, &len);
        if (clientSock < 0)
            if (!running)
                break;
            else
            {
                continue;
            }
        pthread_t tid;
        int *pclient = new int(clientSock);
        pthread_create(&tid, nullptr, clientHandler, pclient);
        pthread_detach(tid);
    }
    cliThread.join();
    close(serverSock);
    return 0;
}
