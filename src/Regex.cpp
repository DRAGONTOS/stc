#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <mutex>
#include <ostream>
#include <string>
#include <sstream>
#include <thread>
#include "includes/Regex.hpp"

// pulls the mod or collection name out of the fetched workshop page's <title> tag
std::string workshoptitle(const std::string& sourcefile) {
    std::istringstream iss(sourcefile);
    for (std::string htmlLine; std::getline(iss, htmlLine); ) {
        if (htmlLine.find("<title>") == std::string::npos) {
            continue;
        }
        std::size_t start = htmlLine.find("Workshop::");
        if (start == std::string::npos) {
            continue;
        }
        std::size_t end = htmlLine.find_last_not_of(" \t\n\r\">");
        if (end != std::string::npos) {
            return htmlLine.substr(start + 10, end - start - 16);
        }
    }
    return "";
}

// looks up a mod's title in the page's embedded JSON, empty when the id is not on the page
std::string jsontitle(const std::string& sourcefile, const std::string& modId) {
    std::string searchPattern = "\"id\":\"" + modId + "\",\"title\":\"";
    std::size_t titlePos = sourcefile.find(searchPattern);
    if (titlePos == std::string::npos) {
        return "";
    }

    std::size_t titleStart = titlePos + searchPattern.length();
    std::size_t titleEnd   = sourcefile.find("\"", titleStart);
    if (titleEnd == std::string::npos) {
        return "";
    }
    return sourcefile.substr(titleStart, titleEnd - titleStart);
}

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

// guards the appends to a .stc-manifest from the install threads
static std::mutex manifestmutex;

// records an installed mod in the destination folder's .stc-manifest as gameid, modid and the folder name it lives in, so -u can update it in place
void recordinstall(const cmd *inputCmd, const std::string& idnumber, const std::string& idname) {
    std::lock_guard<std::mutex> lock(manifestmutex);

    std::filesystem::path manifestpath = std::filesystem::path(inputCmd->dir) / ".stc-manifest";
    std::string entry = std::string(inputCmd->gameid) + " " + idnumber + " " + idname;

    std::ifstream existing{manifestpath};
    std::string line;
    while (std::getline(existing, line)) {
        if (line == entry) {
            return;
        }
    }
    existing.close();

    std::ofstream manifest{manifestpath, std::ios::app};
    manifest << entry << "\n";
}

// renames modid to modname gathered from the html's metedata, retrying since steamcmd may still be settling the files
void filerestort(cmd *inputCmd, std::string idnumber, std::string idname) {
  std::filesystem::path steamdir  = std::string(inputCmd->userHome) + "/.cache/steamapps/workshop/content/" + inputCmd->gameid + "/" + idnumber;
  std::filesystem::path modname   = inputCmd->dir + "/" + idname;

  // renames and moves the files.
  for (int attempt = 0; attempt < 3; ++attempt) {
    try {
        if (std::filesystem::exists(steamdir)) {
            if (std::filesystem::exists(modname)) {
                std::filesystem::remove_all(modname);
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            std::filesystem::rename(steamdir, modname);
            recordinstall(inputCmd, idnumber, idname);
            return;
        } else {
            std::cerr << "Error: The directory " << steamdir << " does not exist." << std::endl;
            return;
        }
    } catch (const std::filesystem::filesystem_error& e) {
        std::cerr << "Filesystem error: " << e.what() << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
    }
  }
  std::cerr << "Failed after 3 attempts: could not move " << steamdir << std::endl;
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

    if (!inputCmd->collectionid.empty()) {
        idname = jsontitle(inputCmd->source, idnumber);
        filerestort(inputCmd, idnumber, idname);
    } else {
        idname = workshoptitle(inputCmd->source);
        filerestort(inputCmd, idnumber, idname);
    }
    }
}
