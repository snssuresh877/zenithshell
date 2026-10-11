#!/usr/bin/env bash
set -e

echo "🔧 Optimizing ZRAM & Kernel Memory Settings..."

# -------------------------------
# 1️⃣ ZRAM CONFIG
# -------------------------------
echo "📦 Configuring ZRAM..."

sudo mkdir -p /etc/systemd
sudo tee /etc/systemd/zram-generator.conf >/dev/null <<'EOF'
[zram0]
zram-size = ram * 1.5
compression-algorithm = zstd
swap-priority = 100
EOF

echo "🔄 Applying ZRAM configuration..."
sudo systemctl daemon-reexec
sudo systemctl restart systemd-zram-setup@zram0 || true

# -------------------------------
# 2️⃣ KERNEL MEMORY TUNING
# -------------------------------
echo "🧠 Applying kernel RAM pressure tuning..."

sudo tee /etc/sysctl.d/99-performance.conf >/dev/null <<'EOF'
vm.swappiness=10
vm.vfs_cache_pressure=50
vm.dirty_ratio=15
vm.dirty_background_ratio=5
vm.overcommit_memory=1
EOF

echo "⚙️ Reloading sysctl settings..."
sudo sysctl --system

# -------------------------------
# 3️⃣ STATUS CHECK
# -------------------------------
echo ""
echo "✅ ZRAM STATUS:"
swapon --show

echo ""
echo "✅ MEMORY PARAMETERS:"
sysctl vm.swappiness vm.vfs_cache_pressure vm.dirty_ratio vm.dirty_background_ratio vm.overcommit_memory

echo ""
echo "🎉 Optimization complete!"
