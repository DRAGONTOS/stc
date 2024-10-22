#ifndef REGEX_H
#define REGEX_H
#include <string>
#include <vector>
// all the var's
struct cmd {
const char* userHome  = getenv("HOME");
// buffers
std::string ids;
std::string source;

// what the args need 
std::string collectionid;
std::string modid;
std::string user = "anonymous";
std::string pass;
std::string gameid;
std::string dir;

bool ab = false;

// slashing
int slashtp  = 0;
std::string slash;

// counter
std::vector<std::string> sucids;

int successes = 0;
int timedout  = 0;
int errors    = 0;
int totalmeow = 0;
size_t threadsCompleted = 0;
};

void Regex(cmd *inputCmd);
void Modname(cmd *inputCmd, size_t index);
#endif
