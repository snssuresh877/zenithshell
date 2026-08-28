#!/bin/bash
set -e

echo "▶ Installing aria2..."
sudo pacman -S --needed --noconfirm aria2 ethtool iproute2

echo "▶ Creating directories..."
mkdir -p ~/.config/aria2
mkdir -p ~/.local/bin

echo "▶ Writing aria2.conf..."
cat > ~/.config/aria2/aria2.conf << 'EOF'
# ===== BASIC =====
continue=true
file-allocation=trunc
auto-file-renaming=false
summary-interval=60
log-level=notice

# ===== CONNECTION OPTIMIZATION =====
split=16
min-split-size=5M
max-connection-per-server=16
reuse-connection=true

# ===== DOWNLOAD PERFORMANCE =====
piece-length=1M

# ===== NETWORK STABILITY =====
timeout=60
connect-timeout=60
retry-wait=5
max-tries=5

# ===== DISK =====
disk-cache=64M
enable-mmap=false
EOF

echo "▶ Writing aria2 auto-scale script..."
cat > ~/.local/bin/aria2-autoscale.sh << 'EOF'
#!/bin/bash

# Detect active network interface
IFACE=$(ip route get 1.1.1.1 2>/dev/null | awk '{print $5; exit}')

# Try to get link speed (Mbps)
SPEED=$(ethtool "$IFACE" 2>/dev/null | awk -F': ' '/Speed/ {gsub("Mb/s",""); print $2}')

# Fallback for Wi-Fi / unknown
[[ -z "$SPEED" ]] && SPEED=50

# Defaults
X=8
K=1M

if (( SPEED < 20 )); then
    X=6;  K=512K
elif (( SPEED < 50 )); then
    X=8;  K=1M
elif (( SPEED < 120 )); then
    X=16; K=1M
elif (( SPEED < 300 )); then
    X=24; K=2M
else
    X=32; K=4M
fi

echo "-x$X -k$K"
EOF

echo "▶ Making script executable..."
chmod +x ~/.local/bin/aria2-autoscale.sh

echo "▶ Ensuring ~/.local/bin is in PATH..."
if ! echo "$PATH" | grep -q "$HOME/.local/bin"; then
    echo 'export PATH="$HOME/.local/bin:$PATH"' >> ~/.bashrc
    echo "   Added to ~/.bashrc"
fi

echo "✅ Installation complete!"
echo "➡ Test with: aria2-autoscale.sh"
echo "➡ Use with yt-dlp: --downloader-args \"aria2c:\$(aria2-autoscale.sh)\""
