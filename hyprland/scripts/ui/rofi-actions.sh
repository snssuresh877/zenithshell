#!/bin/bash

# Prevent multiple rofi instances
pgrep -x rofi >/dev/null && exit

# Ensure Wayland/Hyprland environment
export XDG_CURRENT_DESKTOP=Hyprland
export XDG_SESSION_TYPE=wayland

choice=$(printf \
  "󰣆 Apps
󰖯 Windows
📋 Clipboard Picker
🧹 Clipboard Clear
🧹 System Clean
⚙ System Settings
⏻ Power Menu
🖼 Wallpaper Switcher" |
  rofi -dmenu -i -p "Search")

[ -z "$choice" ] && exit

case "$choice" in
*Apps*) rofi -show drun ;;
*Windows*) rofi -show window ;;
*Clipboard\ Picker*) ~/.config/hypr/scripts/clipboard_picker.sh ;;
*Clipboard\ Clear*) ~/.config/hypr/scripts/clipboard_clear.sh ;;
*System\ Clean*) ~/.config/hypr/scripts/clean.sh ;;
*System\ Settings*) ~/.config/hypr/scripts/system.sh ;;
*Power\ Menu*) ~/.config/hypr/scripts/powermenu.sh ;;
*Wallpaper\ Switcher*) ~/.config/hypr/scripts/switch_wallpaper.sh ;;
esac
