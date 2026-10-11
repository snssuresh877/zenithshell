#!/usr/bin/env bash
set -euo pipefail

# Trigger ZenithShell native Spotlight launcher directly
zenithctl launcher 2>/dev/null || gdbus call --session --dest dev.zenith.Shell --object-path /dev/zenith/Shell --method dev.zenith.Shell.ToggleSpotlight >/dev/null 2>&1 || rofi -show drun
