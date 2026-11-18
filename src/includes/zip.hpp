#ifndef ZIP_HPP
#define ZIP_HPP
#include <string>
#include "Regex.hpp"

// packs the tracked mods of a dir, its .stc-manifest and an import script into a zip
void exportzip(const std::string& dir, const std::string& zipfile);

// extracts the mods of a zip into a dir and registers them in that dir's .stc-manifest so -u updates them
void importzip(const std::string& zipfile, const std::string& targetdir);
#endif
