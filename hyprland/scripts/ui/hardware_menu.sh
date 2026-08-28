#!/bin/bash
pgrep -x rofi >/dev/null && exit 0

choice=$(printf \
"󰓃 Reload Audio (PipeWire / WirePlumber)
󰂯 Reload Bluetooth Stack (BlueZ)
󰤨 Reload Wi-Fi Driver & NetworkManager
󰟸 Reload Trackpad / Input Drivers
󰍹 Reload Monitors & Hyprland Config
⚡ Run Full Hardware Diagnostics" | rofi -dmenu -i -p "Hardware Controls")

[ -z "$choice" ] && exit 0

case "$choice" in
  *Audio*)
    systemctl --user restart pipewire wireplumber pipewire-pulse 2>/dev/null || true
    notify-send "Hardware" "Audio stack (PipeWire) reloaded"
    ;;
  *Bluetooth*)
    sudo systemctl restart bluetooth 2>/dev/null || true
    notify-send "Hardware" "Bluetooth service reloaded"
    ;;
  *Wi-Fi*)
    sudo systemctl restart NetworkManager 2>/dev/null || true
    notify-send "Hardware" "NetworkManager reloaded"
    ;;
  *Trackpad*)
    sudo modprobe -r psmouse 2>/dev/null && sudo modprobe psmouse 2>/dev/null || true
    notify-send "Hardware" "Trackpad driver reset"
    ;;
  *Monitors*)
    hyprctl reload
    notify-send "Hyprland" "Monitors and configuration reloaded"
    ;;
  *Full\ Hardware*)
    if command -v inxi >/dev/null 2>&1; then
      foot -T "Hardware Diagnostics" -e inxi -Fazy
    elif command -v lshw >/dev/null 2>&1; then
      foot -T "Hardware Diagnostics" -e sudo lshw -short
    fi
    ;;
esac
