#pragma once
#include <string_view>

const std::string_view USAGE = R"#(usage:  Steam Collector [flags] [<command> [args]]
LISTING FLAGS:
    -c:           Install a collection.
    -m:           Install a specific mod.
    -h:           For the entire help msg.
    -u:           Update steamcmd and the mods in a folder.
    -z:           Pack the installed mods of a folder into a zip.
    -i:           Import a zip of mods and register them for updates.
    -w:           Open the website on localhost.
)#";

const std::string_view HELP = R"#(usage:  Steam Collector [flags] [<command> [args]]
LISTING FLAGS:
    -c:           Install a collection.
    -m:           Install a specific mod.
    -h:           For the entire help msg.
    -u:           Update steamcmd and the mods in a folder.
    -z:           Pack the installed mods of a folder into a zip.
    -i:           Import a zip of mods and register them for updates.
    -w:           Open the website on localhost.

 LISTING ARGS:
    1: Should be the collectionid/modid.
    2: Could be the username if required.
    3: Could be the password if required.
    4: The dir to where you want to download the mods.

    For -u an arg is the folder whose mods to update; without an arg only steamcmd is updated.
    For -z the args are the folder (default mods folder) and the zip file (default <folder>.zip).
    For -i the args are the zip file and the folder (default mods folder).
)#";
