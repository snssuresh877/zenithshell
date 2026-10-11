#!/usr/bin/env bash
set -euo pipefail

# Trigger ZenithShell native Clipboard Manager directly
zenithctl clipboard 2>/dev/null || (cliphist list | rofi -dmenu -p "Clipboard" | cliphist decode | wl-copy 2>/dev/null)
