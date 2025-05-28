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
        std::string line;

        try {
            std::string searchPattern = "\"id\":\"" + idnumber + "\",\"title\":\"";
            size_t titlePos = inputCmd->source.find(searchPattern);
            
            if (titlePos != std::string::npos) {
                size_t titleStart = titlePos + searchPattern.length();
                size_t titleEnd = inputCmd->source.find("\"", titleStart);
                
                if (titleEnd != std::string::npos) {
                    line = inputCmd->source.substr(titleStart, titleEnd - titleStart);
                }
                idname = line;
            }
        } catch (...) {
            std::cerr << "Error parsing mod data" << std::endl;
        }
        filerestortthreaded(inputCmd, idnumber, idname);
    } else {
        std::istringstream iss(inputCmd->source);
        for (std::string htmlLine; std::getline(iss, htmlLine); ) {
            if (htmlLine.find("<title>") != std::string::npos) {
                if (auto start = htmlLine.find("Workshop::"); start != std::string::npos) {
                    if (auto end = htmlLine.find_last_not_of(" \t\n\r\">"); end != std::string::npos) {
                        idname = htmlLine.substr(start + 10, end - start - 16);
                        break;
                    }
                }
            }
        }
        filerestort(inputCmd, idnumber, idname);
    }
    }
}
