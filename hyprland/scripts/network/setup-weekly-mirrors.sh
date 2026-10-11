#!/usr/bin/env bash
set -e

echo "🔄 Setting up weekly Arch mirror refresh (systemd)..."

# -----------------------------
# Install reflector
# -----------------------------
if ! pacman -Q reflector &>/dev/null; then
  echo "📦 Installing reflector..."
  sudo pacman -S --needed --noconfirm reflector
else
  echo "✅ Reflector already installed"
fi

# -----------------------------
# Create systemd service
# -----------------------------
echo "🛠️ Creating reflector.service..."

sudo tee /etc/systemd/system/reflector.service >/dev/null <<'EOF'
[Unit]
Description=Refresh Arch Linux mirrorlist using Reflector
Wants=network-online.target
After=network-online.target

[Service]
Type=oneshot
ExecStart=/usr/bin/reflector \
  --country India \
  --protocol https \
  --age 12 \
  --latest 20 \
  --sort rate \
  --save /etc/pacman.d/mirrorlist
EOF

# -----------------------------
# Create systemd timer
# -----------------------------
echo "⏱️ Creating reflector.timer..."

sudo tee /etc/systemd/system/reflector.timer >/dev/null <<'EOF'
[Unit]
Description=Weekly Arch Linux mirror refresh

[Timer]
OnCalendar=weekly
Persistent=true

[Install]
WantedBy=timers.target
EOF

# -----------------------------
# Enable timer
# -----------------------------
echo "▶️ Enabling weekly mirror refresh timer..."

sudo systemctl daemon-reload
sudo systemctl enable --now reflector.timer

# -----------------------------
# Optional first run
# -----------------------------
echo "🚀 Running reflector once now (optional but recommended)..."
sudo systemctl start reflector.service

echo
echo "✅ Weekly mirror refresh is now ACTIVE"
echo "📅 Runs automatically every week"
echo "🧠 Script no longer needed"
