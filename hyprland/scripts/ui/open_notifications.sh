#!/usr/bin/env bash
set -euo pipefail

# Trigger ZenithShell native Notification History Center directly
zenithctl nc 2>/dev/null || gdbus call --session --dest dev.zenith.Shell --object-path /dev/zenith/Shell --method dev.zenith.Shell.ToggleNotifications >/dev/null 2>&1 || swaync-client -t -sw 2>/dev/null || true
