#pragma once
#include <bits/stdc++.h>
using namespace std;

string downloadFile(vector<string> tokens);
string getFile(vector<string> tokens);
string listFiles(vector<string> tokens);
string uploadDone(vector<string> tokens);
string addPieceHash(vector<string> tokens);
string uploadFile(vector<string> tokens, string &loggedInUser);
string leaveGroup(const vector<string> &tokens, string &loggedInUser);
string acceptRequest(const string &command, vector<string> tokens, string &loggedInUser);
string listRequest(vector<string> tokens, string &loggedInUser);
string listGroups();
string joinGroup(const string &command, vector<string> tokens, string &loggedInUser);
string createGroup(const string &command, vector<string> tokens, string &loggedInUser);
string logout(const string &command, string &loggedInUser);
string login(const string &command, vector<string> tokens, string &loggedInUser);
string createUser(const string &command, vector<string> tokens);