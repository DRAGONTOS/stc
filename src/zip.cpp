#include <archive.h>
#include <archive_entry.h>
#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include "includes/zip.hpp"

// parses one .stc-manifest line: gameid and modid, the folder name is the rest of the line
static bool manifestline(const std::string& line, trackedmod& mod) {
    std::istringstream entry(line);
    if (!(entry >> mod.gameid >> mod.modid)) {
        return false;
    }
    std::getline(entry, mod.folder);
    if (!mod.folder.empty() && mod.folder.front() == ' ') {
        mod.folder.erase(0, 1);
    }
    while (!mod.folder.empty() && (mod.folder.back() == ' ' || mod.folder.back() == '\r')) {
        mod.folder.pop_back();
    }
    return !mod.folder.empty();
}

// the script packed into every zip, it installs the extracted mods and registers them for stc -u
static const std::string installscript = R"script(#!/bin/sh
# installs the mods packed in this zip and registers them for stc -u
target="${1:-$HOME/.local/share/stc/mods}"
mkdir -p "$target"
cd "$(dirname "$0")" || exit 1
while read -r gameid modid folder; do
    [ -n "$folder" ] || continue
    if [ -d "$folder" ]; then
        rm -rf "$target/$folder"
        cp -r "$folder" "$target/$folder"
    fi
    printf '%s %s %s\n' "$gameid" "$modid" "$folder" >> "$target/.stc-manifest"
done < .stc-manifest
echo "Installed into $target, run: stc -u $target"
)script";

// writes one file entry into the zip
static void writeentry(struct archive* zip, const std::string& zippath, const std::string& content) {
    struct archive_entry* entry = archive_entry_new();
    archive_entry_set_pathname(entry, zippath.c_str());
    archive_entry_set_size(entry, content.size());
    archive_entry_set_filetype(entry, AE_IFREG);
    archive_entry_set_perm(entry, 0644);
    if (archive_write_header(zip, entry) == ARCHIVE_OK) {
        archive_write_data(zip, content.data(), content.size());
    }
    archive_entry_free(entry);
}

// streams one file from disk into the zip
static void writefileentry(struct archive* zip, const std::string& zippath, const std::filesystem::path& file) {
    std::error_code sizeerror;
    unsigned long long size = std::filesystem::file_size(file, sizeerror);
    if (sizeerror) {
        return;
    }

    struct archive_entry* entry = archive_entry_new();
    archive_entry_set_pathname(entry, zippath.c_str());
    archive_entry_set_size(entry, size);
    archive_entry_set_filetype(entry, AE_IFREG);
    archive_entry_set_perm(entry, 0644);
    if (archive_write_header(zip, entry) != ARCHIVE_OK) {
        archive_entry_free(entry);
        return;
    }

    std::ifstream in{file, std::ios::binary};
    char buffer[65536];
    while (in) {
        in.read(buffer, sizeof(buffer));
        if (in.gcount() > 0) {
            archive_write_data(zip, buffer, in.gcount());
        }
    }
    archive_entry_free(entry);
}

std::string exportzip(const std::string& dir, const std::string& zipfile, const std::vector<std::string>* only) {
    std::filesystem::path manifestpath = std::filesystem::path(dir) / ".stc-manifest";
    std::ifstream manifest{manifestpath};
    if (!manifest) {
        std::cerr << "Nothing is tracked in " << dir << ", run stc -c or stc -m first." << std::endl;
        return "";
    }
    std::stringstream manifestcontent;
    manifestcontent << manifest.rdbuf();

    struct archive* zip = archive_write_new();
    archive_write_set_format_zip(zip);
    if (archive_write_open_filename(zip, zipfile.c_str()) != ARCHIVE_OK) {
        std::cerr << "Could not write " << zipfile << std::endl;
        archive_write_free(zip);
        return "";
    }

    // the filtered manifest, so a zip registers exactly what it carries
    std::ostringstream filtered;
    int packed = 0;
    std::vector<std::filesystem::path> folders;
    std::string line;
    while (std::getline(manifestcontent, line)) {
        trackedmod mod;
        if (!manifestline(line, mod)) {
            continue;
        }
        if (only != nullptr && std::find(only->begin(), only->end(), mod.folder) == only->end()) {
            continue;
        }
        std::filesystem::path folderpath = std::filesystem::path(dir) / mod.folder;
        if (!std::filesystem::exists(folderpath)) {
            std::cerr << "Skipping " << mod.folder << ", it is not in " << dir << " anymore." << std::endl;
            continue;
        }
        filtered << mod.gameid << " " << mod.modid << " " << mod.folder << "\n";
        folders.push_back(folderpath);
        packed++;
    }

    writeentry(zip, ".stc-manifest", filtered.str());
    writeentry(zip, "install.sh", installscript);

    for (const std::filesystem::path& folderpath : folders) {
        for (std::filesystem::recursive_directory_iterator it(folderpath), end; it != end; ++it) {
            if (it->is_regular_file()) {
                std::string relpath = it->path().lexically_relative(dir).string();
                writefileentry(zip, relpath, it->path());
            }
        }
    }

    archive_write_close(zip);
    archive_write_free(zip);
    return "Packed " + std::to_string(packed) + " mods into " + zipfile;
}

std::string importzip(const std::string& zipfile, const std::string& targetdir) {
    struct archive* in = archive_read_new();
    archive_read_support_format_zip(in);
    archive_read_support_format_tar(in);
    if (archive_read_open_filename(in, zipfile.c_str(), 10240) != ARCHIVE_OK) {
        std::cerr << "Could not read " << zipfile << std::endl;
        archive_read_free(in);
        return "";
    }

    std::filesystem::create_directories(targetdir);
    std::filesystem::path targetmanifest = std::filesystem::path(targetdir) / ".stc-manifest";

    // the ids the target already tracks, so imports do not add duplicates
    std::vector<std::pair<std::string, std::string>> known;
    std::ifstream existing{targetmanifest};
    std::string line;
    trackedmod mod;
    while (std::getline(existing, line)) {
        if (manifestline(line, mod)) {
            known.push_back({mod.gameid, mod.modid});
        }
    }
    existing.close();

    std::string zipmanifest;
    int imported = 0;
    int registered = 0;

    struct archive* ext = archive_write_disk_new();
    archive_write_disk_set_options(ext, ARCHIVE_EXTRACT_TIME | ARCHIVE_EXTRACT_PERM);

    struct archive_entry* entry;
    while (archive_read_next_header(in, &entry) == ARCHIVE_OK) {
        std::string path = archive_entry_pathname(entry);

        // a zip from somewhere else is not trusted with paths outside the target
        if (path.find("..") != std::string::npos || path.find('\0') != std::string::npos) {
            archive_read_data_skip(in);
            continue;
        }

        if (path == ".stc-manifest") {
            const void* buffer;
            size_t size;
            la_int64_t offset;
            while (archive_read_data_block(in, &buffer, &size, &offset) == ARCHIVE_OK) {
                zipmanifest.append(static_cast<const char*>(buffer), size);
            }
            continue;
        }
        if (path == "install.sh") {
            archive_read_data_skip(in);
            continue;
        }

        std::string full = targetdir + "/" + path;
        archive_entry_set_pathname(entry, full.c_str());
        archive_write_header(ext, entry);
        const void* buffer;
        size_t size;
        la_int64_t offset;
        while (archive_read_data_block(in, &buffer, &size, &offset) == ARCHIVE_OK) {
            archive_write_data_block(ext, buffer, size, offset);
        }
        archive_write_finish_entry(ext);
        imported++;
    }

    archive_read_free(in);
    archive_write_free(ext);

    std::istringstream entries(zipmanifest);
    std::ofstream out{targetmanifest, std::ios::app};
    while (std::getline(entries, line)) {
        if (!manifestline(line, mod)) {
            continue;
        }
        bool knownpair = false;
        for (const auto& old : known) {
            if (old.first == mod.gameid && old.second == mod.modid) {
                knownpair = true;
                break;
            }
        }
        if (knownpair) {
            continue;
        }
        known.push_back({mod.gameid, mod.modid});
        out << mod.gameid << " " << mod.modid << " " << mod.folder << "\n";
        registered++;
    }

    std::string summary = "Imported " + std::to_string(imported) + " files from " + zipfile + " into " + targetdir
              + ", " + std::to_string(registered) + " mods registered for updates.";
    if (registered > 0) {
        summary += "\nRun stc -u " + targetdir + " to update them.";
    }
    return summary;
}
