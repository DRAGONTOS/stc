#include <algorithm>
#include <cctype>
#include <cpptoml.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <ios>
#include <iostream>
#include <limits>
#include <ostream>
#include <stdexcept>
#include <string>

void clearBuffer() {
  std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
}

inline void checkIfExtractionFailed() {
  if (!std::cin) {        // if previous extraction failed
    if (std::cin.eof()) { // check if eof and if yes aborts the program
      std::abort();
    }

    std::cin.clear(); // put std::cin back into normal mode
    clearBuffer();    // remove bad input
  }
}

std::string woof(std::ifstream& meow){
  std::ostringstream nya;
  nya << meow.rdbuf();
  return nya.str();
}

const std::string USAGE = R"#(usage:  RapidMenu [flags] [<command> [args]]
LISTING COMMANDS:
    -c:           To specify which config to use.
    -b:           Make a executable out of a config.
)#";

const std::string invalidvalue = "Invalid value in config: ";
const std::string invalidconfig = "Not a valid config: ";

int main(int argc, char **argv, char **envp) {

  const char* userHome = getenv("HOME");

  std::string collectionid;
  std::string user = "anonymous";
  std::string pass;
  std::string gameid;
  std::string dir;
  dir = std::filesystem::current_path();

  std::vector<std::string> ARGS{argv, argv + argc};
  for (int i = 0; i < argc; ++i) {
    ARGS[i] = std::string{argv[i]};
  }

  if (ARGS.size() < 2) {
    std::cout << USAGE;
    return 1;
  }
  for (int i = 1; i < argc; ++i) {
    std::string arg = argv[i];
      if (ARGS[i] == "-ci") {
        collectionid = ARGS[1+1];
        std::cout << collectionid;
        
        if (ARGS[i+2] == "-u") {
          user = ARGS[i+3];
          std::cout << user;
        } else if (ARGS[i+2] == "-gi") {
          gameid = ARGS[i+3];
          std::cout << gameid;
          if (ARGS[i+4] == "-d") {
            dir = ARGS[i+5];
            std::cout << dir;
          }
          break;
        }

        if (ARGS[i+4] == "-p") {
          pass = ARGS[i+5];
          std::cout << pass;
        }

        if (ARGS[i+6] == "-gi") {
          gameid = ARGS[i+7];
          std::cout << gameid;
        }

        if (ARGS[i+8] == "-d") {
          dir = ARGS[i+9];
          std::cout << dir;
        }

      }
    }
  // std::cerr << USAGE.c_str();

  std::ifstream ids{"ids.txt"};
  system(std::string {"sh ~/Steam/steamcmd.sh +force_install_dir " + dir + " +login " + user + pass + R"( \ )" + woof(ids)}.c_str());
}
