#!/bin/bash

echo "======================================="
echo "   Arch Linux System Cleanup Script"
echo "======================================="

# 1. Pacman cache (FULL CLEAN)
echo "▶ Cleaning pacman cache..."
sudo pacman -Sc --noconfirm

# 2. Remove orphan packages
echo "▶ Removing orphan packages..."
orphans=$(pacman -Qtdq)
if [ -n "$orphans" ]; then
  sudo pacman -Rns --noconfirm $orphans
else
  echo "✔ No orphan packages found"
fi

# 3. yay / AUR cache
echo "▶ Cleaning yay cache..."
rm -rf ~/.cache/yay/*

# 4. User cache
echo "▶ Cleaning user cache..."
rm -rf ~/.cache/*

# 5. Thumbnail cache
echo "▶ Cleaning thumbnails..."
rm -rf ~/.cache/thumbnails/*

# 6. Journal logs (keep last 3 days)
echo "▶ Cleaning system logs..."
sudo journalctl --vacuum-time=3d

# 7. Trash
echo "▶ Emptying trash..."
rm -rf ~/.local/share/Trash/*

# 8. Flatpak unused (if installed)
if command -v flatpak &>/dev/null; then
  echo "▶ Cleaning Flatpak..."
  flatpak uninstall --unused -y
else
  echo "✔ Flatpak not installed"
fi

# 9. Rebuild font cache
echo "▶ Rebuilding font cache..."
fc-cache -rv >/dev/null 2>&1

echo "======================================="
echo "   Cleanup completed successfully ✅"
echo "======================================="
