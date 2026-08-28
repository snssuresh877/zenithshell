#!/usr/bin/env bash
# ==========================================
# Arch Linux Professional Backup Script
# Optimized for Hyprland + Workstation setups
# ==========================================

set -Eeuo pipefail

# ==========================================
# ERROR HANDLING
# ==========================================

trap 'echo;
echo "❌ Backup failed at line $LINENO";
notify-send "Backup Failed" "Check terminal output";
exit 1' ERR

# ==========================================
# HELPERS
# ==========================================

msg() {
  printf "\n==> %s\n" "$1"
}

ok() {
  printf "✔ %s\n" "$1"
}

warn() {
  printf "⚠ %s\n" "$1"
}

require_cmd() {
  command -v "$1" >/dev/null 2>&1 || {
    echo "❌ Missing dependency: $1"
    notify-send "Backup Failed" "Missing dependency: $1"
    exit 1
  }
}

# ==========================================
# DEPENDENCY CHECKS
# ==========================================

require_cmd rsync
require_cmd tar
require_cmd notify-send

# zstd optional
USE_ZSTD=1

if [[ $USE_ZSTD -eq 1 ]]; then
  require_cmd zstd
fi

# ==========================================
# SUDO PRECHECK
# ==========================================

sudo -v

# Keep sudo alive
while true; do
  sudo -n true
  sleep 60
  kill -0 "$$" || exit
done 2>/dev/null &

SUDO_KEEPALIVE_PID=$!

trap 'kill $SUDO_KEEPALIVE_PID 2>/dev/null || true' EXIT

# ==========================================
# SETTINGS
# ==========================================

DATE="$(date +%F_%H-%M-%S)"
HOST="$(cat /etc/hostname 2>/dev/null || uname -n)"

BACKUP_NAME="system-backup-${HOST}-${DATE}"

BACKUP_DIR="$HOME/$BACKUP_NAME"

if [[ $USE_ZSTD -eq 1 ]]; then
  ARCHIVE="$HOME/${BACKUP_NAME}.tar.zst"
else
  ARCHIVE="$HOME/${BACKUP_NAME}.tar.gz"
fi

# ==========================================
# DISK SPACE CHECK
# ==========================================

AVAILABLE=$(df "$HOME" --output=avail | tail -1)

if ((AVAILABLE < 4000000)); then
  warn "Low disk space detected"
  notify-send "Backup Warning" "Low disk space detected"
fi

# ==========================================
# START
# ==========================================

clear

echo "==========================================="
echo " Arch Linux Professional Backup"
echo "==========================================="
echo " Host   : $HOST"
echo " Folder : $BACKUP_DIR"
echo " Archive: $ARCHIVE"
echo "==========================================="

mkdir -p "$BACKUP_DIR"/{config,home,system}
mkdir -p "$BACKUP_DIR/config/shell"
mkdir -p "$BACKUP_DIR/home/.local/share"
mkdir -p "$BACKUP_DIR/home/.mozilla"

# ==========================================
# PACKAGE LISTS
# ==========================================

msg "Saving package lists"

pacman -Qqe >"$BACKUP_DIR/pkglist-pacman.txt"
pacman -Qqm >"$BACKUP_DIR/pkglist-aur.txt"
pacman -Q >"$BACKUP_DIR/pkglist-full-version.txt"
pacman -Qent >"$BACKUP_DIR/pkglist-explicit-native.txt"
pacman -Qemt >"$BACKUP_DIR/pkglist-explicit-foreign.txt"

if command -v flatpak >/dev/null 2>&1; then
  flatpak list --app >"$BACKUP_DIR/pkglist-flatpak.txt"
fi

ok "Package lists saved"

# ==========================================
# SHELL CONFIGS
# ==========================================

msg "Backing up shell configs"

for file in .bashrc .bash_profile .zshrc .profile; do
  if [[ -f "$HOME/$file" ]]; then
    cp -f "$HOME/$file" "$BACKUP_DIR/"
  fi
done

ok "Shell configs backed up"

# ==========================================
# USER CONFIGS
# ==========================================

msg "Backing up ~/.config"

for dir in \
  hypr \
  waybar \
  kitty \
  nvim \
  rofi \
  swaync \
  wlogout \
  fastfetch \
  aria2 \
  gtk-3.0 \
  gtk-4.0 \
  nwg-look \
  systemd; do
  if [[ -d "$HOME/.config/$dir" ]]; then
    rsync -a \
      --exclude='node_modules' \
      --exclude='.cache' \
      --exclude='Cache' \
      --exclude='GPUCache' \
      "$HOME/.config/$dir/" \
      "$BACKUP_DIR/config/$dir/"
  fi
done

if [[ -f "$HOME/.config/starship.toml" ]]; then
  cp -f "$HOME/.config/starship.toml" \
    "$BACKUP_DIR/config/shell/"
fi

if [[ -d "$HOME/.config/wal" ]]; then
  rsync -a "$HOME/.config/wal/" \
    "$BACKUP_DIR/config/wal/"
fi

ok "Configs backed up"

# ==========================================
# FIREFOX
# ==========================================

msg "Backing up Firefox"

if [[ -d "$HOME/.mozilla/firefox" ]]; then
  rsync -a \
    --exclude='*/cache2' \
    --exclude='*/thumbnails' \
    --exclude='*/startupCache' \
    --exclude='*/Cache' \
    --exclude='*/GPUCache' \
    "$HOME/.mozilla/firefox/" \
    "$BACKUP_DIR/home/.mozilla/firefox/"
fi

ok "Firefox backed up"

# ==========================================
# PERSONAL ASSETS
# ==========================================

msg "Backing up personal assets"

if [[ -d "$HOME/Pictures/wallpapers" ]]; then
  rsync -a \
    "$HOME/Pictures/wallpapers/" \
    "$BACKUP_DIR/config/wallpapers/"
fi

if [[ -d "$HOME/.icons" ]]; then
  rsync -a "$HOME/.icons/" \
    "$BACKUP_DIR/home/.icons/"
fi

if [[ -d "$HOME/.themes" ]]; then
  rsync -a "$HOME/.themes/" \
    "$BACKUP_DIR/home/.themes/"
fi

if [[ -d "$HOME/.local/share/fonts" ]]; then
  rsync -a \
    "$HOME/.local/share/fonts/" \
    "$BACKUP_DIR/home/.local/share/fonts/"
fi

if [[ -d "$HOME/.local/bin" ]]; then
  rsync -a \
    "$HOME/.local/bin/" \
    "$BACKUP_DIR/home/.local/bin/"
fi

ok "Assets backed up"

# ==========================================
# SYSTEM CONFIGS
# ==========================================

msg "Backing up system configs"

SYSTEM_DIR="$BACKUP_DIR/system"

# Boot
[[ -f /etc/mkinitcpio.conf ]] && sudo cp -f /etc/mkinitcpio.conf "$SYSTEM_DIR/"
[[ -f /etc/default/grub ]] && sudo cp -f /etc/default/grub "$SYSTEM_DIR/"
[[ -d /etc/grub.d ]] && sudo cp -r /etc/grub.d "$SYSTEM_DIR/"
[[ -f /boot/grub/grub.cfg ]] && sudo cp -f /boot/grub/grub.cfg "$SYSTEM_DIR/"

# Power
[[ -f /etc/tlp.conf ]] && sudo cp -f /etc/tlp.conf "$SYSTEM_DIR/"
[[ -d /etc/tlp.d ]] && sudo cp -r /etc/tlp.d "$SYSTEM_DIR/"
[[ -f /etc/systemd/sleep.conf ]] && sudo cp -f /etc/systemd/sleep.conf "$SYSTEM_DIR/"

# logind
[[ -f /etc/systemd/logind.conf ]] && sudo cp -f /etc/systemd/logind.conf "$SYSTEM_DIR/"
[[ -d /etc/systemd/logind.conf.d ]] && sudo cp -r /etc/systemd/logind.conf.d "$SYSTEM_DIR/"

# sudo
[[ -f /etc/sudoers ]] && sudo cp -f /etc/sudoers "$SYSTEM_DIR/"
[[ -d /etc/sudoers.d ]] && sudo cp -r /etc/sudoers.d "$SYSTEM_DIR/"

# NetworkManager
[[ -d /etc/NetworkManager ]] && sudo cp -r /etc/NetworkManager "$SYSTEM_DIR/"

# Core system files
for file in environment hosts locale.conf hostname; do
  [[ -f "/etc/$file" ]] &&
    sudo cp -f "/etc/$file" "$SYSTEM_DIR/"
done

sudo chown -R "$USER:$USER" "$SYSTEM_DIR"

ok "System configs backed up"

# ==========================================
# SYSTEM INFO
# ==========================================

msg "Saving system info"

uname -a >"$BACKUP_DIR/system-info.txt"
lsblk -f >"$BACKUP_DIR/disk-layout.txt"
df -h >"$BACKUP_DIR/storage-usage.txt"
free -h >"$BACKUP_DIR/memory-info.txt"

ok "System info saved"

# ==========================================
# COMPRESS
# ==========================================

msg "Creating archive"

[[ -d "$BACKUP_DIR" ]] || {
  echo "❌ Backup directory missing"
  exit 1
}

cd "$HOME"

if [[ "$ARCHIVE" == *.tar.zst ]]; then
  tar -I 'zstd -19 -T0 --progress' \
    -cf "$ARCHIVE" \
    "$BACKUP_NAME"
else
  tar -czf "$ARCHIVE" "$BACKUP_NAME"
fi

ok "Archive created"

# ==========================================
# CLEANUP OLD BACKUPS
# ==========================================

find "$HOME" \
  -maxdepth 1 \
  -name "system-backup-*" \
  -mtime +14 \
  -exec rm -rf {} + 2>/dev/null || true

# ==========================================
# FINISH
# ==========================================

echo
echo "==========================================="
echo " BACKUP COMPLETED SUCCESSFULLY"
echo "==========================================="
echo " Folder : $BACKUP_DIR"
echo " Archive: $ARCHIVE"
echo

ls -lh "$ARCHIVE"

echo "==========================================="

notify-send "Backup Completed" "$ARCHIVE"
