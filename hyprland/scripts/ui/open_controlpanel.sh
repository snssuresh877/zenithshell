#!/usr/bin/env bash
set -euo pipefail

# Trigger ZenithShell native Quick Settings / Control Center directly
zenithctl cc 2>/dev/null || gdbus call --session --dest dev.zenith.Shell --object-path /dev/zenith/Shell --method dev.zenith.Shell.ToggleControlCenter >/dev/null 2>&1 || "$HOME/.config/hypr/scripts/ui/rofi-controlpanel.sh"
