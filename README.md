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

## Help message

```
usage:  Steam Collector [flags] [<command> [args]]
LISTING COMMANDS:
    -c:           Install a collection.
    -m:           Install a specific mod.
```

# Contributors

- DRAGONTOS
- maukkis
