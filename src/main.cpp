#include <cctype>
#include <cpptoml.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include "includes/getHttp.h"
#include <ios>
#include <iostream>
#include <ostream>
#include "regex"
#include <string>

std::string woof(std::ifstream& meow){
  std::ostringstream nya;
  nya << meow.rdbuf();
  return nya.str();
}

const std::string USAGE = R"#(usage:  Steam Collector [flags] [<command> [args]]
LISTING COMMANDS:
    -c:           Install a collection.
    -m:           Install a specific mod.
)#";

// strings are kinda broken
const std::string invalidvalue  = "Invalid value in config: \n";
const std::string invalidconfig = "Not a valid config: \n";

int main(int argc, char **argv, char **envp) {

  // need some cleaning in the future ata
  const char* userHome  = getenv("HOME");
  std::string userCache = std::string(userHome)   + "/.cache/";
  std::string cacheid   = std::string(userCache)  + "ids.txt";
  std::string cachesc   = std::string(userCache)  + "sources.html";

  std::string collectionid;
  std::string modid;
  std::string user = "anonymous";
  std::string pass;
  std::string gameid;
  std::string dir;
  dir = std::filesystem::current_path();

  // Removes cache
  if (std::filesystem::exists(cacheid) &&
      std::filesystem::is_directory(cacheid)) {
    int status  = remove(std::string {cacheid}.c_str());
    int status2 = remove(std::string {cachesc}.c_str());
  } else {
    int status  = remove(std::string {cacheid}.c_str());
    int status2 = remove(std::string {cachesc}.c_str());
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
        //collectionid
        if (ARGS[i] == "-c") {
          if (argc < 3 || argv[2][0] == '-') {
              std::cerr << USAGE.c_str();
              return 1;
          }

          if (argc == 6 || argc == 7) {
              
            collectionid  = ARGS[1+1];
            user          = ARGS[1+2];
            pass          = ARGS[1+3];
            gameid        = ARGS[1+4];

            if (argc == 7) {
            dir = ARGS[1+5];
            }
              
            getHttp(std::string {"https://steamcommunity.com/sharedfiles/filedetails/?id=" + collectionid}, &cachesc);
            std::cout << collectionid << user << pass << gameid << dir;
            std::cout << "success1\n";
            break;
        }
            
            if (argc == 4 || argc == 5) {

            collectionid  = ARGS[1+1];
            gameid        = ARGS[1+2];
            
            if (argc == 5) {
            dir = ARGS[1+3];
            }

            getHttp(std::string {"https://steamcommunity.com/sharedfiles/filedetails/?id=" + collectionid}, &cachesc);
            std::cout << collectionid << gameid << dir;

            std::cout << "success\n";
            break;
            } else {
              std::cerr << USAGE;
              return 1;
            }
        }

        // modid
        if (ARGS[i] == "-m") {
          if (argc < 3 || argv[2][0] == '-') {
              std::cerr << USAGE.c_str();
              return 1;
          }

          if (argc == 6 || argc == 7) {
              
            modid  = ARGS[1+1];
            user          = ARGS[1+2];
            pass          = ARGS[1+3];
            gameid        = ARGS[1+4];

            if (argc == 7) {
            dir = ARGS[1+5];
            }
              
            std::cout << "success1\n";
            break;
          }

          if (argc == 4 || argc == 5) {

          modid  = ARGS[1+1];
          gameid        = ARGS[1+2];
          
          if (argc == 5) {
          dir = ARGS[1+3];
          }

          std::cout << "success\n";
          break;
          } else {
            std::cerr << USAGE;
            return 1;
          }
        } else {
           std::cerr << USAGE;
           return 1;
        }
      }
   }

  // regex and stuff (collectionid)
  if (!collectionid.empty()) {
    // Input and output file paths
    std::string inputFilePath   = cachesc;
    std::string outputFilePath  = cacheid;

    // Open the input file (source.html)
    std::ifstream inputFile(inputFilePath);
    std::ofstream outputFile(outputFilePath, std::ios::app);

    if (!inputFile.is_open() && !outputFile.is_open()) {
        std::cerr << "Unable to open file";
        return 1;
    }

    std::regex grepRegex(R"(<div class="workshopItemPreviewHolder  ")");

    std::string line;

    // Process each line
    while (std::getline(inputFile, line)) {
        //grep-like behavior (only process lines containing the pattern with two spaces)
        if (std::regex_search(line, grepRegex)) {

            //sed 's/"><div class=.*//'
            std::size_t divPos = line.find("\"><div class=");
            if (divPos != std::string::npos) {
                line = line.substr(0, divPos);  // Trim everything after '"><div class='
            }

            //sed 's/.*id=//'
            std::size_t idPos = line.find("id=");
            if (idPos != std::string::npos) {
                line = line.substr(idPos + 3);  // Trim everything before 'id=' and keep the ID
            }

            line = "+workshop_download_item " + gameid + " " + line;

            line += " \\";

            outputFile << line << std::endl;
        }
    }

    // Step 6: Write "+quit" at the end of the output file
    outputFile << "+quit" << std::endl;

    // Close the input and output files
    inputFile.close();
    outputFile.close();

  // (modid)
  } else if (!modid.empty()) {
    std::string wpd = " +workshop_download_item " + gameid + " " + modid /*+ R"( \)"*/;
    
    // main command
    system(std::string {"sh ~/Steam/steamcmd.sh +force_install_dir " + dir + " +login " + user + pass + wpd + " +quit"}.c_str());

    return 1;
  }

  // main command
  std::ifstream ids{cacheid};
  system(std::string {"sh ~/Steam/steamcmd.sh +force_install_dir " + dir + " +login " + user + pass + R"( \ )" + woof(ids)}.c_str());
}
