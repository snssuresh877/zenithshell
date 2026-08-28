#!/usr/bin/env bash

set -euo pipefail

source "$HOME/.cache/wal/colors.sh"

mkdir -p "$HOME/.config/departure"

cat >"$HOME/.config/departure/style.css" <<EOF
window {
    background-color: ${background}CC;
}

button {
    background-color: ${color0};
    color: ${foreground};

    border-radius: 18px;
    border: 2px solid ${color4};

    min-width: 150px;
    min-height: 150px;

    font-size: 18px;

    margin: 10px;

    transition: all 180ms ease;
}

button:hover {
    background-color: ${color4};
    color: ${background};
}

label {
    font-family: "JetBrainsMono Nerd Font";
    font-weight: bold;
}
EOF
