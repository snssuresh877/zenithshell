#!/usr/bin/env bash
# ==============================================================================
# 📂 Dynamic High-End File Manager Launcher
# ==============================================================================
# - Launches GUI file manager (cosmic-files / dolphin / nautilus) or Yazi TUI
# - Opens in the focused window's directory if available
# ==============================================================================

set -euo pipefail

MODE="${1:-gui}"

# Resolve target directory from focused window
TARGET_DIR="$HOME"
if command -v hyprctl >/dev/null 2>&1 && command -v jq >/dev/null 2>&1; then
    ACTIVE_PID=$(hyprctl activewindow -j 2>/dev/null | jq -r '.pid // empty')
    if [ -n "$ACTIVE_PID" ] && [ "$ACTIVE_PID" != "null" ] && [ -d "/proc/$ACTIVE_PID" ]; then
        CHILD_PID=$(pgrep -P "$ACTIVE_PID" 2>/dev/null | tail -n 1 || true)
        if [ -n "$CHILD_PID" ] && [ -d "/proc/$CHILD_PID/cwd" ]; then
            RESOLVED_DIR=$(readlink -f "/proc/$CHILD_PID/cwd" 2>/dev/null || true)
        else
            RESOLVED_DIR=$(readlink -f "/proc/$ACTIVE_PID/cwd" 2>/dev/null || true)
        fi
        if [ -n "$RESOLVED_DIR" ] && [ -d "$RESOLVED_DIR" ]; then
            TARGET_DIR="$RESOLVED_DIR"
        fi
    fi
fi

if [ "$MODE" = "tui" ] || [ "$MODE" = "yazi" ]; then
    if command -v yazi >/dev/null 2>&1; then
        if command -v foot >/dev/null 2>&1; then
            exec foot -D "$TARGET_DIR" -e yazi "$TARGET_DIR"
        elif command -v kitty >/dev/null 2>&1; then
            exec kitty --directory "$TARGET_DIR" -e yazi "$TARGET_DIR"
        else
            cd "$TARGET_DIR" && exec yazi
        fi
    fi
fi

# GUI Mode
if command -v cosmic-files >/dev/null 2>&1; then
    exec cosmic-files "$TARGET_DIR"
elif command -v dolphin >/dev/null 2>&1; then
    exec dolphin "$TARGET_DIR"
elif command -v nautilus >/dev/null 2>&1; then
    exec nautilus "$TARGET_DIR"
elif command -v thunar >/dev/null 2>&1; then
    exec thunar "$TARGET_DIR"
elif command -v yazi >/dev/null 2>&1; then
    exec foot -D "$TARGET_DIR" -e yazi "$TARGET_DIR"
else
    xdg-open "$TARGET_DIR"
fi
