#!/usr/bin/env bash
set -euo pipefail

# Trigger ZenithShell Native Keybindings Overlay
gdbus call --session --dest dev.zenith.Shell --object-path /dev/zenith/Shell --method dev.zenith.Shell.ToggleKeybinds >/dev/null 2>&1 || true
