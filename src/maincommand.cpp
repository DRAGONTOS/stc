#include <cstddef>
#include <cctype>
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
#include <sys/time.h>
#include <fcntl.h>
#include <poll.h>
#include <spawn.h>
#include <unistd.h>
#include <string>
#include <thread>
#include <vector>
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

// spawns steamcmd with an argument array instead of a shell string; the login password and the download commands go over stdin, so they never appear in the process list; with quietstderr its own error chatter goes to the error log instead of the terminal
steamproc spawnSteamcmd(const cmd *inputCmd, const std::string& commands, bool quietstderr = false) {
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
    // the stdin console takes commands without the '+' prefix the argv form uses
    std::istringstream lines(commands);
    for (std::string line; std::getline(lines, line); ) {
        if (!line.empty() && line[0] == '+') {
            line.erase(0, 1);
        }
        stdinlines += line + "\n";
    }

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
    if (quietstderr) {
        std::string errorlog = std::string(inputCmd->userHome) + "/.cache/errors.txt";
        posix_spawn_file_actions_addopen(&actions, STDERR_FILENO, errorlog.c_str(), O_WRONLY | O_APPEND | O_CREAT, 0644);
    }
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

// a mod recorded in a .stc-manifest
// colors for terminal output, all empty when stdout is not a terminal
struct uicolors {
    bool tty = false;
    std::string green, red, yellow, cyan, bold, reset;

    uicolors() {
        tty = isatty(STDOUT_FILENO) == 1;
        if (tty) {
            green  = "\033[32m";
            red    = "\033[31m";
            yellow = "\033[33m";
            cyan   = "\033[36m";
            bold   = "\033[1m";
            reset  = "\033[0m";
        }
    }
};

// percent shown in a steamcmd progress fragment, -1 when the fragment has none
int linepercent(const std::string& segment) {
    std::size_t pct = segment.find('%');
    if (pct != std::string::npos) {
        std::size_t begin = pct;
        while (begin > 0 && (std::isdigit(static_cast<unsigned char>(segment[begin - 1])) || segment[begin - 1] == '.')) {
            begin--;
        }
        if (begin < pct) {
            try {
                int value = std::stoi(segment.substr(begin, pct - begin));
                if (value >= 0 && value <= 100) {
                    return value;
                }
            } catch (...) {
            }
        }
    }

    std::size_t mark = segment.find("progress: ");
    if (mark != std::string::npos) {
        std::size_t begin = mark + 10;
        std::size_t end = begin;
        while (end < segment.size() && (std::isdigit(static_cast<unsigned char>(segment[end])) || segment[end] == '.')) {
            end++;
        }
        if (end > begin) {
            try {
                double value = std::stod(segment.substr(begin, end - begin));
                int whole = static_cast<int>(value);
                if (whole >= 0 && whole <= 100) {
                    return whole;
                }
            } catch (...) {
            }
        }
    }
    return -1;
}

// the number after a marker in a steamcmd line, empty when there is none
std::string numberafter(const std::string& line, const std::string& marker) {
    std::size_t at = line.find(marker);
    if (at == std::string::npos) {
        return "";
    }
    std::size_t begin = at + marker.size();
    while (begin < line.size() && !std::isdigit(static_cast<unsigned char>(line[begin]))) {
        begin++;
    }
    std::size_t end = begin;
    while (end < line.size() && std::isdigit(static_cast<unsigned char>(line[end]))) {
        end++;
    }
    return line.substr(begin, end - begin);
}

// removes the ansi color escapes steamcmd writes, for the error log
std::string stripescapes(const std::string& text) {
    std::string clean;
    std::size_t at = 0;
    while (at < text.size()) {
        if (text[at] == '\033') {
            at++;
            if (at < text.size() && text[at] == '[') {
                at++;
                while (at < text.size() && !std::isalpha(static_cast<unsigned char>(text[at]))) {
                    at++;
                }
                if (at < text.size()) {
                    at++;
                }
            }
        } else {
            clean += text[at];
            at++;
        }
    }
    return clean;
}

// shows the steamcmd output live and counts successes, timeouts and errors; on a terminal it draws a dnf-style status line with a progress bar, when piped it prints one line per finished item
void execAndDisplay(cmd *inputCmd, const std::string& commands, const std::string& sourcefile, const std::string& collectionid, const std::string& total, const std::vector<trackedmod>& knownmods = {}) {
    uicolors ui;
    bool tty = ui.tty;
    const char* cok   = ui.green.c_str();
    const char* cbad  = ui.red.c_str();
    const char* chead = ui.cyan.c_str();
    const char* cend  = ui.reset.c_str();
    int totalitems = 0;
    try {
        totalitems = std::stoi(total);
    } catch (...) {
    }
    if (totalitems < 0) {
        totalitems = 0;
    }

    steamproc proc = spawnSteamcmd(inputCmd, commands, tty);
    int totalmods = 1;
    int doneitems = 0;
    int percent = -1;
    std::string currentname;

    auto namefor = [&](const std::string& modId) {
        for (const trackedmod& known : knownmods) {
            if (known.modid == modId) {
                return known.folder;
            }
        }
        return collectionid.empty() ? workshoptitle(sourcefile) : jsontitle(sourcefile, modId);
    };

    // the status line, redrawn in place
    auto drawline = [&]() {
        if (!tty) {
            return;
        }
        std::cout << "\r\033[K";
        if (totalitems > 0) {
            int width = 30;
            int filled = doneitems * width / totalitems;
            if (filled > width) {
                filled = width;
            }
            std::cout << "[";
            for (int at = 0; at < width; ++at) {
                std::cout << (at < filled ? '#' : '-');
            }
            std::cout << "] " << doneitems << "/" << totalitems;
        }
        if (percent >= 0) {
            std::cout << "  " << percent << "%";
        }
        if (!currentname.empty()) {
            std::cout << "  " << chead << currentname << cend;
        }
        std::cout << std::flush;
    };

    // one permanent result line per finished item
    auto drawresult = [&](bool ok, const std::string& name) {
        if (!tty) {
            return;
        }
        std::cout << "\r\033[K" << (ok ? cok : cbad) << (ok ? "ok " : "err") << cend << "  " << name << std::endl;
    };

    // one terminated line of steamcmd output
    auto handleline = [&](const std::string& text) {
        // counts for the summary
        if (text.find("Timeout") != std::string::npos) {
            inputCmd->timedout++;
        }
        if (text.find("ERROR!") != std::string::npos) {
            inputCmd->errors++;
            // tmp fix for errors.
            std::ofstream meow{std::string(inputCmd->userHome) + "/.cache/errors.txt", std::ios::app};
            meow << stripescapes(text) << "\n";
        }

        // names the downloaded items
        if (text.find("Download") != std::string::npos) {
            std::string modId = numberafter(text, " item ");
            std::string idname = namefor(modId);

            if (text.find("Success") != std::string::npos) {
                inputCmd->successes++;
                inputCmd->sucids.push_back(text + "\n");

                if (!tty) {
                    if (!collectionid.empty()) {
                        std::cout << "[" << totalmods << "/" << total << "] Successfully downloaded: " << idname << std::endl;
                        totalmods++;
                    } else {
                        std::cout << "Successfully downloaded: " << idname << std::endl;
                    }
                } else {
                    drawresult(true, idname);
                }
                doneitems++;
                percent = -1;
                currentname.clear();
                drawline();
            } else if (text.find("ERROR") != std::string::npos) {
                if (!tty) {
                    if (!collectionid.empty()) {
                        std::cout << "[" << totalmods << "/" << total << "] Error downloading: " << idname << std::endl;
                        totalmods++;
                    } else {
                        std::cout << "Error downloading: " << idname << std::endl;
                    }
                } else {
                    drawresult(false, idname);
                }
                doneitems++;
                percent = -1;
                currentname.clear();
                drawline();
            } else {
                // an item started downloading
                currentname = idname.empty() ? modId : idname;
                percent = -1;
                drawline();
            }
        } else {
            int seen = linepercent(text);
            if (seen >= 0) {
                percent = seen;
                drawline();
            }
        }
    };

    // steamcmd rewrites its progress with carriage returns and no newline, so bytes are read as they arrive; a \r replaces the line being rewritten, a \n ends it
    int outfd = proc.out == nullptr ? -1 : fileno(proc.out);
    std::string rewritten;
    std::string piece;

    while (proc.out != nullptr) {
        struct pollfd watcher { outfd, POLLIN, 0 };
        if (poll(&watcher, 1, 500) <= 0) {
            continue;
        }

        char chunk[1024];
        ssize_t got = read(outfd, chunk, sizeof(chunk));
        if (got <= 0) {
            break;
        }
        std::string data(chunk, got);

        std::size_t at = 0;
        while (at < data.size()) {
            char now = data[at];
            if (now == '\r') {
                rewritten = piece;
                piece.clear();
            } else if (now == '\n') {
                handleline(rewritten + piece);
                rewritten.clear();
                piece.clear();
            } else {
                piece += now;
            }
            at++;
        }

        // the unterminated rest is steamcmd's live redraw; show its percent if it has one
        int live = linepercent(piece);
        if (live < 0) {
            live = linepercent(rewritten);
        }
        if (live >= 0) {
            percent = live;
            drawline();
        }
    }

    if (tty) {
        std::cout << "\r\033[K" << std::flush;
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

// main function
void maincommand(cmd *inputCmd) {
  //setup for steamcmd
  setup(std::string(inputCmd->userHome) + "/.local/share/stc");

  // clears cache
  std::filesystem::remove_all(std::string(inputCmd->userHome) + "/.cache/steamapps");
  std::filesystem::remove_all(std::string(inputCmd->userHome) + "/.cache/steamcmd_linux.tar.gz");

  std::string sourcefile   = inputCmd->source; // needed for some void functions

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
     uicolors ui;
     std::string rule(71, '=');
     std::cout << ui.cyan << rule << ui.reset << "\n\n";

     if (!inputCmd->collectionid.empty()) {
        std::cout << "Installing collection: " << ui.bold << title << ui.reset << "\nDestination: " << ui.cyan << inputCmd->dir << ui.reset << std::endl;
        std::cout << "\n" << ui.cyan << "Collection Summary:" << ui.reset << "\n Installing:         " << ui.green << totalmods << " Mods" << ui.reset << std::endl;
     } else {
        std::cout << "Installing mod: " << ui.bold << title << ui.reset << "\nDestination: " << ui.cyan << inputCmd->dir << ui.reset << "\n";
     }
     
     // y\n prompt
     if (getenv("STC_YES") == nullptr) {
        std::cout << "\nIs this ok [y/N]: ";

        char input = 'n';
        if (!(std::cin >> input) || std::tolower(input) != 'y') {
           std::cout << "Operation aborted by the user.\n";
           std::exit(0);
        }
     }
     std::cout << std::endl;

     execAndDisplay(inputCmd, commands, sourcefile, inputCmd->collectionid, totalmods);

     std::cout << "\n" << ui.cyan << rule << ui.reset << std::flush;
  } catch (const std::runtime_error& e) {
     std::cerr << "\nError: " << e.what() << "\n";
  }

  // mod or collection?
  std::string colm  = (inputCmd->collectionid.empty()) ? R"(Mod)"   : R"(Collection)";
  uicolors ui;

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
    std::cout << "\n\n";
    if (!inputCmd->collectionid.empty()) {
        std::cout << "Total: " << ui.green << inputCmd->totalmods << ui.reset << "\n\n";
    } else {
        std::cout << "Total: 1\n";
    }
    std::cout << "Finished: "  << ui.green  << inputCmd->successes << ui.reset << "\n";
    std::cout << "Timed out: " << ui.yellow << inputCmd->timedout  << ui.reset << "\n";
    std::cout << "Errored: "   << ui.red    << inputCmd->errors    << ui.reset << "\n\n";

    std::cout << colm << " has been downloaded too: " << ui.cyan << inputCmd->dir << ui.reset << "\n";
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

// -u: refreshes steamcmd; with a dir given it also re-downloads the mods tracked in the destination's .stc-manifest into their existing folders and updates a stc git checkout
void updatecommand(cmd *inputCmd, bool everything) {
    if (everything) {
        struct stat sb;
        if (stat(inputCmd->dir.c_str(), &sb) != 0 || !S_ISDIR(sb.st_mode)) {
            inputCmd->dir = std::string(inputCmd->userHome) + "/.local/share/stc/mods";
        }
    }

    std::cout << "Checking for a steamcmd update..." << std::endl;
    bool tty = isatty(STDOUT_FILENO) == 1;
    steamproc proc = spawnSteamcmd(inputCmd, "+quit\n", tty);
    if (proc.out != nullptr) {
        int outfd = fileno(proc.out);
        std::string rewritten;
        std::string piece;
        char chunk[1024];
        while (true) {
            struct pollfd watcher { outfd, POLLIN, 0 };
            if (poll(&watcher, 1, 500) <= 0) {
                continue;
            }
            ssize_t got = read(outfd, chunk, sizeof(chunk));
            if (got <= 0) {
                break;
            }
            std::string data(chunk, got);
            if (!tty) {
                std::cout << data << std::flush;
            }
            for (char now : data) {
                if (now == '\r') {
                    rewritten = piece;
                    piece.clear();
                } else if (now == '\n') {
                    rewritten.clear();
                    piece.clear();
                } else {
                    piece += now;
                }
            }
            int live = linepercent(piece);
            if (live < 0) {
                live = linepercent(rewritten);
            }
            if (tty) {
                if (live >= 0) {
                    std::cout << "\r\033[Ksteamcmd: " << live << "%" << std::flush;
                }
            }
        }
    }
    if (tty) {
        std::cout << "\r\033[K" << std::flush;
    }
    proc.close();

    if (!everything) {
        std::cout << "Pass a folder to also update the mods tracked in it." << std::endl;
        return;
    }

    std::ifstream manifest{std::filesystem::path(inputCmd->dir) / ".stc-manifest"};
    if (!manifest) {
        std::cout << "Nothing is tracked in " << inputCmd->dir << ", run stc -c or stc -m first." << std::endl;
    } else {
        std::vector<trackedmod> tracked;
        std::string line;
        while (std::getline(manifest, line)) {
            std::istringstream entry(line);
            trackedmod mod;
            if (!(entry >> mod.gameid >> mod.modid)) {
                continue;
            }
            // the folder name is the rest of the line, it can contain spaces
            std::getline(entry, mod.folder);
            if (!mod.folder.empty() && mod.folder.front() == ' ') {
                mod.folder.erase(0, 1);
            }
            while (!mod.folder.empty() && (mod.folder.back() == ' ' || mod.folder.back() == '\r')) {
                mod.folder.pop_back();
            }
            if (mod.folder.empty()) {
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
            uicolors ui;
            std::string rule(71, '=');
            std::cout << "\n" << ui.cyan << rule << ui.reset << "\n\n";
            std::cout << "Updating: " << ui.green << tracked.size() << " mods" << ui.reset << "\n";
            std::cout << "Destination: " << ui.cyan << inputCmd->dir << ui.reset << "\n\n";

            // a fresh download cache, same as installs use; stale workshop state made the first item of a batch fail
            std::filesystem::remove_all(std::string(inputCmd->userHome) + "/.cache/steamapps");

            std::string commands;
            for (const trackedmod& mod : tracked) {
                commands += "+workshop_download_item " + mod.gameid + " " + mod.modid + "\n";
            }
            commands += "+quit\n";

            execAndDisplay(inputCmd, commands, "", "", std::to_string(tracked.size()), tracked);

            for (const trackedmod& mod : tracked) {
                inputCmd->gameid = mod.gameid;
                filerestort(inputCmd, mod.modid, mod.folder);
            }

            std::cout << "\n" << ui.cyan << rule << ui.reset << "\n\n";
            std::cout << "Total: " << ui.green << tracked.size() << ui.reset << "\n\n";
            std::cout << "Finished: " << ui.green << inputCmd->successes << ui.reset << "\n";
            std::cout << "Timed out: " << ui.yellow << inputCmd->timedout << ui.reset << "\n";
            std::cout << "Errored: " << ui.red << inputCmd->errors << ui.reset << "\n\n";
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
