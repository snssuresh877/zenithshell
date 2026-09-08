# 📦 ZenithShell & Hyprland: Complete Package & Dependency Guide

This document is the definitive master reference for setting up **ZenithShell** and a fully featured **Hyprland** workstation. Whether you are **reinstalling Arch Linux from scratch** or a new user wanting a turn-key experience with zero missing packages, broken scripts, or mounting issues, this guide covers everything.

---

## ⚡ 1-Click Automated Setup (Single Confirmation)

If you already have a working Arch, Fedora, or Ubuntu installation, you can install the entire workstation, dotfiles, and dependencies with a single command:

```bash
# Clone the repository
git clone https://github.com/snssuresh877/zenithshell.git ~/Projects/zenithshell
cd ~/Projects/zenithshell

# Interactive setup (press ENTER to install all recommended components)
./setup-workstation.sh

# Or 100% unattended mode (auto-accepts all defaults)
./setup-workstation.sh -y
```

---

## 📑 Complete Package Overview by Function

| Category | Recommended Packages (Arch Linux) | Purpose & Feature Tied to It |
|---|---|---|
| **Kernel Resilience** | `kernel-modules-hook` | **Prevents broken USB drives / modules** when `pacman` updates the kernel before reboot. |
| **Compiler & Build System** | `base-devel`, `cmake`, `ninja`, `pkgconf` | C++20 compilation & build orchestration for ZenithShell. |
| **GUI & Layer Shell** | `gtk3`, `gtk-layer-shell`, `glib2`, `cairo`, `pango`, `nlohmann-json` | Native Wayland floating UI, layer shell anchoring, vector graphics, JSON config. |
| **Compositor & Wayland Portals** | `hyprland`, `xdg-desktop-portal-hyprland`, `xdg-desktop-portal-gtk`, `qt5-wayland`, `qt6-wayland` | Core Wayland compositor, screen sharing, file pickers, and Qt/GTK integration. |
| **Security & Authentication** | `hyprpolkitagent` | Native Wayland PolicyKit agent for privilege escalation modals (`pkexec` / system). |
| **Storage & USB Flash Drives** | `udisks2`, `dosfstools`, `ntfs-3g`, `exfatprogs`, `gvfs`, `gvfs-mtp` | Auto-detects & mounts FAT32, NTFS, and exFAT pendrives, phones, and external disks in file managers. |
| **Audio Infrastructure** | `pipewire`, `wireplumber`, `pipewire-audio`, `pipewire-alsa`, `pipewire-pulse`, `pipewire-jack`, `pavucontrol`, `playerctl` | PipeWire low-latency audio, real-time volume routing, GUI mixer (`SUPER + P`), hardware media keys. |
| **Network & Bluetooth** | `networkmanager` (`nmcli`), `network-manager-applet` (`nm-connection-editor`), `bluez`, `bluez-utils`, `blueman` | Control Center background Wi-Fi scanning (`nmcli`), advanced network dialog, and Bluetooth pairing. |
| **Backlight & Power Management** | `brightnessctl`, `hypridle`, `hyprlock`, `hyprshade` | Hardware backlight slider, Wayland lock screen (`SUPER + L`), idle sleep, and night light shaders. |
| **Wallpaper & Theming** | Native C++ Layer-Shell, `nwg-look`, `bibata-cursor-theme`, `papirus-icon-theme` | Inbuilt 60fps Wayland wallpaper engine (zero `awww`/`swww` dependency!) & native dynamic palette extractor (zero Python/pywal dependency!). |
| **Clipboard History** | Inbuilt C++ Engine, `wl-clipboard` | `SUPER + V` native Zenith clipboard overlay with search, per-clip deletion, and reboot JSON persistence (zero `cliphist`/`wl-clip-persist` dependency!). |
| **Screenshots & Capture** | `grim`, `slurp`, `swappy`, `libnotify`, `jq` | `Print` (window), `SHIFT + Print` (interactive area), instant editor, and desktop notifications. |
| **Color Picker & Recording** | `hyprpicker`, `wf-recorder` | `SUPER + SHIFT + C` pixel color sampler, `SUPER + SHIFT + R` instant Wayland screen recorder. |
| **OCR Text Grab** | `tesseract`, `tesseract-data-eng` | `SUPER + SHIFT + T` select any screen text and copy words directly to clipboard. |
| **Terminals & Shell** | `foot`, `kitty`, `fish`, `starship` | Ultra-fast Foot (`SUPER + RETURN`), GPU Kitty (`SUPER + SHIFT + RETURN`), Fish shell, and Starship prompt. |
| **Modern CLI & File Managers** | `cosmic-files`, `yazi`, `btop`, `fd`, `ripgrep`, `fzf`, `zoxide`, `eza`, `bat`, `zip`, `unzip`, `p7zip` | Cosmic GUI file manager (`SUPER + E`), Yazi TUI (`SUPER + SHIFT + E`), BTOP monitor (`SUPER + ESC`). |
| **Yazi Previewers** | `imagemagick`, `ffmpegthumbnailer`, `poppler`, `chafa` | High-resolution terminal image, video, and PDF previews in Yazi. |
| **Office & Metric Fonts** | `libreoffice-fresh`, `ttf-jetbrains-mono-nerd`, `noto-fonts`, `noto-fonts-cjk`, `noto-fonts-emoji`, `ttf-carlito`, `ttf-caladea`, `ttf-liberation` | 100% Microsoft Excel/Word/PowerPoint formatting compatibility (Calibri/Cambria replacement) + Nerd Font symbols. |

---

## 🐧 Arch Linux: Fresh Install Master Commands

If you just completed a base Arch Linux installation, run these two commands to install all packages and enable essential services:

### Step 1: Install All Essential Packages via Pacman

```bash
sudo pacman -S --needed \
    base-devel cmake ninja pkgconf git \
    linux-headers \
    kernel-modules-hook \
    hyprland xdg-desktop-portal-hyprland xdg-desktop-portal-gtk qt5-wayland qt6-wayland \
    gtk3 gtk-layer-shell cairo pango glib2 nlohmann-json \
    hyprpolkitagent \
    udisks2 dosfstools ntfs-3g exfatprogs gvfs gvfs-mtp \
    pipewire wireplumber pipewire-audio pipewire-alsa pipewire-pulse pipewire-jack pavucontrol playerctl \
    networkmanager network-manager-applet bluez bluez-utils blueman \
    brightnessctl hypridle hyprlock hyprshade \
    nwg-look bibata-cursor-theme papirus-icon-theme \
    wl-clipboard \
    grim slurp swappy hyprpicker wf-recorder tesseract tesseract-data-eng libnotify jq \
    foot kitty fish starship \
    yazi btop fd ripgrep fzf zoxide eza bat zip unzip p7zip \
    imagemagick ffmpegthumbnailer poppler chafa \
    ttf-jetbrains-mono-nerd noto-fonts noto-fonts-cjk noto-fonts-emoji \
    ttf-carlito ttf-caladea ttf-liberation \
    libreoffice-fresh
```

*(Note: If desired, install `cosmic-files` GUI file manager from the AUR via `yay -S cosmic-files`)*.

---

### Step 2: Enable Core System Services

```bash
# System services
sudo systemctl enable --now NetworkManager
sudo systemctl enable --now bluetooth
sudo systemctl enable --now linux-modules-cleanup.service
sudo systemctl enable --now udisks2.service

# User services (Polkit & Audio)
systemctl --user enable --now hyprpolkitagent.service
systemctl --user enable --now pipewire.service wireplumber.service
```

---

## 🎩 Fedora / RHEL Install Command

```bash
# Build toolchain & development headers
sudo dnf groupinstall -y "Development Tools"
sudo dnf install -y \
    gcc-c++ cmake ninja-build pkgconf-pkg-config \
    gtk3-devel gtk-layer-shell-devel cairo-devel pango-devel glib2-devel json-devel \
    hyprland xdg-desktop-portal-hyprland xdg-desktop-portal-gtk \
    hyprpolkitagent \
    udisks2 dosfstools ntfs-3g exfatprogs gvfs gvfs-mtp \
    pipewire wireplumber pavucontrol playerctl \
    NetworkManager network-manager-applet bluez blueman \
    brightnessctl hypridle hyprlock \
    wl-clipboard \
    grim slurp wf-recorder tesseract \
    foot kitty fish btop ripgrep fzf zoxide eza bat p7zip \
    ffmpegthumbnailer ImageMagick chafa \
    google-noto-sans-fonts fira-code-fonts \
    liberation-fonts google-carlito-fonts google-caladea-fonts \
    libreoffice
```

---

## 📦 Ubuntu / Debian / Pop!_OS Install Command

```bash
# Build toolchain & development headers
sudo apt update
sudo apt install -y \
    build-essential cmake ninja-build pkg-config \
    libgtk-3-dev libgtk-layer-shell-dev libcairo2-dev libpango1.0-dev libglib2.0-dev nlohmann-json3-dev \
    hyprland xdg-desktop-portal-hyprland xdg-desktop-portal-gtk \
    udisks2 dosfstools ntfs-3g exfatprogs gvfs gvfs-backends \
    pipewire wireplumber pavucontrol playerctl \
    network-manager network-manager-gnome bluez blueman \
    brightnessctl hypridle hyprlock \
    wl-clipboard \
    grim slurp wf-recorder tesseract-ocr \
    foot kitty fish btop ripgrep fzf zoxide eza bat p7zip-full \
    ffmpegthumbnailer imagemagick chafa \
    fonts-noto fonts-noto-cjk fonts-noto-color-emoji \
    fonts-liberation fonts-carlito fonts-caladea \
    libreoffice
```

> [!NOTE]
> **Debian Users**: See the dedicated [**`DEBIAN_GUIDE.md`**](DEBIAN_GUIDE.md) for full details on running Hyprland on Debian 12 & 13, PEP 668 Python workarounds, automated installation (`./install-debian.sh`), and package mappings.

---

## 🔍 Critical Pain Points Solved

### 1. 🛡️ Kernel Updates Breaking USB Flash Drives (`kernel-modules-hook`)
* **The Issue:** On Arch Linux, when `pacman -Syu` updates the kernel (e.g. `6.18.49-1-lts` to `6.18.49-3-lts`), it immediately deletes the old modules directory. If you plug in a USB pendrive, external drive, or new hardware before rebooting, the kernel fails to load `usb-storage` or filesystem drivers, leaving `/dev/sdb` uncreated.
* **The Solution:** `kernel-modules-hook` preserves the running kernel's modules in `/usr/lib/modules/` until the next reboot, ensuring USB drives and new modules load flawlessly at all times.

### 2. 🔌 USB Flash Drive Auto-Mounting (`udisks2`, `dosfstools`, `ntfs-3g`, `exfatprogs`)
* Provides full read/write support for Windows FAT32, NTFS, and modern exFAT USB flash drives and SD cards directly inside **Cosmic Files** and **Yazi**.

### 3. 📶 Background Wi-Fi & Advanced Network Dialog
* ZenithShell's Control Center Wi-Fi drawer communicates with `nmcli` in the background for zero-RAM wireless scanning.
* When advanced setup is required (static IP, enterprise 802.1X, VPNs, hidden SSIDs), `nm-connection-editor` (from `network-manager-applet`) is launched on demand.

### 4. 📊 100% Microsoft Excel & Word Document Precision (`setup-libreoffice-excel.sh`)
* Metric-compatible fonts (`Carlito` for Calibri, `Caladea` for Cambria, `Liberation` for Arial/Times) ensure that spreadsheets, tables, and documents opened in LibreOffice maintain exact column widths, margins, and formatting identical to Microsoft Office 365 on Windows.
* Automated optimizer script located at:
  ```bash
  ~/.config/hypr/scripts/apps/setup-libreoffice-excel.sh
  ```

---

## 🚀 Verification Command

To verify that your C++ build dependencies are satisfied:

```bash
pkg-config --exists gtk+-3.0 gtk-layer-shell-0 glib-2.0 cairo pango && echo "✅ All C++ build dependencies are satisfied!" || echo "❌ Missing some dependencies"
```
