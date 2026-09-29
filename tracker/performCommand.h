#pragma once
#include <string>

// Handles a command string and returns a response
std::string handleCommand(const std::string &command, std::string &loggedInUser);