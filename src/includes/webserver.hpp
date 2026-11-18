#ifndef WEBSERVER_HPP
#define WEBSERVER_HPP
#include <string>
#include "Regex.hpp"

// serves the local website on 127.0.0.1, never returns
void webserver(const cmd *inputCmd, const std::string& port);
#endif
