#!/usr/bin/env bash
set -euo pipefail
if command -v hyprshade >/dev/null 2>&1; then
    hyprshade toggle blue-light-filter
fi
