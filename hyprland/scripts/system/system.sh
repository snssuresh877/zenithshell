#!/usr/bin/env bash

CHOICE=$(printf \
  "󰤨 Network\n󰕾 Volume\n󰂯 Bluetooth\n󰂙 Night Light\n󰍹 Screenshot" |
  rofi -dmenu -p "System")

case "$CHOICE" in
*Network*) nm-connection-editor ;;
*Volume*) pavucontrol ;;
*Bluetooth*) blueman-manager ;;
*Night*) hyprctl dispatch exec hyprshade toggle ;;
*Screenshot*) grim -g "$(slurp)" - | wl-copy ;;
esac
