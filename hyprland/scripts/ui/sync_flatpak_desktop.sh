#!/usr/bin/env bash
set -euo pipefail

TARGET_DIR="$HOME/.local/share/applications"
mkdir -p "$TARGET_DIR"

# Copy and resolve flatpak desktop files into ~/.local/share/applications
for f in /var/lib/flatpak/exports/share/applications/*.desktop "$HOME/.local/share/flatpak/exports/share/applications/"*.desktop; do
    if [ -f "$f" ] || [ -L "$f" ]; then
        filename=$(basename "$f")
        cp -L "$f" "$TARGET_DIR/$filename" 2>/dev/null || true
    fi
done

# Update desktop database
update-desktop-database "$TARGET_DIR" 2>/dev/null || true
