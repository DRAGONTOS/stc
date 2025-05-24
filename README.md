# Steam Collector

Easily download collections and mods from steam.

# Requirements

- curl
- cmake
- libarchive
- ninja
- steamcmd

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

```
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
