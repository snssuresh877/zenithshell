#!/usr/bin/env bash
# arch-backup-packages.sh
# Backup package/app lists on Arch Linux

set -euo pipefail

DATE="$(date +%F_%H-%M-%S)"
BACKUP_DIR="$HOME/system-backup-$DATE"

mkdir -p "$BACKUP_DIR"

echo "==> Creating backup folder:"
echo "    $BACKUP_DIR"
echo

# ---------------------------------
# Official repo packages
# ---------------------------------
echo "==> Saving pacman explicit package list..."
pacman -Qqe >"$BACKUP_DIR/pkglist-pacman.txt"

# ---------------------------------
# Foreign / AUR packages
# ---------------------------------
echo "==> Saving foreign/AUR package list..."
pacman -Qqm >"$BACKUP_DIR/pkglist-aur.txt"

# ---------------------------------
# Full package list with versions
# ---------------------------------
echo "==> Saving full package list..."
pacman -Q >"$BACKUP_DIR/pkglist-full-version.txt"

# ---------------------------------
# yay explicit list (if installed)
# ---------------------------------
if command -v yay >/dev/null 2>&1; then
  echo "==> Saving yay explicit package list..."
  yay -Qqe >"$BACKUP_DIR/pkglist-all.txt"
fi

# ---------------------------------
# Flatpak apps
# ---------------------------------
if command -v flatpak >/dev/null 2>&1; then
  echo "==> Saving Flatpak app list..."
  flatpak list --app >"$BACKUP_DIR/pkglist-flatpak.txt"
fi

# ---------------------------------
# Snap apps
# ---------------------------------
if command -v snap >/dev/null 2>&1; then
  echo "==> Saving Snap app list..."
  snap list >"$BACKUP_DIR/pkglist-snap.txt" 2>/dev/null || true
fi

# ---------------------------------
# Optional configs backup
# ---------------------------------
echo "==> Backing up shell configs..."
cp -f "$HOME/.bashrc" "$BACKUP_DIR/" 2>/dev/null || true
cp -f "$HOME/.bash_profile" "$BACKUP_DIR/" 2>/dev/null || true

echo "==> Backing up key config folders..."
mkdir -p "$BACKUP_DIR/config"

cp -r "$HOME/.config/hypr" "$BACKUP_DIR/config/" 2>/dev/null || true
cp -r "$HOME/.config/waybar" "$BACKUP_DIR/config/" 2>/dev/null || true
cp -r "$HOME/.config/kitty" "$BACKUP_DIR/config/" 2>/dev/null || true
cp -r "$HOME/.config/nvim" "$BACKUP_DIR/config/" 2>/dev/null || true

# ---------------------------------
# Summary
# ---------------------------------
echo
echo "======================================"
echo " Backup completed successfully"
echo " Location: $BACKUP_DIR"
echo "======================================"
echo

ls -lh "$BACKUP_DIR"
