#ifndef REGEX_H
#define REGEX_H
#include <string>

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

// total mods
int totalmods = 0;

// counter
std::string sucids;
std::string idnumber;
std::string idname;

int successes = 0;
int timedout  = 0;
int errors    = 0;
int totalmeow = 0;
};

void Regex(cmd *inputCmd);
void filerestort(cmd *inputCmd);
void Modname(cmd *inputCmd, std::string input);
#endif
