#include <filesystem>
#include <iostream>
#include <ostream>
#include <string>
#include <sstream> 
#include "includes/Regex.hpp"

void Regex(cmd* inputCmd) {
    std::istringstream inputStream(inputCmd->source);
    std::string line;
    
    const std::string searchPattern = "<div class=\"workshopItemPreviewHolder  ";
    const std::string divPattern = "\"><div class=";
    const std::string idPattern = "id=";

    while (std::getline(inputStream, line)) {
        if (line.find(searchPattern) != std::string::npos) {
            // Extract the ID portion
            size_t divPos = line.find(divPattern);
            if (divPos != std::string::npos) {
                line = line.substr(0, divPos);
            }

            size_t idPos = line.find(idPattern);
            if (idPos != std::string::npos) {
                line = line.substr(idPos + idPattern.length());
            }

            // Format the output line
            inputCmd->ids += "+workshop_download_item " + inputCmd->gameid + " " + line + " \\\n";
            inputCmd->totalmods++;
        }
    }

    // Add the quit command
    inputCmd->ids += "+quit\n";
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

    if (!inputCmd->sucids.empty()) {
        const std::string& line  = inputCmd->sucids[index];
        const std::string prefix = "Downloaded item ";
        
    std::size_t pos = line.find(prefix);
    if (pos != std::string::npos) {
        std::size_t numStart = pos + prefix.length();
        std::size_t numEnd = line.find_first_not_of("0123456789", numStart);
        if (numEnd == std::string::npos) numEnd = line.length();
        idnumber = line.substr(numStart, numEnd - numStart);
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
