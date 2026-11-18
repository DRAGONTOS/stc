#pragma once
#include <string_view>

const std::string_view USAGE = R"#(usage:  Steam Collector [flags] [<command> [args]]
LISTING FLAGS:
    -c:           Install a collection.
    -m:           Install a specific mod.
    -h:           For the entire help msg.
    -u:           Update steamcmd and the mods in a folder.
)#";

const std::string_view HELP = R"#(usage:  Steam Collector [flags] [<command> [args]]
LISTING FLAGS:
    -c:           Install a collection.
    -m:           Install a specific mod.
    -h:           For the entire help msg.
    -u:           Update steamcmd and the mods in a folder.

 LISTING ARGS:
    1: Should be the collectionid/modid.
    2: Could be the username if required.
    3: Could be the password if required.
    4: The dir to where you want to download the mods.

    For -u an arg is the folder whose mods to update; without an arg only steamcmd is updated.
)#";
