#!/usr/bin/env bash
set -euo pipefail

CURRENT=$(gsettings get org.gnome.desktop.interface color-scheme 2>/dev/null || echo "'prefer-light'")

if [ "$CURRENT" = "'prefer-dark'" ]; then
    gsettings set org.gnome.desktop.interface color-scheme 'prefer-light'
    gsettings set org.gnome.desktop.interface gtk-theme 'Orchis-Light' 2>/dev/null || true
    echo "light"
else
    gsettings set org.gnome.desktop.interface color-scheme 'prefer-dark'
    gsettings set org.gnome.desktop.interface gtk-theme 'Orchis-Dark' 2>/dev/null || true
    echo "dark"
fi
