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
#include "regex"
#include <string>
// should be in an alphabatical order ^^^

// clear buffers might not be needed
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

// strings are kinda broken
const std::string invalidvalue = "Invalid value in config: ";
const std::string invalidconfig = "Not a valid config: ";

int main(int argc, char **argv, char **envp) {

  // need some cleaning in the future ata
  const char* userHome = getenv("HOME");
  std::string userCache = std::string(userHome) + "/.cache/";
  std::string cacheid = std::string(userCache) + "ids.txt";
  std::string cachesc = std::string(userCache) + "sources.html";

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
    int status = remove(std::string {cacheid}.c_str());
    int status2 = remove(std::string {cachesc}.c_str());
  } else {
    int status = remove(std::string {cacheid}.c_str());
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

      if (ARGS[i] == "-ci") { // collection id
       collectionid = ARGS[1+1];
      std::cout << collectionid;
      // should be done with libcurl in the future ata
      system(std::string {"curl https://steamcommunity.com/sharedfiles/filedetails/?id=" + collectionid + " -o " + cachesc}.c_str());
      } else if (ARGS[i] == "-mi") {
      modid = ARGS[1+1];
      std::cout << modid;
      }

      if (ARGS[i+2] == "-u") { // user if required [OPTIONAL]
        user = ARGS[i+3];
        std::cout << user;
      } else if (ARGS[i+2] == "-gi") { // game id
        gameid = ARGS[i+3];
        std::cout << gameid;
        if (ARGS[i+4] == "-d") {
          dir = ARGS[i+5];
          std::cout << dir;
        }
        break;
      }

      if (ARGS[i+4] == "-p") { // pass [OPTIONAL]
        pass = ARGS[i+5];
        std::cout << pass;
      }

      if (ARGS[i+6] == "-gi") { // game id
        gameid = ARGS[i+7];
        std::cout << gameid;
      }

      if (ARGS[i+8] == "-d") { // download dir [OPTIONAL]
        dir = ARGS[i+9];
        std::cout << dir;
      }


    }

  if (!collectionid.empty()) {
    // Input and output file paths
    std::string inputFilePath = cachesc;
    std::string outputFilePath = cacheid;

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

  } else if (!modid.empty()) {
    std::string wpd = " +workshop_download_item " + gameid + " " + modid /*+ R"( \)"*/;
    system(std::string {"sh ~/Steam/steamcmd.sh +force_install_dir " + dir + " +login " + user + pass + wpd + " +quit"}.c_str());

    return 1;
  }

  // main command
  std::ifstream ids{cacheid};
  system(std::string {"sh ~/Steam/steamcmd.sh +force_install_dir " + dir + " +login " + user + pass + R"( \ )" + woof(ids)}.c_str());
}
