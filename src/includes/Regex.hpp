#ifndef REGEX_H
#define REGEX_H
#include <string>
#include <vector>
// all the var's
struct cmd {
// the user's home; a missing HOME falls back to /tmp so nothing builds a std::string from null
std::string userHome = getenv("HOME") == nullptr ? "/tmp" : getenv("HOME");
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

// total mods
int totalmods = 0;

// Modnamestuff
std::vector<std::string> sucids;

int successes = 0;
int timedout  = 0;
int errors    = 0;
};

void Regex(cmd *inputCmd);
void Modname(cmd *inputCmd, size_t index);
void filerestort(cmd *inputCmd, std::string idnumber, std::string idname);
std::string workshoptitle(const std::string& sourcefile);
std::string jsontitle(const std::string& sourcefile, const std::string& modId);
#endif
