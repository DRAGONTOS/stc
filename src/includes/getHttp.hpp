#ifndef GETHTTP_H
#define GETHTTP_H
#include <string>
#include "Regex.hpp"
void downloadFile(const std::string& url, const std::string& outputPath);
void getHttp(cmd *inputCmd, std::string url);
#endif
