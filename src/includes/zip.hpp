#ifndef ZIP_HPP
#define ZIP_HPP
#include <string>
#include "Regex.hpp"

// packs the tracked mods of a dir, its .stc-manifest and an import script into a zip; with only given, packs just those folders; returns the summary, empty when it failed
std::string exportzip(const std::string& dir, const std::string& zipfile, const std::vector<std::string>* only = nullptr);

// extracts the mods of a zip into a dir and registers them in that dir's .stc-manifest so -u updates them; returns the summary
std::string importzip(const std::string& zipfile, const std::string& targetdir);
#endif
