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
#include <sys/wait.h>
#include <spawn.h>
#include <unistd.h>
#include <string>
#include <thread>
#include <vector>
#include <mutex>
#include "includes/Regex.hpp"
#include "includes/getHttp.hpp"

extern char **environ;

// a spawned steamcmd: its stdout as a stream, the thread feeding its stdin and its pid for reaping
struct steamproc {
    std::FILE* out        = nullptr;
    pid_t      pid        = -1;
    std::thread stdinwriter;

    // joins the stdin writer, closes the stdout stream and reaps the child
    void close() {
        if (stdinwriter.joinable()) {
            stdinwriter.join();
        }
        if (out != nullptr) {
            std::fclose(out);
            out = nullptr;
        }
        if (pid > 0) {
            int status;
            waitpid(pid, &status, 0);
            pid = -1;
        }
    }
};

// spawns steamcmd with an argument array instead of a shell string; the login password and the download commands go over stdin, so they never appear in the process list
steamproc spawnSteamcmd(const cmd *inputCmd, const std::string& commands) {
    std::string steamcmd = std::string(inputCmd->userHome) + "/.local/share/stc/Steam/steamcmd.sh";
    if (!std::filesystem::exists(steamcmd)) {
        std::cerr << "steamcmd is not installed at " << steamcmd << ", run stc -c or stc -m once to set it up." << std::endl;
        return {};
    }

    std::vector<std::string> args{
        "sh",
        steamcmd,
        "+force_install_dir",
        std::string(inputCmd->userHome) + "/.cache",
        "+login",
        inputCmd->user,
    };

    std::string stdinlines;
    if (!inputCmd->pass.empty()) {
        stdinlines += inputCmd->pass + "\n";
    }
    stdinlines += commands;

    int outpipe[2];
    int inpipe[2];
    if (pipe(outpipe) != 0) {
        return {};
    }
    if (pipe(inpipe) != 0) {
        close(outpipe[0]);
        close(outpipe[1]);
        return {};
    }

    posix_spawn_file_actions_t actions;
    posix_spawn_file_actions_init(&actions);
    posix_spawn_file_actions_adddup2(&actions, inpipe[0], STDIN_FILENO);
    posix_spawn_file_actions_adddup2(&actions, outpipe[1], STDOUT_FILENO);
    posix_spawn_file_actions_addclose(&actions, outpipe[0]);
    posix_spawn_file_actions_addclose(&actions, inpipe[1]);
    posix_spawn_file_actions_addclose(&actions, inpipe[0]);
    posix_spawn_file_actions_addclose(&actions, outpipe[1]);

    std::vector<char*> argv;
    for (const std::string& arg : args) {
        argv.push_back(const_cast<char*>(arg.c_str()));
    }
    argv.push_back(nullptr);

    pid_t pid;
    int spawned = posix_spawnp(&pid, "sh", &actions, nullptr, argv.data(), environ);
    posix_spawn_file_actions_destroy(&actions);
    close(inpipe[0]);
    close(outpipe[1]);

    if (spawned != 0) {
        close(inpipe[1]);
        close(outpipe[0]);
        return {};
    }

    steamproc proc;
    proc.pid = pid;
    proc.out = fdopen(outpipe[0], "r");
    if (proc.out == nullptr) {
        close(outpipe[0]);
        close(inpipe[1]);
        return proc;
    }

    int infd = inpipe[1];
    proc.stdinwriter = std::thread([stdinlines = std::move(stdinlines), infd]() {
        size_t written = 0;
        while (written < stdinlines.size()) {
            ssize_t chunk = write(infd, stdinlines.data() + written, stdinlines.size() - written);
            if (chunk <= 0) {
                break;
            }
            written += static_cast<size_t>(chunk);
        }
        close(infd);
    });
    return proc;
}

// shows results
void execAndDisplay(cmd *inputCmd, const std::string& commands) {
    char temp[128];
    steamproc proc = spawnSteamcmd(inputCmd, commands);

    while (proc.out != nullptr && fgets(temp, sizeof(temp), proc.out) != nullptr) {
        std::string line(temp);  

        // checks for success.
        if (line.find("Success") != std::string::npos) {
            inputCmd->successes++;
            inputCmd->sucids.push_back( line + "\n");
            std::cout << line << std::endl;
        }

        // checks for timed out ones.
        if (line.find("Timeout") != std::string::npos) {
            inputCmd->timedout++;
        }

        // checks for errors.
        if (line.find("ERROR!") != std::string::npos) {
            inputCmd->errors++;
            // tmp fix for errors.
            std::ofstream meow{std::string(inputCmd->userHome) + "/.cache/errors.txt", std::ios::app};
            meow << line;
        }
    }
    proc.close();
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
    std::string home        = getenv("HOME") == nullptr ? "/tmp" : getenv("HOME");
    std::string steamdir    = dirname + "/Steam";
    std::string downloaddir = home + "/.cache/steamcmd_linux.tar.gz";
    std::string moddir      = dirname + "/mods";

    // checks if the steamdir exists and or if steamcmd is installed or not
    if (steamexists(steamdir)) {
        } else {
            try {
                downloadFile("https://steamcdn-a.akamaihd.net/client/installer/steamcmd_linux.tar.gz", downloaddir);
                extractTarGz(downloaddir, home + "/.local/share/stc/Steam");
            } catch (const std::exception& e) {
                std::cerr << "Error installing steamcmd: " << e.what() << std::endl;
            }
    }

    // Creates the default mods folder 
    mkdir(moddir.c_str(), 0777);
}

void installedmodslist(const cmd *inputCmd, const std::string& commands, const std::string& sourcefile, const std::string& collectionid, const std::string& total) {
    int totalmods = 1;

    steamproc proc = spawnSteamcmd(inputCmd, commands);

    char buffer[256];
    while (proc.out != nullptr && fgets(buffer, sizeof(buffer), proc.out) != nullptr) {

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
                    successidname = jsontitle(sourcefile, modId);
                    std::cout << "[" << totalmods << "/" << total << "] " << downloadsuccess << successidname << std::endl;
                    totalmods++;
                } else {
                    successidname = workshoptitle(sourcefile);
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
            
            if (itemPos != std::string::npos && failedPos != std::string::npos) {
                std::string modId = errorLine.substr(
                    itemPos + 6, // Skip " item "
                    failedPos - (itemPos + 6) // Length until " failed "
                );
                if (!collectionid.empty()) {
                    failedidname = jsontitle(sourcefile, modId);
                    std::cout << "[" << totalmods << "/" << total << "] " << downloaderror << failedidname << std::endl;
                } else {
                    failedidname = workshoptitle(sourcefile);
                    std::cout << downloaderror << failedidname << std::endl;
                }
            }
            }
        }
    }
    }
    proc.close();
}

// main function
void maincommand(cmd *inputCmd) {
  //setup for steamcmd
  setup(std::string(inputCmd->userHome) + "/.local/share/stc");

  // clears cache
  std::filesystem::remove_all(std::string(inputCmd->userHome) + "/.cache/steamapps");
  std::filesystem::remove_all(std::string(inputCmd->userHome) + "/.cache/steamcmd_linux.tar.gz");

  std::string sourcefile   = inputCmd->source; // needed for some void functions
  std::string collectionid = inputCmd->collectionid;

  // the steamcmd stdin commands; the password, if any, is added by spawnSteamcmd
  std::string commands;
  if (!inputCmd->collectionid.empty()) {
      std::istringstream ids(inputCmd->ids);
      for (std::string idline; std::getline(ids, idline); ) {
          if (!idline.empty() && idline.back() == '\\') {
              idline.pop_back();
          }
          if (idline.empty() || idline.find("+quit") != std::string::npos) {
              continue;
          }
          commands += idline + "\n";
      }
  } else {
      commands += "+workshop_download_item " + inputCmd->gameid + " " + inputCmd->modid + "\n";
  }
  commands += "+quit\n";

  std::string title;
  std::string totalmods;

  // description collection
  const std::string startdesPattern = "A collection of ";
  const std::string enddesPattern   = " items created by";

  struct stat sb;

  // sets dir to default mods folder if dir is empty or invalid.
  if (stat(inputCmd->dir.c_str(), &sb) == 0 && S_ISDIR(sb.st_mode)) {
  } else {
        inputCmd->dir = std::string(inputCmd->userHome) + "/.local/share/stc/mods";
  }

  // how many mods the collection holds, and the title of mod or collection
  size_t startPos2 = sourcefile.find(startdesPattern);
  if (startPos2 != std::string::npos) {
      size_t endPos2 = sourcefile.find(enddesPattern, startPos2);
      if (endPos2 != std::string::npos) {
          totalmods = sourcefile.substr(
              startPos2 + startdesPattern.length(),
              endPos2 - (startPos2 + startdesPattern.length())
          );
      }
  }
  title = workshoptitle(sourcefile);

  // runs steamcmd
  try {
     std::cout << "=======================================================================\n\n";

     if (!inputCmd->collectionid.empty()) {
        std::cout << "Installing collection: " << title << "\nDestination: " << inputCmd->dir << std::endl;
        std::cout << "\nCollection Summary:\n Installing:         " << totalmods << " Mods" << std::endl;
     } else {
        std::cout << "Installing mod: " << title << "\n";
     }
     
     // y\n prompt
     std::cout << "\nIs this ok [y/N]: ";

     char input = 'n';
     if (!(std::cin >> input) || std::tolower(input) != 'y') {
        std::cout << "Operation aborted by the user.\n";
        std::exit(0);
     }
     std::cout << std::endl;

     std::thread installedmods([&inputCmd, &commands, &sourcefile, &collectionid, &totalmods]() {
     installedmodslist(inputCmd, commands, sourcefile, collectionid, totalmods);
     });

     installedmods.join();
     execAndDisplay(inputCmd, commands);

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
        std::vector<std::thread> meowthreads;
        for(size_t nya{0}; nya < inputCmd->sucids.size(); ++nya){
            meowthreads.emplace_back(Modname, inputCmd, nya);
        }
        for (std::thread& meowT : meowthreads) {
            meowT.join();
        }
    } else {
        for(size_t nya{0}; nya < inputCmd->sucids.size(); ++nya) {
            Modname(inputCmd, nya);  // Process directly instead of in thread
        }
    }
    std::cout << "\n\n" + mods + colm + " has been downloaded too: " + inputCmd->dir + "\n";
}

// runs a command with inherited stdio and waits for it; returns its exit status or -1 when it could not be spawned
int runwait(const std::vector<std::string>& args) {
    std::vector<char*> argv;
    for (const std::string& arg : args) {
        argv.push_back(const_cast<char*>(arg.c_str()));
    }
    argv.push_back(nullptr);

    posix_spawn_file_actions_t actions;
    posix_spawn_file_actions_init(&actions);

    pid_t pid;
    int spawned = posix_spawnp(&pid, args[0].c_str(), &actions, nullptr, argv.data(), environ);
    posix_spawn_file_actions_destroy(&actions);
    if (spawned != 0) {
        return -1;
    }

    int status;
    waitpid(pid, &status, 0);
    return status;
}

// a mod recorded in a .stc-manifest
struct trackedmod {
    std::string gameid;
    std::string modid;
    std::string folder;
};

// -u: refreshes steamcmd, re-downloads the mods tracked in the destination's .stc-manifest into their existing folders and updates a stc git checkout
void updatecommand(cmd *inputCmd) {
    struct stat sb;
    if (stat(inputCmd->dir.c_str(), &sb) != 0 || !S_ISDIR(sb.st_mode)) {
        inputCmd->dir = std::string(inputCmd->userHome) + "/.local/share/stc/mods";
    }

    std::cout << "Checking for a steamcmd update..." << std::endl;
    steamproc proc = spawnSteamcmd(inputCmd, "+quit\n");
    char temp[128];
    while (proc.out != nullptr && fgets(temp, sizeof(temp), proc.out) != nullptr) {
        std::cout << temp;
    }
    proc.close();

    std::ifstream manifest{std::filesystem::path(inputCmd->dir) / ".stc-manifest"};
    if (!manifest) {
        std::cout << "Nothing is tracked in " << inputCmd->dir << ", run stc -c or stc -m first." << std::endl;
    } else {
        std::vector<trackedmod> tracked;
        std::string line;
        while (std::getline(manifest, line)) {
            std::istringstream entry(line);
            trackedmod mod;
            if (!(entry >> mod.gameid >> mod.modid >> mod.folder)) {
                continue;
            }
            bool known = false;
            for (const trackedmod& old : tracked) {
                if (old.gameid == mod.gameid && old.modid == mod.modid) {
                    known = true;
                    break;
                }
            }
            if (!known) {
                tracked.push_back(mod);
            }
        }

        if (tracked.empty()) {
            std::cout << "The manifest in " << inputCmd->dir << " has no mods in it." << std::endl;
        } else {
            std::cout << "Updating " << tracked.size() << " mods in " << inputCmd->dir << "..." << std::endl;

            std::string commands;
            for (const trackedmod& mod : tracked) {
                commands += "+workshop_download_item " + mod.gameid + " " + mod.modid + "\n";
            }
            commands += "+quit\n";

            execAndDisplay(inputCmd, commands);

            for (const trackedmod& mod : tracked) {
                inputCmd->gameid = mod.gameid;
                filerestort(inputCmd, mod.modid, mod.folder);
            }
        }
    }

    // stc itself: a git checkout can pull and rebuild, a packaged install belongs to the package manager
    std::filesystem::path repo;
    std::filesystem::path here = std::filesystem::read_symlink("/proc/self/exe").parent_path();
    for (std::filesystem::path walk = here; !walk.empty(); walk = walk.parent_path()) {
        if (std::filesystem::exists(walk / ".git")) {
            repo = walk;
            break;
        }
    }

    if (!repo.empty()) {
        std::cout << "\nstc itself: git checkout at " << repo << ", pulling and rebuilding..." << std::endl;
        if (runwait({"git", "-C", repo.string(), "pull", "--ff-only"}) == 0) {
            runwait({"make", "-C", repo.string(), "all"});
            std::cout << "Rebuilt. Run sudo make install to update the installed binary." << std::endl;
        } else {
            std::cerr << "git pull failed; resolve it by hand and rebuild with make all." << std::endl;
        }
    } else {
        std::cout << "\nstc itself was installed from a package; update it the way you installed it (pacman -Syu, the deb or rpm, or a fresh make all from source)." << std::endl;
    }
}
