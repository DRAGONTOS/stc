# Steam Collector

Easily download collections and mods from steam.

# NOTICE

Debian might not work!!

# Requirements

- curl
- cmake
- libarchive
- ninja
- pkg-config

# Installing

Grab a build from [the releases](https://github.com/DRAGONTOS/stc/releases) —
`latest` carries a fresh build of master, tags carry pinned versions, each with
the binary, a deb and an rpm. Or compile it yourself:

```
make all
sudo make install
```

Using rpm:

```
make all
cd build
cpack -G RPM
sudo rpm -i *.rpm
```

Using deb:

```
make all
cd build
cpack -G deb
sudo dpkg -i *.deb
```

Using PKGBUILD:
(Dont forget to enable multilib repo)

```
sudo pacman -S lib32-glibc lib32-gcc-libs
makepkg -si PKGBUILD
```

# Usage

```
usage:  Steam Collector [flags] [<command> [args]]
LISTING FLAGS:
    -c:           Install a collection.
    -m:           Install a specific mod.
    -h:           For the entire help msg.
    -u:           Update steamcmd and the mods in a folder.
    -z:           Pack the installed mods of a folder into a zip.
    -i:           Import a zip of mods and register them for updates.
```

Install a collection or a single mod:

```
stc -c <collectionid> <gameid> [dir]
stc -m <modid> <gameid> [dir]
```

Login is only needed for games that gate their workshop, pass a username and a
password between the id and the gameid:

```
stc -c <collectionid> <user> <pass> <gameid> [dir]
```

The gameid and the ids must be numbers, the dir defaults to the mods folder.

# Updating

```
stc -u [dir]
```

Without a dir this only checks steamcmd for updates. With a dir it also
re-downloads every mod recorded in that folder's `.stc-manifest` into the same
named folder it was installed to, and pulls and rebuilds a git checkout of stc
itself. Packaged installs get pointed at their package manager instead. Updates
run anonymously, so mods behind a login are reported as failed, rerun your
install command with your credentials for those.

Every successful install appends a line with the gameid, the mod id and the
folder name to `.stc-manifest` inside the destination folder, that is the
registry `-u` reads.

# Zips

```
stc -z [dir] [zipfile]
stc -i <zipfile> [dir]
```

`-z` packs the mods a folder tracks, its `.stc-manifest` and an `install.sh`
script into a zip, which the website can hand out and take back. The script
installs the extracted mods into a dir and registers them, so a zip is a full
backup. `-i` imports such a zip: it restores the mod folders into the dir and
merges the registry entries, so `stc -u` keeps updating those mods afterwards.
Importing the same zip twice does not duplicate anything.

# Website

```
stc -w [port]
```

```
opened website on localhost:8080
```

Serves a local site on `127.0.0.1` only, default port 8080 (it walks up to the
next free one and says which it took, Ctrl-C stops it). Material You styling
with purple accents, dark scheme aware. The page can install collections and
mods, update a folder, download a folder as a zip and upload a zip to import
it. One job runs at a time; steam credentials stay on this machine and are only
passed to steamcmd.

# Where things live

- `~/.local/share/stc/Steam`: the steamcmd install, created on first use.
- `~/.local/share/stc/mods`: the default download folder.
- `~/.cache`: the steamcmd download cache, cleared on every run.
