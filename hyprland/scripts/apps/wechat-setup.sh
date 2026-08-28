#!/bin/bash

set -e

echo "🧹 Removing old WeChat (AUR)..."
sudo pacman -Rns --noconfirm wechat wechat-bin || true

echo "🧼 Cleaning old configs..."
rm -rf ~/.local/share/WeChat_Data
rm -rf ~/.config/wechat*
rm -rf ~/.cache/wechat*

echo "📦 Installing required packages..."
sudo pacman -S --noconfirm flatpak xdg-desktop-portal-hyprland

echo "🌐 Adding Flathub..."
flatpak remote-add --if-not-exists flathub https://flathub.org/repo/flathub.flatpakrepo

echo "⬇️ Installing WeChat..."
flatpak install -y flathub com.tencent.WeChat

echo "📂 Creating secure share folder..."
mkdir -p ~/WeChatShare

echo "🔐 Applying Flatpak filesystem permission..."
sudo flatpak override com.tencent.WeChat --filesystem=$HOME/WeChatShare

echo "⚙️ Fixing Hyprland environment..."

CONFIG_FILE="$HOME/.config/hypr/conf.d/env.conf"
mkdir -p "$(dirname "$CONFIG_FILE")"

# Remove old XDG_DATA_DIRS entries
sed -i '/XDG_DATA_DIRS/d' "$CONFIG_FILE" 2>/dev/null || true

# Add correct environment
echo "env = XDG_DATA_DIRS,/usr/local/share:/usr/share:/var/lib/flatpak/exports/share:$HOME/.local/share/flatpak/exports/share" >>"$CONFIG_FILE"

echo "🖥️ Adding alias..."
if ! grep -q "alias wechat=" ~/.bashrc; then
  echo 'alias wechat="flatpak run com.tencent.WeChat"' >>~/.bashrc
fi

echo "🔄 Restarting portal services..."
systemctl --user restart xdg-desktop-portal || true
systemctl --user restart xdg-desktop-portal-hyprland || true

echo "🔄 Reloading Hyprland..."
hyprctl reload || true

echo ""
echo "✅ DONE!"
echo "👉 Run: wechat"
echo "👉 Use folder: ~/WeChatShare"
