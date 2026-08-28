#!/usr/bin/env bash
set -euo pipefail

STATE_FILE="$HOME/.config/hypr/ui_mode"
MODE="zenithshell"

if [ -f "$STATE_FILE" ]; then
    MODE=$(cat "$STATE_FILE" | tr -d '[:space:]')
fi

if [ "$MODE" = "zenithshell" ]; then
    gdbus call --session --dest dev.zenith.Shell --object-path /dev/zenith/Shell --method dev.zenith.Shell.ToggleControlCenter >/dev/null 2>&1 || "$HOME/.config/hypr/scripts/ui/rofi-controlpanel.sh"
else
    "$HOME/.config/hypr/scripts/ui/rofi-controlpanel.sh"
fi
