#include <cctype>
#include <cpptoml.h>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <ios>
#include <iostream>
#include <ostream>
#include <string>
#include "regex"
#include "includes/Regex.hpp"

// regex and stuff (collectionid)
void Regex(cmd *inputCmd) {
  if (!inputCmd->collectionid.empty()) {
    // Input and output file paths
    std::string inputFilePath   = inputCmd->cachesc;
    std::string outputFilePath  = inputCmd->cacheid;

    // Open the input file (source.html)
    std::ifstream inputFile(inputFilePath);
    std::ofstream outputFile(outputFilePath, std::ios::app);

    if (!inputFile.is_open() && !outputFile.is_open()) {
      throw("Unable to open file\n");
    }

    std::regex grepRegex(R"(<div class="workshopItemPreviewHolder  ")");

    std::string line;

    // Process each line
  while (std::getline(inputFile, line)) {
      // grep-like behavior (only process lines containing the pattern with two
      // spaces)
      if (std::regex_search(line, grepRegex)) {

     // sed 's/"><div class=.*//'
        std::size_t divPos = line.find("\"><div class=");
        if (divPos != std::string::npos) {
          line = line.substr(0, divPos); // Trim everything after '"><div class='
        }

        // sed 's/.*id=//'
        std::size_t idPos = line.find("id=");
        if (idPos != std::string::npos) {
          line = line.substr(idPos + 3); // Trim everything before 'id=' and keep the ID
        }

        line = "+workshop_download_item " + inputCmd->gameid + " " + line;

        line += " \\";

        outputFile << line << std::endl;
      }
    }

    // Step 6: Write "+quit" at the end of the output file
    outputFile << "+quit" << std::endl;

    // Close the input and output files
    inputFile.close();
    outputFile.close();
  }  
}
