#!/usr/bin/env bash
# Open ZenithShell Network & Wi-Fi Management Panel

if pgrep -x zenithshell >/dev/null; then
    gdbus call --session --dest dev.zenith.Shell --object-path /dev/zenith/Shell --method dev.zenith.Shell.ToggleNetwork >/dev/null 2>&1
else
    # Fallback to nm-connection-editor if shell is not running
    nm-connection-editor &
fi
