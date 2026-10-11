#!/usr/bin/env bash
set -euo pipefail

# Check pacman updates
UPDATES_PACMAN=$(checkupdates 2>/dev/null | wc -l || echo "0")
UPDATES_FLATPAK=$(flatpak update --appstream >/dev/null 2>&1 && flatpak remote-ls --updates 2>/dev/null | wc -l || echo "0")

TOTAL_UPDATES=$((UPDATES_PACMAN + UPDATES_FLATPAK))

if [ "$TOTAL_UPDATES" -gt 0 ]; then
    notify-send -i software-update-available \
        -u normal \
        "System Updates Available" \
        "$TOTAL_UPDATES package update(s) ready to install\nPacman: $UPDATES_PACMAN | Flatpak: $UPDATES_FLATPAK"
fi
