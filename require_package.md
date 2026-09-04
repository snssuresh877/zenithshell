# 📦 ZenithShell: Required Packages & Dependencies

This document provides the complete list of system packages, development headers, runtime utilities, and recommended fonts required to build and run **ZenithShell** across major Linux distributions.

---

## 📑 Quick Package Overview

| Category | Primary Packages | Purpose |
|---|---|---|
| **Compiler & Build System** | `gcc` / `g++` (≥ 11), `cmake` (≥ 3.20), `ninja`, `pkg-config` | C++20 compilation & build orchestration |
| **GUI & Layer Shell** | `gtk3`, `gtk-layer-shell`, `glib2`, `cairo`, `pango` | Native Wayland floating UI & layer shell rendering |
| **JSON Parser** | `nlohmann-json` | Parsing `config.json` |
| **Audio Infrastructure** | `pipewire`, `wireplumber` (`wpctl`) | PipeWire volume & audio routing |
| **Network & Bluetooth** | `networkmanager` (`nmcli`), `bluez`, `bluez-utils` (`bluetoothctl`) | Wi-Fi scanning & Bluetooth control |
| **Hardware Controls** | `brightnessctl` | Display backlight range control |
| **Wallpaper & Theming** | `awww` (or `swww`), `python-pywal` | Animated wallpaper transitions & dynamic color extraction |
| **Clipboard & Extras** | `cliphist`, `wl-clipboard`, `hyprshade` | Clipboard search/paste & blue light filter |
| **Security & Authentication** | `hyprpolkitagent` | Native Wayland PolicyKit authorization modal agent |
| **Pro Workstation Tools** | `yazi`, `cosmic-files`, `fd`, `ripgrep`, `jq`, `fzf`, `zoxide`, `eza`, `bat`, `zip`, `unzip`, `p7zip` | High-end terminal, file management, and instant directory navigation |

---

## ⚡ Automated 1-Click Setup (Single Confirmation)

You can install and configure the entire workstation with a single confirmation:

```bash
# Automated 1-Click Turn-Key Setup
./setup-workstation.sh

# Or unattended mode (accepts all recommended defaults automatically)
./setup-workstation.sh -y
```

---

## 🐧 Manual One-Line Install Commands by Distribution

### 1. Arch Linux / Manjaro / EndeavourOS / CachyOS

```bash
# Build toolchain and development headers
sudo pacman -S --needed \
    base-devel \
    cmake \
    ninja \
    pkgconf \
    gtk3 \
    gtk-layer-shell \
    cairo \
    pango \
    glib2 \
    nlohmann-json

# Runtime utilities, audio, network, theming, polkit, and workstation tools
sudo pacman -S --needed \
    pipewire \
    wireplumber \
    networkmanager \
    bluez \
    bluez-utils \
    brightnessctl \
    python-pywal \
    cliphist \
    wl-clipboard \
    hyprpolkitagent \
    hyprshade \
    yazi \
    btop \
    fd \
    ripgrep \
    jq \
    fzf \
    zoxide \
    eza \
    bat \
    zip \
    unzip \
    p7zip \
    imagemagick \
    ffmpegthumbnailer \
    poppler \
    chafa \
    ttf-jetbrains-mono-nerd \
    papirus-icon-theme
```

---

### 2. Fedora / RHEL

```bash
# Build toolchain and development headers
sudo dnf groupinstall -y "Development Tools"
sudo dnf install -y \
    gcc-c++ \
    cmake \
    ninja-build \
    pkgconf-pkg-config \
    gtk3-devel \
    gtk-layer-shell-devel \
    cairo-devel \
    pango-devel \
    glib2-devel \
    json-devel

# Runtime utilities & tools
sudo dnf install -y \
    pipewire \
    wireplumber \
    NetworkManager \
    bluez \
    brightnessctl \
    python3-pywal \
    wl-clipboard \
    google-noto-sans-fonts \
    fira-code-fonts
```

---

### 3. Ubuntu / Debian / Pop!_OS (22.04 / 24.04+)

```bash
# Build toolchain and development headers
sudo apt update
sudo apt install -y \
    build-essential \
    cmake \
    ninja-build \
    pkg-config \
    libgtk-3-dev \
    libgtk-layer-shell-dev \
    libcairo2-dev \
    libpango1.0-dev \
    libglib2.0-dev \
    nlohmann-json3-dev

# Runtime utilities & tools
sudo apt install -y \
    pipewire \
    wireplumber \
    network-manager \
    bluez \
    brightnessctl \
    python3-pip \
    wl-clipboard \
    fonts-noto

# Install pywal via pipx / pip
pip3 install --user pywal
```

---

## 🔍 Detailed Package Descriptions

### 🛠️ Build-Time Dependencies

1. **`gtk3` (`gtk+-3.0`)**:
   - Provides core GTK3 widgets, CSS styling engine, event handling, and window containers.
2. **`gtk-layer-shell` (`gtk-layer-shell-0`)**:
   - Wayland `wlr-layer-shell` protocol client for GTK. Enables anchoring to screen edges, exclusive desktop margins (so windows don't overlap the topbar), and transparent overlay backdrops.
3. **`cairo` & `pango`**:
   - Hardware-accelerated 2D vector drawing and advanced typography / font glyph layout.
4. **`nlohmann-json`**:
   - Modern, single-header C++ JSON parser for reading `config.json`.

---

### ⚙️ Runtime Daemons & Tools

1. **`wireplumber` (`wpctl`)**:
   - ZenithShell invokes `wpctl get-volume @DEFAULT_AUDIO_SINK@`, `wpctl set-volume`, and `wpctl status` for real-time sink/source routing and volume levels.
2. **`networkmanager` (`nmcli`)**:
   - Used by the Control Center Wi-Fi drawer to scan networks (`nmcli -t -f SSID,SIGNAL,SECURITY dev wifi list`) and connect to access points.
3. **`brightnessctl`**:
   - Controls backlight hardware (`brightnessctl s <percent>%` / `brightnessctl g`).
4. **`awww` / `swww`**:
   - Wayland animated wallpaper daemon used by `ThemeEngine::set_wallpaper()` for smooth fade/grow transitions.
5. **`python-pywal` (`wal`)**:
   - Extracts 16-color palettes from wallpapers into `~/.cache/wal/colors.json` for **Dynamic (Wallpaper)** mode.
6. **`cliphist` & `wl-clipboard`**:
   - Powers the fast clipboard history manager overlay (`Super+V`).
7. **`hyprshade`**:
   - Toggles blue-light filters / Night Light mode in Hyprland from the Control Center Quick Modes grid.

---

## 🔤 Icon & Font Recommendations

To render all status symbols and category glyphs correctly, install at least one Nerd Font:
- **JetBrains Mono Nerd Font** (`ttf-jetbrains-mono-nerd`) (Recommended)
- **Fira Code Nerd Font** (`ttf-firacode-nerd`)
- **MesloLGS Nerd Font** (`ttf-meslo-nerd-font-powerlevel10k`)

---

## 📊 LibreOffice MS Office & Excel Precision Compatibility

To open, edit, and save Microsoft Excel (`.xlsx`), Word (`.docx`), and PowerPoint (`.pptx`) documents without breaking fonts, column widths, tables, or formatting:

### 1. Essential Metric-Identical & Microsoft Fonts
Install Google's metric-compatible drop-in replacements for standard Microsoft fonts:
- **`Carlito`** (`ttf-carlito` / `google-carlito-fonts` / `fonts-carlito`): 1:1 metric replacement for **Calibri**.
- **`Caladea`** (`ttf-caladea` / `google-caladea-fonts` / `fonts-caladea`): 1:1 metric replacement for **Cambria**.
- **`Liberation`** (`ttf-liberation` / `fonts-liberation`): 1:1 metric replacements for **Arial**, **Times New Roman**, and **Courier New**.
- **`ttf-ms-fonts`** / **`ttf-mscorefonts-installer`**: Original Microsoft TrueType core fonts.

### 2. One-Click Automated Setup
Run the automated optimizer script:
```bash
~/.config/hypr/scripts/apps/setup-libreoffice-excel.sh
```
This script automatically:
1. Sets default save filters to **Excel 2007-365 (`.xlsx`)**, **Word 2007-365 (`.docx`)**, and **PowerPoint (`.pptx`)**.
2. Disables alien format warnings for clean saving.
3. Enables document font embedding and high-DPI Skia hardware rendering.

---

## 🚀 Verification Command

To quickly verify that all required build dependencies are installed and detectable by `pkg-config`:

```bash
pkg-config --exists gtk+-3.0 gtk-layer-shell-0 glib-2.0 cairo pango && echo "✅ All C++ build dependencies are satisfied!" || echo "❌ Missing some dependencies"
```
