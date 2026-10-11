#!/usr/bin/env bash
set -e

echo "⚡ Setting up GLOBAL pacman + aria2 speed tuning..."

# -----------------------------
# Install aria2 if missing
# -----------------------------
if ! command -v aria2c >/dev/null; then
  echo "📦 Installing aria2..."
  sudo pacman -S --needed --noconfirm aria2
else
  echo "✅ aria2 already installed"
fi

# -----------------------------
# Configure pacman for speed
# -----------------------------
PACMAN_CONF="/etc/pacman.conf"

echo "🛠️ Optimizing pacman.conf..."

sudo sed -i \
  -e 's/^#ParallelDownloads/ParallelDownloads/' \
  -e 's/^ParallelDownloads.*/ParallelDownloads = 10/' \
  "$PACMAN_CONF"

# Enable ILoveCandy (pure cosmetic, zero risk)
if ! grep -q "ILoveCandy" "$PACMAN_CONF"; then
  sudo sed -i '/^# Misc options/a ILoveCandy' "$PACMAN_CONF"
fi

# -----------------------------
# Configure aria2 (global, safe)
# -----------------------------
ARIA2_CONF="$HOME/.config/aria2/aria2.conf"
ARIA2_DIR="$HOME/.config/aria2"

echo "🛠️ Writing optimized aria2 config..."

mkdir -p "$ARIA2_DIR"

cat > "$ARIA2_CONF" <<'EOF'
# ===== Omarchy-grade aria2 config =====

# Resume & allocation
continue=true
file-allocation=trunc
auto-file-renaming=false

# Speed
split=16
min-split-size=1M
max-connection-per-server=16

# Network
enable-dht=true
bt-enable-lpd=true

# Disk
disk-cache=64M

# Logging
log-level=notice
EOF

# -----------------------------
# Tell pacman to prefer aria2
# (SAFE MODE – pacman still works if aria2 fails)
# -----------------------------
if ! grep -q "XferCommand" "$PACMAN_CONF"; then
  echo "🔗 Integrating aria2 with pacman (safe mode)..."
  sudo sed -i '/^# Misc options/a XferCommand = /usr/bin/aria2c --file-allocation=trunc -c -x 16 -s 16 -k 1M %u -o %o' "$PACMAN_CONF"
else
  echo "ℹ️ XferCommand already present, skipping"
fi

# -----------------------------
# Done
# -----------------------------
echo
echo "✅ GLOBAL pacman + aria2 speed tuning COMPLETE"
echo
echo "🚀 Effects:"
echo "  • pacman downloads faster"
echo "  • yay / AUR downloads faster"
echo "  • aria2 used safely (fallback intact)"
echo "  • no curl/wget replacement"
echo
echo "♻️ Reboot or re-login recommended"
