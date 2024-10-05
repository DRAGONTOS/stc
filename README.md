# RapidMenu
Utilize configuration files to create runner scripts.

# Requirements
- cpptoml
- tomlplusplus
- a runner like rofi, dmenu etc

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
usage:  RapidMenu [flags] [<command> [args]]
LISTING COMMANDS:
    -c:           To specify which config to use.
    -b:           Make a executable out of a config.
```

### You will need to first setup the config dir
To do that you just need to, use this command.
```
RapidMenu
```
And it should say "Created /home/$USER/.config/RapidMenu".

### How to use (-c) 
If you want to open a configuration, just use this command.
```
RapidMenu -c [config]
```
You should put the config files in the config dir. So, if you want
to make a dashboard, then call it dashboard.conf. For games, use games.conf etc.

### How to use (-b)
Just use this command, it will throw an error if incorrect.
Also, it will ask what you want to name the executable, and if it already exists.
```
RapidMenu -b [config]
```

## Configuration
It is required to have the [runner] section in the configuration.
Example:
```
[runner]
rname = "Dashboard:" #name if needed
rtheme = "-show-icons -theme ~/.config/rofi/themes/rounded-purple-dark.rasi" #theme if needed
rcommand = "rofi -dmenu -p" #dmenu, rofi, bemenu etc
```
An example of something to add to the configuration.
```
[Disk] #To set the name
names = "Disk" #To set the name in the runner
description = "You chose Disk." #Not required but can be set
command = "kitty -e /home/$USER/.config/RapidMenu/hdd.bash" #bash command
```

# Contributors
- DRAGONTOS
- maukkis
