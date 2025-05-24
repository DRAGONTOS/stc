#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <archive.h>
#include <archive_entry.h>
#include <istream>
#include <ostream>
#include <stdexcept>
#include <sys/stat.h>  // for mkdir()
#include <string>
#include <thread>
#include <chrono>
#include <atomic>
#include "includes/Regex.hpp"
#include "includes/getHttp.hpp"
#include <tar.h>
#include <zlib.h>


void execAndDisplay(cmd *inputCmd, const std::string& cmd, std::atomic<bool>& running) {
    char temp[128];

    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd.c_str(), "r"), pclose);

    if (!pipe) {
        throw std::runtime_error("popen() failed!");
    }

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

// loading cursor
void showLoadingCursor(std::atomic<bool>& running) {
    const char* cursor = "|/-\\";
    int i = 0;

    while (running) {
        std::cout << "\r" << cursor[i++ % 4] << " Getting Mods...";
        std::cout.flush();
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

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
    const char* userHome  = getenv("HOME");
    std::string steamdir = dirname + "/Steam";
    std::string downloaddir = std::string(userHome) + "/.cache/steamcmd_linux.tar.gz";

    if (mkdir(dirname.c_str(), 0777) == 0) {
    } else {}
    // checks if the steamdir exists and or if steamcmd is installed or not
    if (steamexists(steamdir)) {
        } else {
            try {
                downloadFile("https://steamcdn-a.akamaihd.net/client/installer/steamcmd_linux.tar.gz", downloaddir);
                extractTarGz(downloaddir, std::string(userHome) + "/.local/share/stc/Steam");
            } catch (std::string &meow) {}
    }
}

void maincommand(cmd *inputCmd) {
  //setup for steamcmd
  setup(std::string(inputCmd->userHome) + "/.local/share/stc");

  std::string idsm = R"( +workshop_download_item )" + inputCmd->gameid + " " + inputCmd->modid + " +quit";

  std::filesystem::remove_all(std::string(inputCmd->userHome) + "/.cache/steamapps");
  std::filesystem::remove_all(std::string(inputCmd->userHome) + "/.cache/steamcmd_linux.tar.gz");

  std::string maincommand2 = std::string{"sh " 
     + std::string(inputCmd->userHome) 
     + "/.local/share/stc/Steam/steamcmd.sh +force_install_dir "
     + std::string(inputCmd->userHome) 
     + "/.cache"
     + " +login " 
     + inputCmd->user 
     + ((inputCmd->pass.empty()) ? "" : inputCmd->pass)  
     + " "
     + ((inputCmd->ab == true) ? idsm: inputCmd->ids)}.c_str();

  try {
      std::atomic<bool> running(true); 
      std::thread cursorThread(showLoadingCursor, std::ref(running)); 
      execAndDisplay(inputCmd, maincommand2, running); 

      running = false; 
      cursorThread.join();
  } catch (const std::runtime_error& e) {
      std::cerr << "\nError: " << e.what() << "\n"; 
  }

  // mod or collection?
  std::string total = (inputCmd->ab == true) ? "Total: 1" : "Total: " + std::to_string(inputCmd->totalmods -1) + "\n";
  std::string colm  = (inputCmd->ab == true) ? R"(Mod)"   : R"(Collection)"; 

  // shows how much and what has downloaded
  std::string mods = total  
    + "\n"
    + "Finished: "  + std::to_string(inputCmd->successes)     + "\n"
    + "Timed out: " + std::to_string(inputCmd->timedout)      + "\n"
    + "Errored: "   + std::to_string(inputCmd->errors)        + "\n" + "\n";

    //start the threads
    for(size_t nya{0}; nya < inputCmd->sucids.size(); ++nya){
      inputCmd->totalmods++; 
      std::thread meowT {Modname, inputCmd, nya};
      meowT.detach();
    }

  while(inputCmd->threadsCompleted != inputCmd->sucids.size()){}
  std::cout << "\n\n" + mods + colm + " has been downloaded too: " + inputCmd->dir + "\n";
}
