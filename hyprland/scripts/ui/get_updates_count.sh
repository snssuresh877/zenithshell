#!/usr/bin/env bash
set -euo pipefail

PACMAN_COUNT=$(checkupdates 2>/dev/null | wc -l || echo "0")
FLATPAK_COUNT=$(flatpak remote-ls --updates 2>/dev/null | wc -l || echo "0")
TOTAL=$((PACMAN_COUNT + FLATPAK_COUNT))

if [ "$TOTAL" -gt 0 ]; then
    echo "󰏔  Updates ($TOTAL)"
else
    echo "󰏔  Up to date"
fi
