#include <filesystem>
#include <iostream>
#include <ostream>
#include <string>
#include <sstream> 
#include "regex"
#include "includes/Regex.hpp"

// regex and stuff (collectionid)
void Regex(cmd *inputCmd) {
    if (!inputCmd->collectionid.empty()) {
        std::istringstream inputStream(inputCmd->source);

        // Regex to search for the desired pattern
        std::regex grepRegex(R"(<div class="workshopItemPreviewHolder  )");
        std::string line;

        // Process each line from the input file
        while (std::getline(inputStream, line)) {
            // grep-like behavior (only process lines containing the pattern with two spaces)
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

                inputCmd->ids += line + "\n"; // Write to output buffer
            }
        }

        // Step 6: Write "+quit" at the end of the output buffer
        inputCmd->ids += "+quit\n";
    }

    std::istringstream inputStream(inputCmd->ids);
    std::string line;

    // Count the number of lines
    while (std::getline(inputStream, line)) {
        inputCmd->totalmods++; 
    }
}

void filerestort(cmd *inputCmd, std::string idnumber, std::string idname) {
  std::filesystem::path steamdir  = std::string(inputCmd->userHome) + "/.cache/steamapps/workshop/content/" + inputCmd->gameid + "/" + idnumber;
  std::filesystem::path modname   = inputCmd->dir + "/" + idname;

  // renames and moves the files.
  try {
      if (std::filesystem::exists(steamdir)) {
          if (std::filesystem::exists(modname)) {
              std::filesystem::remove_all(modname); 
          }
          std::filesystem::rename(steamdir, modname);
          inputCmd->threadsCompleted++;
      } else {
          std::cerr << "Error: The directory " << steamdir << " does not exist." << std::endl;
          inputCmd->threadsCompleted++;
      }
  } catch (const std::filesystem::filesystem_error& e) {
      std::cerr << "Filesystem error: " << e.what() << std::endl;
      inputCmd->threadsCompleted++;

  } catch (const std::exception& e) {
      std::cerr << "Error: " << e.what() << std::endl;
      inputCmd->threadsCompleted++;

  }
}

void Modname(cmd *inputCmd, size_t index) {
  std::string idnumber;
  std::string idname;
  if (!inputCmd->sucids.empty()) {
    std::regex downloadItemRegex(R"(Downloaded item (\d+))");

    std::smatch match;
    if (std::regex_search(inputCmd->sucids[index], match, downloadItemRegex)) {
        std::string downloadItemNumber = match[1].str();
        idnumber = downloadItemNumber;
    }

    std::istringstream inputStream(inputCmd->source);
    
    std::regex grepRegex("\"id\":\"" + idnumber + "\",\"title\":\"");
    std::string line;

    while (std::getline(inputStream, line)) {

        if (std::regex_search(line, grepRegex)) {

            std::size_t divPos = line.find("\",\"description\":");
            if (divPos != std::string::npos) {
                line = line.substr(0, divPos); // Trim everything after '"><div class='
            }

            std::string idPrefix = "\",\"title\":\""; // The prefix to search for
            std::size_t idPos = line.find(idPrefix);
            if (idPos != std::string::npos) {
                line = line.substr(idPos + idPrefix.length()); // Keep everything after "id="
            }
            idname = line;
        }
    }
    filerestort(inputCmd, idnumber, idname);
  }
}
