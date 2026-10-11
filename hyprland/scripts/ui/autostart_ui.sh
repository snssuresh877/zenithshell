#!/usr/bin/env bash
# ==============================================================================
# 🌌 ZenithShell / Desktop UI Autostart Launcher
# ==============================================================================

set -euo pipefail

STATE_FILE="$HOME/.config/hypr/ui_mode"
MODE="zenithshell"

if [ -f "$STATE_FILE" ]; then
    MODE=$(cat "$STATE_FILE" | tr -d '[:space:]')
fi

if [ "$MODE" = "waybar" ]; then
    pgrep -f swaync >/dev/null 2>&1 || swaync >/dev/null 2>&1 &
    pgrep -f waybar >/dev/null 2>&1 || waybar >/dev/null 2>&1 &
else
    if ! pgrep -f zenithshell >/dev/null 2>&1; then
        if [ -x "$HOME/.local/bin/zenithshell" ]; then
            setsid "$HOME/.local/bin/zenithshell" >/tmp/zenithshell.log 2>&1 &
        elif [ -x "$HOME/Projects/zenithshell/build/zenithshell" ]; then
            setsid "$HOME/Projects/zenithshell/build/zenithshell" >/tmp/zenithshell.log 2>&1 &
        fi
    fi
fi
