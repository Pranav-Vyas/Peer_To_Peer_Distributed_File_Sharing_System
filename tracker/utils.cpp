#include "utils.h"
#include "data_structure.h"
#include <bits/stdc++.h>
#include <netinet/in.h>
#include <unistd.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>
using namespace std;

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

void sendToPeer(const string &command)
{
    if (peerIp.empty() || peerPort == 0)
        return;

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0)
        return;

    sockaddr_in peerAddr{};
    peerAddr.sin_family = AF_INET;
    peerAddr.sin_port = htons(peerPort);
    inet_pton(AF_INET, peerIp.c_str(), &peerAddr.sin_addr);

    if (connect(sock, (sockaddr *)&peerAddr, sizeof(peerAddr)) < 0)
    {
        close(sock);
        return; // peer might be down
    }

    string msg = command;
    send(sock, msg.c_str(), msg.size(), 0);
    close(sock);
}
