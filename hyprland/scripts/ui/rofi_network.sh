#!/usr/bin/env bash
set -euo pipefail

# Find active network interface
IFACE=$(ip route get 1.1.1.1 2>/dev/null | awk '{ for (i = 1; i <= NF; i++) if ($i == "dev") { print $(i + 1); exit } }' || echo "")
IP_ADDR=$(ip a show "$IFACE" 2>/dev/null | awk '/inet / {print $2}' | cut -d/ -f1 || echo "No IP")

SSID=""
SIGNAL=""
if [ -n "$IFACE" ]; then
    SSID=$(nmcli -t -f GENERAL.CONNECTION dev show "$IFACE" 2>/dev/null | head -n 1 | cut -d: -f2 || true)
    SIGNAL=$(nmcli -t -f IN-USE,SIGNAL dev wifi list ifname "$IFACE" --rescan no 2>/dev/null | awk -F: '$1 == "*" { print $2; exit }' || echo "100")
fi

SSID_STR="${SSID:-Disconnected}"
SIGNAL_STR="${SIGNAL:+$SIGNAL%}"

# Available Wi-Fi Networks
nmcli dev wifi rescan 2>/dev/null || true

WIFI_LIST=$(nmcli -t -f IN-USE,SSID,SIGNAL,SECURITY dev wifi list 2>/dev/null | awk -F: '
{
    in_use = ($1 == "*") ? "󰤨 *" : "󰤢 "
    ssid = $2
    signal = $3 "%"
    security = ($4 == "") ? "Open" : $4
    if (ssid != "") {
        printf "%s  %-24s  %-6s  %s\n", in_use, ssid, signal, security
    }
}
' | sort -u -k2,2)

MENU_HEADER="Active: $SSID_STR ($SIGNAL_STR) | IP: $IP_ADDR"

MENU_OPTIONS=$(cat << MENU_EOF
🔑 Show Active Wi-Fi Password ($SSID_STR)
📱 Share Wi-Fi via QR Code
⚡ Run Network Speed & Latency Test
ℹ️  Full Network Diagnostics
─────────────────────────────
$WIFI_LIST
─────────────────────────────
🔌 Toggle Wi-Fi On/Off
⚙  Advanced Connection Manager (nm-connection-editor)
MENU_EOF
)

CHOSEN=$(echo "$MENU_OPTIONS" | rofi -dmenu -p "Network ($MENU_HEADER)" -i)

[[ -z "$CHOSEN" ]] && exit 0

case "$CHOSEN" in
    🔑*)
        if [ -n "$SSID" ]; then
            PASS=$(nmcli -s -g 802-11-wireless-security.psk connection show "$SSID" 2>/dev/null || echo "No Saved Password")
            echo "$PASS" | wl-copy 2>/dev/null || true
            rofi -e "Wi-Fi Network: $SSID_STR
Password: $PASS

(Copied to clipboard)"
        fi
        ;;
    📱*)
        if [ -n "$SSID" ]; then
            PASS=$(nmcli -s -g 802-11-wireless-security.psk connection show "$SSID" 2>/dev/null || echo "")
            QR_STR="WIFI:S:$SSID;T:WPA;P:$PASS;;"
            if command -v qrencode >/dev/null 2>&1; then
                qrencode -t PNG -o /tmp/wifi_qr.png "$QR_STR" 2>/dev/null && foot -T "Wi-Fi QR Code" -e chafa /tmp/wifi_qr.png &
            fi
        fi
        ;;
    ⚡*)
        foot -T "Network Speedtest" -e bash -c "curl -s https://raw.githubusercontent.com/sivel/speedtest-cli/master/speedtest.py | python3 -; read -p 'Press enter to exit...'" &
        ;;
    ℹ️*)
        foot -T "Network Diagnostics" -e bash -c "ip a; echo '---'; ip route; echo '---'; ping -c 4 1.1.1.1; read -p 'Press enter to exit...'" &
        ;;
    🔌*)
        CURRENT_STATUS=$(nmcli radio wifi)
        if [ "$CURRENT_STATUS" = "enabled" ]; then
            nmcli radio wifi off
            notify-send "Wi-Fi" "Wi-Fi Disabled"
        else
            nmcli radio wifi on
            notify-send "Wi-Fi" "Wi-Fi Enabled"
        fi
        ;;
    ⚙*)
        nm-connection-editor &
        ;;
    *)
        CLEAN_SSID=$(echo "$CHOSEN" | awk '{print $2}')
        if [ -n "$CLEAN_SSID" ]; then
            PASSWORD=$(rofi -dmenu -p "Enter Password for $CLEAN_SSID:" -password)
            if [ -n "$PASSWORD" ]; then
                nmcli dev wifi connect "$CLEAN_SSID" password "$PASSWORD" || notify-send "Wi-Fi" "Failed to connect"
            else
                nmcli dev wifi connect "$CLEAN_SSID" || notify-send "Wi-Fi" "Failed to connect"
            fi
        fi
        ;;
esac
