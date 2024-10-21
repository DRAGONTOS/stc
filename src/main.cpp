#include <cctype>
#include <cpptoml.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <ostream>
#include <string>
#include "includes/getHttp.hpp"
#include "includes/Strings.hpp"
#include "includes/Regex.hpp"
#include "includes/maincommand.hpp"

std::string woof(std::ifstream &meow) {
  std::ostringstream nya;
  nya << meow.rdbuf();
  return nya.str();
}

int main(int argc, char **argv, char **envp) {
  // struct cmd
  cmd inputCmd;

  inputCmd.dir = std::filesystem::current_path();
  // Removes cache
  if (std::filesystem::exists(inputCmd.cacheid) &&
      std::filesystem::is_directory(inputCmd.cacheid)) {
    std::filesystem::remove(inputCmd.cacheid);
    std::filesystem::remove(inputCmd.cachesc);
  } else {
    std::filesystem::remove(inputCmd.cacheid);
    std::filesystem::remove(inputCmd.cachesc);
  }

  std::vector<std::string> ARGS{argv, argv + argc};
  for (int i = 0; i < argc; ++i) {
    ARGS[i] = std::string{argv[i]};
  }

  if (ARGS.size() < 2) {
    std::cout << USAGE;
    return 1;
  }

  // the args
  for (int i = 1; i < argc; ++i) {
    std::string arg = argv[i];
    if (arg.find('-') == 0) {
      // help msg
      if (ARGS[i] == "-h") {
        if (argc < 3 || argv[2][0] == '-') {
          std::cerr << HELP;
          return 1;
        }
        std::cout << HELP;
        return 1;
      }

      // collectionid
      if (ARGS[i] == "-c") {
        if (argc < 3 || argv[2][0] == '-') {
          std::cerr << USAGE;
          return 1;
        }

        if (argc == 6 || argc == 7) {

          inputCmd.collectionid  = ARGS[1 + 1];
          inputCmd.user          = ARGS[1 + 2];
          inputCmd.pass          = ARGS[1 + 3];
          inputCmd.gameid        = ARGS[1 + 4];

          if (argc == 7) {
            inputCmd.dir = ARGS[1 + 5];
          }

          inputCmd.ab = 0;
          try {
            getHttp(std::string{"https://steamcommunity.com/sharedfiles/filedetails/?id=" + inputCmd.collectionid}, &inputCmd.cachesc);
          } catch (std::string &meow) {
            std::cout << meow;
            return 1;
          }

          break;
        }

        if (argc == 4 || argc == 5) {

          inputCmd.collectionid  = ARGS[1 + 1];
          inputCmd.gameid        = ARGS[1 + 2];

          if (argc == 5) {
            inputCmd.dir = ARGS[1 + 3];
          }

          inputCmd.ab = 0;
          try {
            getHttp(std::string{"https://steamcommunity.com/sharedfiles/filedetails/?id=" + inputCmd.collectionid}, &inputCmd.cachesc);
          } catch (std::string &meow) {
            std::cout << meow;
            return 1;
          }

          break;
        } else {
          std::cerr << HELP;
          return 1;
        }
      }

      // modid
      if (ARGS[i] == "-m") {
        if (argc < 3 || argv[2][0] == '-') {
          std::cerr << USAGE;
          return 1;
        }

        if (argc == 6 || argc == 7) {

          inputCmd.modid   = ARGS[1 + 1];
          inputCmd.user    = ARGS[1 + 2];
          inputCmd.pass    = ARGS[1 + 3];
          inputCmd.gameid  = ARGS[1 + 4];

          if (argc == 7) {
            inputCmd.dir = ARGS[1 + 5];
          }

          inputCmd.ab = 1;
          break;
        }

        if (argc == 4 || argc == 5) {

          inputCmd.modid   = ARGS[1 + 1];
          inputCmd.gameid  = ARGS[1 + 2];

          if (argc == 5) {
            inputCmd.dir = ARGS[1 + 3];
          }

          inputCmd.ab = 1;
          break;
        } else {
          std::cerr << HELP;
          return 1;
        }
      } else {
        std::cerr << USAGE;
        return 1;
      }
    } else {
      std::cerr << USAGE;
      return 1;
    }
  }

  Regex(&inputCmd);
  slashing(&inputCmd);
  maincommand(&inputCmd);

  return 1;
}
