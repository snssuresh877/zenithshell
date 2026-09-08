#!/usr/bin/env bash
# ==============================================================================
# 🌌 ZenithShell: Dedicated Debian / Ubuntu Installer & Setup Script
# Ultra-Low RAM C++20 Desktop Shell for Wayland / Hyprland
# Supports: Debian 12 (Bookworm), Debian 13 (Trixie), Debian Sid, Ubuntu / Pop!_OS
# ==============================================================================

set -e

# --- Color Formatting ---
BOLD="\033[1m"
GREEN="\033[1;32m"
BLUE="\033[1;34m"
CYAN="\033[1;36m"
YELLOW="\033[1;33m"
RED="\033[1;31m"
MAGENTA="\033[1;35m"
RESET="\033[0m"

echo -e "${CYAN}"
echo "    ███████╗███████╗███╗   ██╗██╗████████╗██╗  ██╗"
echo "    ╚══███╔╝██╔════╝████╗  ██║██║╚══██╔══╝██║  ██║"
echo "      ███╔╝ █████╗  ██╔██╗ ██║██║   ██║   ███████║"
echo "     ███╔╝  ██╔══╝  ██║╚██╗██║██║   ██║   ██╔══██║"
echo "    ███████╗███████╗██║ ╚████║██║   ██║   ██║  ██║"
echo "    ╚══════╝╚══════╝╚═╝  ╚═══╝╚═╝   ╚═╝   ╚═╝  ╚═╝"
echo "         D E B I A N   &   U B U N T U   S E T U P"
echo -e "${RESET}"

# --- 1. Detect Debian Environment ---
if [ -f /etc/os-release ]; then
    . /etc/os-release
    DISTRO_NAME="$NAME"
    DISTRO_VER="$VERSION_ID"
    DISTRO_CODENAME="${VERSION_CODENAME:-sid}"
else
    echo -e "${RED}❌ Unable to identify operating system via /etc/os-release.${RESET}"
    exit 1
fi

echo -e "${BLUE}▶ System Detected:${RESET} ${BOLD}${DISTRO_NAME} (${DISTRO_CODENAME} / ${DISTRO_VER})${RESET}"

# --- 2. Update APT Package Lists ---
echo -e "\n${BLUE}▶ [1/5] Updating APT repositories...${RESET}"
sudo apt update -y

# --- 3. Install Core C++20 Build Toolchain & GTK3 Layer Shell Headers ---
echo -e "\n${BLUE}▶ [2/5] Installing C++20 build toolchain & development libraries...${RESET}"
sudo apt install -y \
    build-essential \
    g++ \
    cmake \
    ninja-build \
    pkg-config \
    libgtk-3-dev \
    libgtk-layer-shell-dev \
    libcairo2-dev \
    libpango1.0-dev \
    libglib2.0-dev \
    nlohmann-json3-dev

# --- 4. Install Runtime Daemons, Audio, Network & Storage Tools ---
echo -e "\n${BLUE}▶ [3/5] Installing runtime utilities, audio, network, and disk daemons...${RESET}"
sudo apt install -y \
    pipewire \
    wireplumber \
    network-manager \
    network-manager-gnome \
    bluez \
    blueman \
    brightnessctl \
    pavucontrol \
    playerctl \
    udisks2 \
    dosfstools \
    ntfs-3g \
    exfatprogs \
    gvfs \
    gvfs-backends \
    wl-clipboard \
    grim \
    slurp \
    wf-recorder \
    tesseract-ocr \
    libnotify-bin \
    jq \
    fonts-noto \
    fonts-noto-cjk \
    fonts-noto-color-emoji \
    fonts-liberation \
    fonts-carlito \
    fonts-caladea \
    papirus-icon-theme

# Auto-enable storage daemon
sudo systemctl enable --now udisks2.service 2>/dev/null || true

# Install optional packages available in newer Debian/Ubuntu (Trixie/Sid/Noble)
echo -e "${CYAN}Checking availability of Wayland idle, lock, and terminal packages in APT...${RESET}"
sudo apt install -y hypridle hyprlock hyprland foot kitty fish btop ripgrep fd-find fzf zoxide 2>/dev/null || true

# --- 5. Dynamic Theming (Native C++ Inbuilt Extractor) ---
echo -e "\n${BLUE}▶ [4/5] Dynamic Palette Extractor is built-in natively in C++ (Zero Python/Pywal dependency)...${RESET}"

# Ensure ~/.local/bin is in PATH
mkdir -p "$HOME/.local/bin"
if [[ ":$PATH:" != *":$HOME/.local/bin:"* ]]; then
    export PATH="$HOME/.local/bin:$PATH"
fi

# --- 6. Install Starship Prompt (if not installed) ---
if ! command -v starship >/dev/null 2>&1; then
    echo -e "${YELLOW}⚡ Starship prompt not found in APT. Installing standalone binary...${RESET}"
    curl -sS https://starship.rs/install.sh | sh -s -- -y 2>/dev/null || true
fi

# --- 7. Compile ZenithShell Native Binary ---
echo -e "\n${BLUE}▶ [5/5] Compiling ZenithShell (Release mode with Ninja)...${RESET}"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
ninja -C build

# --- 8. Install Binary, Config & Themes ---
echo -e "\n${CYAN}Deploying ZenithShell binary, styles, 23 themes, and wallpapers...${RESET}"
mkdir -p "$HOME/.local/bin" "$HOME/.config/zenithshell"

cp -f build/zenithshell "$HOME/.local/bin/zenithshell"
chmod +x "$HOME/.local/bin/zenithshell"

# Deploy initial configuration and styles if not already customized
if [ ! -f "$HOME/.config/zenithshell/config.json" ]; then
    cp -f config.json "$HOME/.config/zenithshell/config.json"
fi
cp -f style.css "$HOME/.config/zenithshell/style.css"

# Deploy themes & wallpapers
if [ -d "assets/themes" ]; then
    mkdir -p "$HOME/.config/zenithshell/themes"
    cp -rf assets/themes/* "$HOME/.config/zenithshell/themes/" 2>/dev/null || true
fi
if [ -d "assets/wallpapers" ]; then
    mkdir -p "$HOME/.config/zenithshell/wallpapers"
    cp -rf assets/wallpapers/* "$HOME/.config/zenithshell/wallpapers/" 2>/dev/null || true
fi

# Install desktop entry
mkdir -p "$HOME/.local/share/applications"
cat << DESKTOP > "$HOME/.local/share/applications/zenithshell.desktop"
[Desktop Entry]
Name=ZenithShell
Comment=High-Performance C++20 Desktop Shell for Wayland
Exec=$HOME/.local/bin/zenithshell
Icon=preferences-desktop-theme
Terminal=false
Type=Application
Categories=Utility;DesktopSettings;
DESKTOP

# Install systemd user service
mkdir -p "$HOME/.config/systemd/user"
cat << SERVICE > "$HOME/.config/systemd/user/zenithshell.service"
[Unit]
Description=ZenithShell - Ultra-Low RAM C++20 Desktop Shell
PartOf=graphical-session.target
After=graphical-session.target

[Service]
Type=simple
ExecStart=$HOME/.local/bin/zenithshell
Restart=on-failure
RestartSec=1s

[Install]
WantedBy=graphical-session.target
SERVICE

echo -e "\n${GREEN}================================================================"
echo "🎉 ZenithShell successfully installed on ${DISTRO_NAME}!"
echo -e "================================================================${RESET}"
echo -e "Binary installed to: ${BOLD}$HOME/.local/bin/zenithshell${RESET}"
echo -e "Config installed to: ${BOLD}$HOME/.config/zenithshell/config.json${RESET}"
echo -e "\nTo test run now:"
echo -e "  ${CYAN}$HOME/.local/bin/zenithshell${RESET}"
echo -e "\nTo enable autostart on Wayland login via systemd:"
echo -e "  ${CYAN}systemctl --user enable --now zenithshell${RESET}\n"
