#!/usr/bin/env bash
# ==============================================================================
# 🚀 Launch Terminal in Current Focused Window Directory
# ==============================================================================
# - Detects the focused Hyprland window PID
# - Resolves the current working directory (/proc/$PID/cwd)
# - Launches foot or kitty seamlessly inside that directory
# ==============================================================================

set -euo pipefail

TERMINAL_BIN="${1:-foot}"

# Fallback target directory
TARGET_DIR="$HOME"

# Query Hyprland for the active window's PID
if command -v hyprctl >/dev/null 2>&1 && command -v jq >/dev/null 2>&1; then
    ACTIVE_PID=$(hyprctl activewindow -j 2>/dev/null | jq -r '.pid // empty')
    
    if [ -n "$ACTIVE_PID" ] && [ "$ACTIVE_PID" != "null" ] && [ -d "/proc/$ACTIVE_PID" ]; then
        # Check if the process has child shells (e.g. terminal running fish/bash/zsh)
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

# Launch terminal in the resolved directory
if command -v "$TERMINAL_BIN" >/dev/null 2>&1; then
    if [ "$TERMINAL_BIN" = "foot" ]; then
        exec foot -D "$TARGET_DIR"
    elif [ "$TERMINAL_BIN" = "kitty" ]; then
        exec kitty --directory "$TARGET_DIR"
    else
        cd "$TARGET_DIR" && exec "$TERMINAL_BIN"
    fi
else
    cd "$TARGET_DIR" && exec foot 2>/dev/null || exec kitty
fi
