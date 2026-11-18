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

From source:

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

# Where things live

- `~/.local/share/stc/Steam`: the steamcmd install, created on first use.
- `~/.local/share/stc/mods`: the default download folder.
- `~/.cache`: the steamcmd download cache, cleared on every run.
