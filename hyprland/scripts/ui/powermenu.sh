#!/usr/bin/env bash
set -euo pipefail

STATE_FILE="$HOME/.config/hypr/ui_mode"
MODE="zenithshell"

if [ -f "$STATE_FILE" ]; then
    MODE=$(cat "$STATE_FILE" | tr -d '[:space:]')
fi

# If ZenithShell is running and active mode, trigger native Zenith PowerMenu
if [ "$MODE" = "zenithshell" ] && pgrep -x zenithshell >/dev/null; then
    gdbus call --session --dest dev.zenith.Shell --object-path /dev/zenith/Shell --method dev.zenith.Shell.TogglePowerMenu >/dev/null 2>&1
    exit 0
fi

# Fallback to Rofi menu if in Waybar mode or ZenithShell is not active
choice=$(printf " Lock\n Suspend\n󰍃 Logout\n Reboot\n⏻ Power Off" |
  rofi -dmenu -p "Power Menu")

[[ -z "$choice" ]] && exit 0

case "$choice" in
" Lock")
  "$HOME/.config/hypr/scripts/ui/lock_screen.sh"
  ;;
" Suspend")
  loginctl lock-session
  systemctl suspend
  ;;
"󰍃 Logout")
  "$HOME/.config/hypr/scripts/ui/logout_session.sh"
  ;;
" Reboot")
  "$HOME/.config/hypr/scripts/ui/reboot_system.sh"
  ;;
"⏻ Power Off")
  "$HOME/.config/hypr/scripts/ui/power_off.sh"
  ;;
esac
