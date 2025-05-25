#include <filesystem>
#include <iostream>
#include <ostream>
#include <string>
#include <sstream> 
#include "regex"
#include "includes/Regex.hpp"

// regex and stuff (collectionid)
void Regex(cmd *inputCmd) {
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

    std::istringstream inputStreamids(inputCmd->ids);
    std::string lineids;

    // Count the number of lines
    while (std::getline(inputStreamids, lineids)) {
        inputCmd->totalmods++; 
    }
}

// renames modid to modname gathered from the html's metedata
void filerestortthreaded(cmd *inputCmd, std::string idnumber, std::string idname) {
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

// void filerestort(cmd *inputCmd) {
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
      } else {
          std::cerr << "Error: The directory " << steamdir << " does not exist." << std::endl;
      }
  } catch (const std::filesystem::filesystem_error& e) {
      std::cerr << "Filesystem error: " << e.what() << std::endl;
  } catch (const std::exception& e) {
      std::cerr << "Error: " << e.what() << std::endl;
  }
}

void Modname(cmd *inputCmd, size_t index) {
  std::string idnumber;
  std::string idname;
    // std::cout << std::string(inputCmd->source) << std::endl;
  if (!inputCmd->sucids.empty()) {
    std::regex downloadItemRegex(R"(Downloaded item (\d+))");

    std::smatch match;
    if (std::regex_search(inputCmd->sucids[index], match, downloadItemRegex)) {
        std::string downloadItemNumber = match[1].str();
        idnumber = downloadItemNumber;
    }

    std::istringstream inputStream(inputCmd->source);
    if (!inputCmd->collectionid.empty()) {
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
        filerestortthreaded(inputCmd, idnumber, idname);
    } else {
        std::regex grepRegex("Workshop::");
        std::string line;
        while (std::getline(inputStream, line)) {
        if (std::regex_search(line, grepRegex)) {

            std::size_t titleStart = line.find("<title>");
            if (titleStart != std::string::npos) {
                std::size_t titleEnd = line.find("</title>", titleStart);
                if (titleEnd != std::string::npos) {
                    std::string fullTitle = line.substr(titleStart + 7, titleEnd - (titleStart + 7));
                    
                    std::size_t workshopPos = fullTitle.find("Workshop::");
                    if (workshopPos != std::string::npos) {
                        idname = fullTitle.substr(workshopPos + 10); // 10 is length of "Workshop::"
                        
                        idname.erase(idname.find_last_not_of(" \t\n\r\">") + 1);
                    }
                }
            }
        }
    }
        filerestort(inputCmd, idnumber, idname);
    }
    }
}
