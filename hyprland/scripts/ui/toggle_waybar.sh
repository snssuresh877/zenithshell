#!/usr/bin/env bash
set -euo pipefail

# If ZenithShell is running, toggle ZenithShell topbar
if pgrep -x zenithshell >/dev/null || pgrep -f "zenithshell" >/dev/null; then
    gdbus call --session --dest dev.zenith.Shell --object-path /dev/zenith/Shell --method dev.zenith.Shell.ToggleBar >/dev/null 2>&1 || true
    exit 0
fi

# Fallback for Waybar
if pgrep -x waybar >/dev/null; then
    killall -SIGUSR1 waybar 2>/dev/null || pkill -x waybar || true
else
    waybar >/dev/null 2>&1 &
fi
