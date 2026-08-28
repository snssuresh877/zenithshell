#!/bin/bash
pgrep -x rofi >/dev/null && exit 0

choice=$(printf \
"󰔎 Toggle Night Light (Hyprsunset)
󰂛 Toggle Do Not Disturb (DND)
󰖔 Toggle Dark / Light Theme
󱂬 Cycle UI Engine (ZenithShell ↔ Waybar)
󰤨 Toggle Wi-Fi Radio (On/Off)
󰂯 Toggle Bluetooth (On/Off)
󰍹 Toggle Floating Window Mode" | rofi -dmenu -i -p "System Toggles")

[ -z "$choice" ] && exit 0

case "$choice" in
  *Night\ Light*) ~/.config/hypr/scripts/ui/toggle_nightlight.sh ;;
  *Do\ Not\ Disturb*) ~/.config/hypr/scripts/ui/toggle_dnd.sh ;;
  *Dark\ \/\ Light*) ~/.config/hypr/scripts/ui/toggle_darkmode.sh ;;
  *Cycle\ UI*) ~/.config/hypr/scripts/ui/toggle_ui_mode.sh ;;
  *Wi-Fi*)
    STATE=$(nmcli radio wifi)
    if [ "$STATE" = "enabled" ]; then
      nmcli radio wifi off && notify-send "Wi-Fi" "Wi-Fi disabled"
    else
      nmcli radio wifi on && notify-send "Wi-Fi" "Wi-Fi enabled"
    fi
    ;;
  *Bluetooth*)
    bluetoothctl power $(bluetoothctl show 2>/dev/null | grep -q "Powered: yes" && echo "off" || echo "on")
    ;;
  *Floating*) hyprctl dispatch togglefloating ;;
esac
