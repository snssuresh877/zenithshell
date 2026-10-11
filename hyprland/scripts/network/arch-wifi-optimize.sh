#!/usr/bin/env bash
# Arch Linux: Switch NetworkManager Wi-Fi backend to iwd
# Safe setup script with backup + restart

set -e

echo "==> Installing iwd if missing..."
sudo pacman -Sy --needed iwd

echo "==> Creating NetworkManager config directory..."
sudo mkdir -p /etc/NetworkManager/conf.d

# Backup old config if exists
if [ -f /etc/NetworkManager/conf.d/wifi_backend.conf ]; then
  echo "==> Backing up existing config..."
  sudo cp /etc/NetworkManager/conf.d/wifi_backend.conf \
    /etc/NetworkManager/conf.d/wifi_backend.conf.bak.$(date +%F-%H%M%S)
fi

echo "==> Writing iwd backend config..."
sudo tee /etc/NetworkManager/conf.d/wifi_backend.conf >/dev/null <<EOF
[device]
wifi.backend=iwd
EOF

echo "==> Enabling iwd service..."
sudo systemctl enable iwd.service

echo "==> Restarting NetworkManager..."
sudo systemctl restart NetworkManager

echo "==> Waiting 3 seconds..."
sleep 3

echo "==> Status check:"
systemctl --no-pager --full status NetworkManager | head -20
echo
systemctl --no-pager --full status iwd | head -20

echo
echo "Done."
echo "If Wi-Fi disconnects once, reconnect normally."
echo "Reboot recommended."
