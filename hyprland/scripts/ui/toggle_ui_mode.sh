#!/usr/bin/env bash
set -euo pipefail

STATE_FILE="$HOME/.config/hypr/ui_mode"
CURRENT_MODE="zenithshell"

if [ -f "$STATE_FILE" ]; then
    CURRENT_MODE=$(cat "$STATE_FILE" | tr -d '[:space:]')
fi

TARGET_MODE="${1:-}"

if [ -z "$TARGET_MODE" ]; then
    case "$CURRENT_MODE" in
        "zenithshell")
            TARGET_MODE="waybar"
            ;;
        *)
            TARGET_MODE="zenithshell"
            ;;
    esac
fi

case "$TARGET_MODE" in
    "zenithshell")
        # Switch to ZenithShell (Native C++ Desktop Shell)
        pkill -9 -f zenithshell 2>/dev/null || true
        pkill -f waybar 2>/dev/null || true
        pkill -f swaync 2>/dev/null || true
        pkill -f nm-applet 2>/dev/null || true
        sleep 0.3
        setsid "$HOME/.local/bin/zenithshell" >/tmp/zenithshell.log 2>&1 &
        echo "zenithshell" > "$STATE_FILE"
        notify-send -i preferences-desktop-theme "UI Mode Active" "ZenithShell (Native C++20 / GTK3 Shell • ~60MB RAM)"
        ;;
    "waybar"|"waybar_rofi_swaync")
        # Switch to Waybar + Rofi + SwayNC
        pkill -9 -f zenithshell 2>/dev/null || true
        pkill -f swaync 2>/dev/null || true
        pkill -f waybar 2>/dev/null || true
        pkill -f nm-applet 2>/dev/null || true
        sleep 0.3
        swaync >/dev/null 2>&1 &
        waybar >/dev/null 2>&1 &
        nm-applet --indicator >/dev/null 2>&1 &
        echo "waybar" > "$STATE_FILE"
        notify-send -i preferences-desktop-theme "UI Mode Active" "Waybar + Rofi + SwayNC (Classic Bar & Notification Center)"
        ;;
    *)
        echo "Unknown UI mode: $TARGET_MODE" >&2
        exit 1
        ;;
esac
