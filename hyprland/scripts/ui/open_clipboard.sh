#!/usr/bin/env bash
set -euo pipefail

STATE_FILE="$HOME/.config/hypr/ui_mode"
MODE="zenithshell"

if [ -f "$STATE_FILE" ]; then
    MODE=$(cat "$STATE_FILE" | tr -d '[:space:]')
fi

if [ "$MODE" = "zenithshell" ]; then
    zenithctl clipboard >/dev/null 2>&1 || (cliphist list | rofi -dmenu -p "Clipboard" | cliphist decode | wl-copy 2>/dev/null)
else
    cliphist list | rofi -dmenu -p "Clipboard" | cliphist decode | wl-copy 2>/dev/null
fi
