#!/usr/bin/env bash

set -e

WALLPAPER_DIR="$HOME/Pictures/wallpapers"

# Ensure wallpaper daemon exists
pgrep -x awww-daemon >/dev/null || awww-daemon &

sleep 0.5

# Select random wallpaper
FULL_PATH=$(find "$WALLPAPER_DIR" \
  -type f \( -iname "*.jpg" -o -iname "*.jpeg" -o -iname "*.png" -o -iname "*.webp" \) |
  shuf -n 1)

# Exit if nothing found
[ -z "$FULL_PATH" ] && {
  notify-send "Wallpaper switch failed" "No wallpapers found"
  exit 1
}

# Set wallpaper
awww img "$FULL_PATH" \
  --transition-type grow \
  --transition-duration 0.4 \
  --transition-fps 30

# Ensure pywal exists
command -v wal >/dev/null || {
  notify-send "Pywal not installed"
  exit 1
}

# Generate pywal colors
wal -n -q -i "$FULL_PATH"

# Reload Waybar
pkill waybar
waybar >/dev/null 2>&1 &

# Optional notification
notify-send "Wallpaper changed" "$(basename "$FULL_PATH")"
