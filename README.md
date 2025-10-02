# FW12Rotate
Framework 12 rotation application specifically for Omarchy Linux, but should be usable on other distros using Hyprland that don't already have plumbing in place for rotation. This uses a toggle-rotation file in your hyprland configuration folder. (~/.config/hypr/) 

The FW12Rotate file should be moved to /usr/local/bin/ and given permissions to execute. 

This applicaiton is dependent on inotify-tools and iio-sensor-proxy (pacman -S inotify-tools iio-sensor-proxy). The point of this application is to be light weight and allow rotation to just work. 

FW12Rotate uses a a file called rotation-toggle. If this file does not exist, it will generate it and set it to enabled. I currently use a script that is run via a hyprland key combo. This script allows you to toggle rotation as either enabled or disabled. Please note, that FW12Rotate is looking for a file called rotation-toggle in ~/.config/hypr/.
This is an example of how that script could look: 
```
#!/bin/bash
TOGGLE_FILE="$HOME/.config/hypr/rotation-toggle"

# Toggle between 0 (off) and 1 (on)
if [ "$(cat "$TOGGLE_FILE")" -eq 1 ]; then
    echo "0" > "$TOGGLE_FILE"
    notify-send "Auto-rotation disabled"
else
    echo "1" > "$TOGGLE_FILE"
    notify-send "Auto-rotation enabled"
fi
```

You will need to add a an exec-once command and a key combo line to your hyprland.conf file like this: 

```
bind = SUPER, R, exec, ~/.config/hypr/toggle-rotation.sh
exec-once = /usr/local/bin/FW12Rotate
```
Restart your computer or relaunch Hyprland. This will allow you to use your key combo to enable and disable rotation. 
