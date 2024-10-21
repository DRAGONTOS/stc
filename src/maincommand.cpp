#include <iostream>
#include <stdexcept>
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
        }

        // checks for timed out ones.
        if (line.find("Timeout") != std::string::npos) {
            inputCmd->timedout++;
        }

        // checks for errors.
        if (line.find("ERROR!") != std::string::npos) {
            inputCmd->errors++;
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
  std::string idsm = (inputCmd->ab == 1) ? R"( +workshop_download_item )" + inputCmd->gameid + " " + inputCmd->modid + " +quit": R"()";

  std::string maincommand2 = std::string{"sh ~/Steam/steamcmd.sh +force_install_dir "
                           + inputCmd->dir 
                           + " +login " 
                           + inputCmd->user 
                           + inputCmd->pass 
                           + inputCmd->slash
                           + idsm
                           + inputCmd->ids}.c_str();

  try {
      std::atomic<bool> running(true); 
      std::thread cursorThread(showLoadingCursor, std::ref(running)); 
      execAndDisplay(inputCmd, maincommand2, running); 

      running = false; 
      cursorThread.join(); 
  } catch (const std::runtime_error& e) {
      std::cerr << "\nError: " << e.what() << std::endl; 
  }

  // mod or collection?
  std::string total = (!inputCmd->modid.empty()) ? "Total: 1" : "Total: " + std::to_string(inputCmd->slashtp -1) + "\n";
  std::string colm = (inputCmd->ab == 1) ? R"(Mod)": R"(Collection)"; 

  // shows how much and what has downloaded
  std::string mods = total  + "\n"
                            + "Finished: "  + std::to_string(inputCmd->successes +1)  + "\n"
                            + "Timed out: " + std::to_string(inputCmd->timedout)      + "\n"
                            + "Errored: "   + std::to_string(inputCmd->errors)        + "\n" + "\n";

  std::cout << "\n\n" + mods + colm + " has been downloaded too: " + inputCmd->dir + "\n";
}
