#include <string>
#include <sstream> 
#include "regex"
#include "includes/Regex.hpp"

// regex and stuff (collectionid)
void Regex(cmd *inputCmd) {
    if (!inputCmd->collectionid.empty()) {
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
    }

    std::istringstream inputStream(inputCmd->ids);
    std::string line;

    // Count the number of lines
    while (std::getline(inputStream, line)) {
        inputCmd->slashtp++; 
    }
}
