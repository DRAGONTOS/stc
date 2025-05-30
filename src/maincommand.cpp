#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <archive.h>
#include <archive_entry.h>
#include <ostream>
#include <sstream>
#include <stdexcept>
#include <sys/stat.h>
#include <string>
#include <thread>
#include <atomic>
#include "includes/Regex.hpp"
#include "includes/getHttp.hpp"
#include <tar.h>

// shows results
void execAndDisplay(cmd *inputCmd, const std::string& cmd, std::atomic<bool>& running) {
    char temp[128];
    // fixes a stupid compiler warning
    auto pipe_deleter = [](FILE* fp) { pclose(fp); };
    std::unique_ptr<FILE, decltype(pipe_deleter)> pipe(popen(cmd.c_str(), "r"), pipe_deleter);

    while (fgets(temp, sizeof(temp), pipe.get()) != nullptr) {
        std::string line(temp);  

        // checks for success.
        if (line.find("Success") != std::string::npos) {
            inputCmd->successes++;
            inputCmd->sucids.push_back( line + "\n");
        }

        // checks for timed out ones.
        if (line.find("Timeout") != std::string::npos) {
            inputCmd->timedout++;
        }

        // checks for errors.
        if (line.find("ERROR!") != std::string::npos) {
            inputCmd->errors++;
            // tmp fix for errors.
            std::ofstream meow{"/home/rander/.cache/errors.txt"};
            meow << "start:\n" + line; 
        }
    }
    running = false; 
}

// checks if steamexists
bool steamexists(const std::string& path) {
    return std::filesystem::exists(path) && std::filesystem::is_directory(path); 
}

// the targz thingie
void extractTarGz(const std::string& file, const std::string& outputDir) {
    struct archive *a = archive_read_new();
    archive_read_support_filter_gzip(a);
    archive_read_support_format_tar(a);
    
    struct archive *ext = archive_write_disk_new();
    archive_write_disk_set_options(ext, ARCHIVE_EXTRACT_TIME | ARCHIVE_EXTRACT_PERM);
    
    if (archive_read_open_filename(a, file.c_str(), 10240) == ARCHIVE_OK) {
        struct archive_entry *entry;
        while (archive_read_next_header(a, &entry) == ARCHIVE_OK) {
            std::string fullPath = outputDir + "/" + archive_entry_pathname(entry);
            archive_entry_set_pathname(entry, fullPath.c_str());
            archive_write_header(ext, entry);
            
            const void *buff;
            size_t size;
            la_int64_t offset;
            while (archive_read_data_block(a, &buff, &size, &offset) == ARCHIVE_OK) {
                archive_write_data_block(ext, buff, size, offset);
            }
            
            archive_write_finish_entry(ext);
        }
    }
    
    archive_read_free(a);
    archive_write_free(ext);
}

//setup for steamcmd
void setup(const std::string& dirname) {
    const char* userHome    = getenv("HOME");
    std::string steamdir    = dirname + "/Steam";
    std::string downloaddir = std::string(userHome) + "/.cache/steamcmd_linux.tar.gz";
    std::string moddir      = dirname + "/mods";

    // checks if the steamdir exists and or if steamcmd is installed or not
    if (steamexists(steamdir)) {
        } else {
            try {
                downloadFile("https://steamcdn-a.akamaihd.net/client/installer/steamcmd_linux.tar.gz", downloaddir);
                extractTarGz(downloaddir, std::string(userHome) + "/.local/share/stc/Steam");
            } catch (std::string &meow) {}
    }

    // Creates the default mods folder 
    if (mkdir(moddir.c_str(), 0777) == 0) {
    }
}

void installedmodslist(const std::string& cmd, std::string& sourcefile, std::string& collectionid, std::string& total) {
    int totalmods = 1;

    // fixes a stupid compiler warning
    auto pipe_deleter = [](FILE* fp) { pclose(fp); };
    std::unique_ptr<FILE, decltype(pipe_deleter)> pipe(popen(cmd.c_str(), "r"), pipe_deleter);
    
    char buffer[256];
    while (fgets(buffer, sizeof(buffer), pipe.get()) != nullptr) {

        //strings
        std::string line;
        std::string downloaderror;
        std::string downloadsuccess;

        std::istringstream inputStream(buffer);
        while (std::getline(inputStream, line)) {
        if (line.find("Download") != std::string::npos) {
        auto response = line.find("Success") != std::string::npos ? downloadsuccess = "Successfully downloaded: " 
                      : (line.find("ERROR") != std::string::npos ? downloaderror = "Error downloading: " : "");

        // if success trim
        if (!downloadsuccess.empty()) {
            std::string successidname;
            std::string successLine = line; 

            // Extract mod ID (e.g. "1508850027")
            size_t itemPos = successLine.find(" item ");
            size_t toPos   = successLine.find(" to ");
            
            if (itemPos != std::string::npos && toPos != std::string::npos && toPos > itemPos) {
                // Calculate positions correctly
                size_t idStart    = itemPos + 6; // Skip " item "
                size_t idLength   = toPos - idStart;
                std::string modId = successLine.substr(idStart, idLength);

                if (!collectionid.empty()) {
                    // Find mod title in sourcefile JSON
                    try {
                        // Search for JSON pattern: "id":"1508850027","title":"Mod Name"
                        std::string searchPattern = "\"id\":\"" + modId + "\",\"title\":\"";
                        size_t titlePos = sourcefile.find(searchPattern);
                        
                        if (titlePos != std::string::npos) {
                            size_t titleStart = titlePos + searchPattern.length();
                            size_t titleEnd   = sourcefile.find("\"", titleStart);
                            
                            if (titleEnd != std::string::npos) {
                                successidname = sourcefile.substr(titleStart, titleEnd - titleStart);
                            }
                        }
                    } catch (...) {
                        std::cerr << "Error parsing mod data" << std::endl;
                    }
                    std::cout << "[" << totalmods << "/" << total << "] " << downloadsuccess << successidname << std::endl;
                    totalmods++;
                } else {
                    // HTML/Workshop format parsing
                    std::istringstream iss(sourcefile);
                    for (std::string htmlLine; std::getline(iss, htmlLine); ) {
                        if (htmlLine.find("<title>") != std::string::npos) {
                            if (auto start = htmlLine.find("Workshop::"); start != std::string::npos) {
                                if (auto end = htmlLine.find_last_not_of(" \t\n\r\">"); end != std::string::npos) {
                                    successidname = htmlLine.substr(start + 10, end - start - 16);
                                    break;
                                }
                            }
                        }
                    }
                    std::cout << downloadsuccess << successidname << std::endl;
                }
            }
        }

        // if error trim
        if (!downloaderror.empty()) {
            std::string failedidname;
            std::string errorLine = line; 

            // Extract failed mod ID (e.g. "818773962")
            size_t itemPos    = errorLine.find(" item ");
            size_t failedPos  = errorLine.find(" failed ");
            std::string modId = errorLine.substr(itemPos, failedPos);
            
            if (itemPos != std::string::npos && failedPos != std::string::npos) {
                std::string modId = errorLine.substr(
                    itemPos + 6, // Skip " item "
                    failedPos - (itemPos + 6) // Length until " failed "
                );
                if (!collectionid.empty()) {
                    try {
                        // Look for pattern like "id":"818773962","title":"Actual Mod Name"
                        std::string searchPattern = "\"id\":\"" + modId + "\",\"title\":\"";
                        size_t titlePos = sourcefile.find(searchPattern);
                        
                        if (titlePos != std::string::npos) {
                            size_t titleStart = titlePos + searchPattern.length();
                            size_t titleEnd   = sourcefile.find("\"", titleStart);
                            
                            if (titleEnd != std::string::npos) {
                                failedidname = sourcefile.substr(titleStart, titleEnd - titleStart);
                            }
                        }
                        std::cout << downloaderror << failedidname << std::endl;
                    } catch (...) {
                        std::cerr << "Error parsing mod data" << std::endl;
                    }
                std::cout << "[" << totalmods << "/" << total << "] " << downloaderror << failedidname << std::endl;
                } else {
                    std::istringstream iss(sourcefile);
                    for (std::string htmlLine; std::getline(iss, htmlLine); ) {
                        if (htmlLine.find("<title>") != std::string::npos) {
                            if (auto start = htmlLine.find("Workshop::"); start != std::string::npos) {
                                if (auto end = htmlLine.find_last_not_of(" \t\n\r\">"); end != std::string::npos) {
                                    failedidname = htmlLine.substr(start + 10, end - start - 16);
                                    break;
                                }
                            }
                        }
                    }
                    std::cout << downloaderror << failedidname << std::endl;
                }
            }
            }
        }
    }
    }
}

// main function
void maincommand(cmd *inputCmd) {
  //setup for steamcmd
  setup(std::string(inputCmd->userHome) + "/.local/share/stc");
  std::istringstream collectionsource(inputCmd->source);
 
  // clears cache
  std::filesystem::remove_all(std::string(inputCmd->userHome) + "/.cache/steamapps");
  std::filesystem::remove_all(std::string(inputCmd->userHome) + "/.cache/steamcmd_linux.tar.gz");

  std::string idsm         = " +workshop_download_item " + inputCmd->gameid + " " + inputCmd->modid + " +quit";
  std::string sourcefile   = inputCmd->source; // needed for some void functions 
  std::string collectionid = inputCmd->collectionid; 

  std::string collectionline;
  std::string title;
  std::string totalmods;

  // description collection
  const std::string startdesPattern = "A collection of ";
  const std::string enddesPattern   = " items created by";

  // title of mod or collection
  const std::string startPattern    = "<title>Steam Workshop::";
  const std::string endPattern      = "</title>";

  struct stat sb;

  // sets dir to default mods folder if dir is empty or invalid.
  if (stat(inputCmd->dir.c_str(), &sb) == 0 && S_ISDIR(sb.st_mode)) { 
  } else {
        inputCmd->dir = std::string(inputCmd->userHome) + "/.local/share/stc/mods";
  }

  // steamcmd command
  std::string maincommand2 = std::string{"sh " 
     + std::string(inputCmd->userHome) 
     + "/.local/share/stc/Steam/steamcmd.sh +force_install_dir "
     + std::string(inputCmd->userHome) 
     + "/.cache"
     + " +login " 
     + inputCmd->user 
     + ((inputCmd->pass.empty()) ? "" : inputCmd->pass)  
     + " "
     + ((inputCmd->collectionid.empty()) ? idsm: inputCmd->ids)}.c_str();

 // title of mod or collection
 while (std::getline(collectionsource, collectionline)) {
     size_t startPos2 = collectionline.find(startdesPattern);
     if (startPos2 != std::string::npos) {
         size_t endPos2 = collectionline.find(enddesPattern, startPos2);
         if (endPos2 != std::string::npos) {
             // Extract content between <title> and </title>
             totalmods = collectionline.substr(
                 startPos2 + startdesPattern.length(),
                 endPos2 - (startPos2 + startdesPattern.length())
             );
        }
     }

     size_t startPos = collectionline.find(startPattern);
     if (startPos != std::string::npos) {
         size_t endPos = collectionline.find(endPattern, startPos);
         if (endPos != std::string::npos) {
             // Extract content between <title> and </title>
             title = collectionline.substr(
                 startPos + startPattern.length(),
                 endPos - (startPos + startPattern.length())
             );

         }
     }
 }

  // executes maincommand2
  try {
     // std::cout << "Modid:                     Latest Version:                     Authors:\n" 
     //           << "=======================================================================\n\n";
     std::cout << "=======================================================================\n\n";
     std::atomic<bool> running(true); 
     
     if (!inputCmd->collectionid.empty()) {
        std::cout << "Installing collection: " << title << std::endl;
        std::cout << "\nCollection Summary:\n Installing:         " << totalmods << " Mods" << std::endl;
     } else {
        std::cout << "Installing mod: " << title << "\n";
     }
     
     // y\n prompt
     std::cout << "\nIs this ok [y/N]: ";

     char input;
     std::cin >> input;

     if (std::tolower(input) == 'y') {
        // handle 'y' or 'Y' input
     } else if (std::tolower(input) == 'n') {
        // handle 'n' or 'N' input
        std::cout << "Operation aborted by the user.\n";
        std::exit(0);
     }
     std::cout << std::endl;

     std::thread installedmods([&running, maincommand2, &sourcefile, &collectionid, &totalmods]() {
     installedmodslist(maincommand2, sourcefile, collectionid, totalmods);
     });
      
     installedmods.join();
     execAndDisplay(inputCmd, maincommand2, running); 

     running = false; 
     std::cout << "\n=======================================================================" << std::flush;
  } catch (const std::runtime_error& e) {
     std::cerr << "\nError: " << e.what() << "\n"; 
  }

  // mod or collection?
  std::string total = (inputCmd->collectionid.empty()) ? "Total: 1" : "Total: " + std::to_string(inputCmd->totalmods) + "\n";
  std::string colm  = (inputCmd->collectionid.empty()) ? R"(Mod)"   : R"(Collection)"; 

  // shows how much and what has downloaded
  std::string mods = total  
    + "\n"
    + "Finished: "  + std::to_string(inputCmd->successes)     + "\n"
    + "Timed out: " + std::to_string(inputCmd->timedout)      + "\n"
    + "Errored: "   + std::to_string(inputCmd->errors)        + "\n" + "\n";

    if (!inputCmd->collectionid.empty()) {
     //start the threads
        for(size_t nya{0}; nya < inputCmd->sucids.size(); ++nya){
            inputCmd->totalmods++; 
            std::thread meowT {Modname, inputCmd, nya};
            meowT.detach();
        }
        while(inputCmd->threadsCompleted != inputCmd->sucids.size()){}
    } else {
        for(size_t nya{0}; nya < inputCmd->sucids.size(); ++nya) {
            inputCmd->totalmods++;
            Modname(inputCmd, nya);  // Process directly instead of in thread
        }
    }
    std::cout << "\n\n" + mods + colm + " has been downloaded too: " + inputCmd->dir + "\n";
}
