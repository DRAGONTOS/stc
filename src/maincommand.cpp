#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <chrono>
#include <atomic>
#include "includes/Regex.hpp"

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
            inputCmd->sucids += line + "\n";
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

        inputCmd->totalmeow++;
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

void maincommand(cmd *inputCmd) {
  std::string idsm = R"( +workshop_download_item )" + inputCmd->gameid + " " + inputCmd->modid + " +quit";

  std::filesystem::remove_all(std::string(inputCmd->userHome) + "/.cache/steamapps");

  std::string maincommand2 = std::string{"sh ~/Steam/steamcmd.sh +force_install_dir "
     + std::string(inputCmd->userHome) + "/.cache"
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

    // mod names
    std::istringstream inputStream(inputCmd->sucids);
    std::string line;

    while (std::getline(inputStream, line)) {
        inputCmd->totalmods++; 
        Modname(inputCmd, line);
        filerestort(inputCmd); 
    }

  std::cout << "\n\n" + mods + colm + " has been downloaded too: " + inputCmd->dir + "\n";
}
