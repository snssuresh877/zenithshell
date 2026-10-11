#!/usr/bin/env bash
set -euo pipefail
systemctl poweroff || shutdown -h now
