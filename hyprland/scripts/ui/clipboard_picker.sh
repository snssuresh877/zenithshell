#!/usr/bin/env bash

selection=$(cliphist list | rofi -dmenu -config ~/.config/rofi/config-cliphist.rasi)
[ -z "$selection" ] && exit

echo "$selection" | cliphist decode | wl-copy
