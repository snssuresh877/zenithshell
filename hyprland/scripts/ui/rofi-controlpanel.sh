#!/usr/bin/env bash
set -euo pipefail

# Live status queries
DARK_MODE=$(gsettings get org.gnome.desktop.interface color-scheme 2>/dev/null || echo "'prefer-dark'")
IS_DARK=$(echo "$DARK_MODE" | grep -q 'dark' && echo "Dark" || echo "Light")

UI_MODE="waybar"
if [ -f "$HOME/.config/hypr/ui_mode" ]; then
    UI_MODE=$(cat "$HOME/.config/hypr/ui_mode" | tr -d '[:space:]')
fi

if [ "$UI_MODE" = "zenithshell" ]; then
    UI_NAME="ZenithShell (~15MB C++)"
elif [ "$UI_MODE" = "quickshell" ]; then
    UI_NAME="Quickshell (QML)"
else
    UI_NAME="Waybar + SwayNC"
fi

WIFI_SSID=$(nmcli -t -f active,ssid dev wifi 2>/dev/null | grep '^yes' | cut -d: -f2 || true)
WIFI_STR="${WIFI_SSID:-Disconnected}"

VPN_NAME=$(nmcli -t -f TYPE,NAME connection show --active 2>/dev/null | grep -v 'dummy' | grep -iE 'wireguard|vpn|tun|proton' | head -n 1 | cut -d: -f2 || true)
VPN_STR="${VPN_NAME:-Inactive}"

OPTIONS=$(cat << EOF
📶 Network & Wi-Fi ($WIFI_STR)
󰂯 Bluetooth Manager
🔊 Audio Control Mixer
🔒 Active VPN Tunnels ($VPN_STR)
🌙 Toggle Blue Light Filter
󰌵 Toggle Focus Mode (DND)
🎨 Toggle Dark / Light Theme ($IS_DARK)
🖥️ Switch UI Engine (Active: $UI_NAME)
🖼️ Switch Wallpaper
󰍃 Power Menu
EOF
)

CHOICE=$(echo "$OPTIONS" | rofi -dmenu -p "Control Panel" -i)

[[ -z "$CHOICE" ]] && exit 0

case "$CHOICE" in
    *Wi-Fi*|*Network*)
        "$HOME/.config/hypr/scripts/ui/rofi_network.sh"
        ;;
    󰂯*)
        blueman-manager
        ;;
    🔊*)
        pavucontrol
        ;;
    🔒*)
        INFO=$("$HOME/.config/quickshell/scripts/get_network_security_info.sh" 2>/dev/null || echo "Info Unavailable")
        notify-send -i network-wireless "Network & Security" "$INFO"
        ;;
    🌙*)
        "$HOME/.config/hypr/scripts/ui/toggle_nightlight.sh"
        ;;
    󰌵*)
        "$HOME/.config/hypr/scripts/ui/toggle_dnd.sh"
        ;;
    🎨*)
        "$HOME/.config/hypr/scripts/ui/toggle_darkmode.sh"
        ;;
    🖥️*)
        "$HOME/.config/hypr/scripts/ui/toggle_ui_mode.sh"
        ;;
    🖼️*)
        "$HOME/.config/hypr/scripts/ui/switch_wallpaper.sh"
        ;;
    󰍃*)
        "$HOME/.config/hypr/scripts/ui/powermenu.sh"
        ;;
esac
