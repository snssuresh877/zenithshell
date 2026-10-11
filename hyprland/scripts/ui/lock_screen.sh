#!/usr/bin/env bash
set -euo pipefail
if command -v hyprlock >/dev/null 2>&1; then
    hyprlock
else
    loginctl lock-session
fi
