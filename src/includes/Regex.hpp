#ifndef REGEX_H
#define REGEX_H
#include <string>

// all the var's
struct cmd {
const char *userHome  = getenv("HOME");

std::string usercache = std::string(userHome)   + "/.cache/";
std::string cacheid   = std::string(usercache)  + "ids.txt";
std::string cachesc   = std::string(usercache)  + "sources.html";

std::string ids;

std::string collectionid;
std::string modid;
std::string user = "anonymous";
std::string pass;
std::string gameid;
std::string dir;

int ab    = 0;
int step  = 0;

std::string slash;

int successes = 0;
int timedout  = 0;
int errors    = 0;
int totalmeow = 0;
};

void Regex(cmd *inputCmd);
void slashing(cmd *inputCmd);
#endif
