# Agent guide

Steam Collector (`stc`) is a small CLI tool that downloads Steam Workshop collections and single mods. It fetches the workshop page over curl, scrapes the mod ids out of the HTML, builds a steamcmd script and runs steamcmd, which it auto-installs into `~/.local/share/stc` on first use. Written in C++26 on CMake and Ninja, links curl and libarchive through pkg-config. Packaged as a deb, an rpm and an Arch `PKGBUILD`. The old contributor names (DRAGONTOS, maukkis) still show in the README and commit history.

Cross-session handoff notes for this project live in `~/.local/share/agents/stc-handoff.md`. Read that file first when resuming after a context reset, and update it when state changes (commits, queue changes, new gotchas).

## Architecture

| Part | Where | Notes |
|---|---|---|
| Entry and arg parsing | `src/main.cpp` | Checks the network with `gethostbyname`, then dispatches the flags: `-c` collection, `-m` mod, `-h` help, `-u` update. Fills the `cmd` struct and calls `Regex` then `maincommand`, or `updatecommand` for `-u`. |
| HTTP | `src/getHttp.cpp` | `getHttp` pulls a workshop page into a string on `inputCmd->source`; `downloadFile` streams a file to disk. Plain curl easy handles, no error category system — errors `throw`. |
| Scrape | `src/Regex.cpp` | Walks the workshop page line by line with `find`/`substr`, collects `+workshop_download_item <gameid> <id> \` lines into `inputCmd->ids` and counts `totalmods`. `workshoptitle` and `jsontitle` are the shared scrapers every caller uses; `filerestort` moves a finished download into its named folder and appends it to the destination's `.stc-manifest`. Despite the name, no `std::regex` is involved. |
| Steamcmd driver | `src/maincommand.cpp` | Holds the `cmd` struct, `setup` (steamcmd install + mods dir), `spawnSteamcmd` (spawns steamcmd with an argument array and pipes — no shell string; the login password and download commands go over stdin, so they never show in `ps`), `execAndDisplay` (counts successes and timeouts), `installedmodslist` (matches downloaded ids against the page JSON for titles) and `maincommand` (copies results to the target dir). |
| UI strings | `src/includes/Strings.hpp` | `USAGE` and `HELP` raw string literals. Keep them in sync with the flags in `main.cpp`. |
| Version | `props.json` | Single `"version"` key, read by CMake at configure time. Bump it for every release, and update the `line-count` file in the same commit. |

## Build and install

```sh
make all
sudo make install
```

`make all` clears `build/` and configures with Ninja. The `release` target currently passes `-DCMAKE_BUILD_TYPE=Debug`; see the gotchas before trusting build flags. Packaging:

```sh
cd build && cpack -G DEB   # or RPM
makepkg -si PKGBUILD       # Arch, needs multilib
```

| Command | Use |
|---|---|
| `make all` | Fresh cmake configure + build into `build/stc`. |
| `./build/stc -c <collectionid> <gameid> [dir]` | Download a collection. Login args (`user pass` before `gameid`) are optional. |
| `./build/stc -m <modid> <gameid> [dir]` | Download a single mod. Same arg shapes as `-c`. |
| `./build/stc -u [dir]` | Refresh steamcmd, re-download the mods tracked in `<dir>/.stc-manifest` into their recorded folders, and pull+rebuild a git checkout. Without a dir it updates the default mods folder. |

Every install appends `<gameid> <modid> <foldername>` lines to `.stc-manifest` inside the destination folder (see `recordinstall` in `src/Regex.cpp`); `-u` reads that registry to update mods in place. Passwords are never stored, so `-u` updates anonymously and reports what needs credentials.

## Hard rules

- Never push (git or a release) without the owner's explicit approval.
- Never run `sudo` or `make install` on your own; building and running the binary from `build/` is fine.
- GPLv3, keep the LICENSE.
- Commit subjects are lowercase with a type prefix and no body: `fix:`, `added:`, `struct:`, `Bump: bumped to (x.y.z)`.
- The version in `props.json` is the single source of truth; bump it and `line-count` together.
- Don't add a comment where the code can say it itself; the codebase does comment, but sparingly — follow the surrounding file.
- Steam credentials are only ever passed by the user. Don't log them and don't store them anywhere.

## Before pushing

- `make all` builds clean.
- A manual run of `-c` and `-m` against a real workshop id succeeds, including the no-login path.
- `USAGE` and `HELP` in `src/includes/Strings.hpp` still match the flags actually implemented.
- `props.json`, `line-count` and any packaging metadata agree on the version.

## Code style

- C++26, target `stc`, sources globbed from `src/` recursively — a new file needs no CMake edit.
- Indentation is a mix of 2 and 4 spaces across files; match the file you are editing rather than reformatting.
- curl error handling uses `throw`; `main.cpp` catches `std::string&` from `getHttp`. Keep new errors consistent with their file.
- No test suite exists. Verification is manual, via the commands above.

## Gotchas

- `Regex` scrapes live HTML from steamcommunity.com; layout changes on Valve's side break it silently (zero mods found, not an error). Debug scraping failures by dumping `inputCmd->source` first.
- `installedmodslist` matches ids against `"id":"...","title":"..."` JSON in the page source; a missing title means the id was never on the page, usually a scrape miss upstream.
- The steamcmd install lives in `~/.local/share/stc` and the cache in `~/.cache/steamapps` is wiped on every run — never point these at a directory the user owns for something else.
- Network check uses `gethostbyname("google.com")`; it is blocking and DNS-only, so a captive portal passes it and curl still fails later.
- steamcmd's first boot runs a self-update against Valve's CDN and that check can hang for many minutes with no output before it retries; it looks like a hang in stc but is not.
