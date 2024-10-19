#ifndef REGEX_H
#define REGEX_H
#include <string>

// all the var's
struct cmd {
const char *userHome  = getenv("HOME");

std::string usercache = std::string(userHome)   + "/.cache/";
std::string cacheid   = std::string(usercache)  + "ids.txt";
std::string cachesc   = std::string(usercache)  + "sources.html";

std::string collectionid;
std::string modid;
std::string user = "anonymous";
std::string pass;
std::string gameid;
std::string dir;

int ab    = 0;
int step  = 0;

std::string slash;
};

// void Regex(const std::string& collectionid, const std::string& gameid, const std::string& cacheid, const std::string& cachesc);
void Regex(cmd *inputCmd);
void slashing(cmd *inputCmd);
#endif
