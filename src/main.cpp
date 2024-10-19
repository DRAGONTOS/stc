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

          std::cout << "success\n";
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

          std::cout << "success\n";
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
          std::cout << "success\n";
          break;
        }

        if (argc == 4 || argc == 5) {

          inputCmd.modid   = ARGS[1 + 1];
          inputCmd.gameid  = ARGS[1 + 2];

          if (argc == 5) {
            inputCmd.dir = ARGS[1 + 3];
          }

          inputCmd.ab = 1;
          std::cout << "success\n";
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

  // regex function
  Regex(&inputCmd);

  //checks if a "\" is needed or not
  std::ifstream idscount{inputCmd.cacheid};
  int step = 0;

  for (std::string line; std::getline(idscount, line); ) {
      step++;
  }
  
  std::string slash = (step == 2) ? R"( )": R"( \ )";
  idscount.close();

  //gets the ids
  std::ifstream ids{inputCmd.cacheid};
  std::string idsm = (inputCmd.ab == 1) ? R"( +workshop_download_item )" + inputCmd.gameid + " " + inputCmd.modid + " +quit": R"()";

  // main command
  system(std::string{"sh ~/Steam/steamcmd.sh +force_install_dir " + inputCmd.dir + " +login " + inputCmd.user + inputCmd.pass + slash + idsm + woof(ids)}.c_str());
  
  // shows how much and what has downloaded
  std::string mods = (!inputCmd.modid.empty()) ? "" : R"(Mods: )" + std::to_string(step -1) + "\n";
  std::string colm = (inputCmd.ab == 1) ? R"(Mod)": R"(Collection)"; 
  std::cout << "\n\n" + mods + colm + " has been downloaded too: " + inputCmd.dir + "\n";

  return 1;
}
