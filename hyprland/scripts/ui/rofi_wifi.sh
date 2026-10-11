#!/usr/bin/env bash
set -euo pipefail

# Get active Wi-Fi interface
IFACE=$(ip route | awk '/default/ {print $5}' | head -n1 || echo "wlan0")

# Rescan Wi-Fi networks
nmcli dev wifi rescan 2>/dev/null || true

# Get list of Wi-Fi networks
NETWORKS=$(nmcli -t -f IN-USE,SSID,SIGNAL,SECURITY dev wifi list 2>/dev/null | awk -F: '
{
    in_use = ($1 == "*") ? "󰤨 *" : "󰤢 "
    ssid = $2
    signal = $3 "%"
    security = ($4 == "") ? "Open" : $4
    if (ssid != "") {
        printf "%s  %-25s  %-6s  %s\n", in_use, ssid, signal, security
    }
}
' | sort -u -k2,2)

# Add control options
MENU_LIST=$(cat << EOF
$NETWORKS
─────────────────────────────
󰤨  Toggle Wi-Fi On/Off
⚙  Advanced Connection Editor
EOF
)

CHOSEN=$(echo "$MENU_LIST" | rofi -dmenu -p "Wi-Fi Networks" -i)

[[ -z "$CHOSEN" ]] && exit 0

if [[ "$CHOSEN" == *"Toggle Wi-Fi On/Off"* ]]; then
    STATUS=$(nmcli radio wifi)
    if [ "$STATUS" = "enabled" ]; then
        nmcli radio wifi off
        notify-send -i network-wireless-disconnected "Wi-Fi" "Wi-Fi Disabled"
    else
        nmcli radio wifi on
        notify-send -i network-wireless "Wi-Fi" "Wi-Fi Enabled"
    fi
elif [[ "$CHOSEN" == *"Advanced Connection Editor"* ]]; then
    nm-connection-editor
else
    # Extract SSID
    SSID=$(echo "$CHOSEN" | awk '{print $2}')
    
    if [ -n "$SSID" ]; then
        # Check if connection already exists
        if nmcli connection show "$SSID" >/dev/null 2>&1; then
            nmcli connection up "$SSID" && notify-send -i network-wireless "Wi-Fi Connected" "Connected to $SSID"
        else
            # Prompt for password if needed
            PASS=$(rofi -dmenu -p "Enter Password for $SSID" -password)
            if [ -n "$PASS" ]; then
                nmcli dev wifi connect "$SSID" password "$PASS" && notify-send -i network-wireless "Wi-Fi Connected" "Connected to $SSID"
            fi
        fi
    fi
fi
