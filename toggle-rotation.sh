#!/bin/bash
TOGGLE_FILE="$HOME/.config/hypr/rotation-toggle"

# Toggle between 0 (off) and 1 (on)
if [ "$(cat "$TOGGLE_FILE")" -eq 1 ]; then
    echo "0" > "$TOGGLE_FILE"
    notify-send "Auto-rotation disabled"
    #pkill -f iio-hyprland
else
    echo "1" > "$TOGGLE_FILE"
    notify-send "Auto-rotation enabled"
    #iio-hyprland eDP-1 &>/dev/null &
fi
