#!/usr/bin/env bash
# ==============================================================================
# 🌌 ZenithShell: Automated Installer & Setup Script
# Ultra-Low RAM C++20 Desktop Shell for Wayland / Hyprland
# ==============================================================================

set -e

# --- Color Formatting ---
BOLD="\033[1m"
GREEN="\033[1;32m"
BLUE="\033[1;34m"
CYAN="\033[1;36m"
YELLOW="\033[1;33m"
RED="\033[1;31m"
RESET="\033[0m"

echo -e "${CYAN}"
echo "    ███████╗███████╗███╗   ██╗██╗████████╗██╗  ██╗"
echo "    ╚══███╔╝██╔════╝████╗  ██║██║╚══██╔══╝██║  ██║"
echo "      ███╔╝ █████╗  ██╔██╗ ██║██║   ██║   ███████║"
echo "     ███╔╝  ██╔══╝  ██║╚██╗██║██║   ██║   ██╔══██║"
echo "    ███████╗███████╗██║ ╚████║██║   ██║   ██║  ██║"
echo "    ╚══════╝╚══════╝╚═╝  ╚═══╝╚═╝   ╚═╝   ╚═╝  ╚═╝"
echo "              S H E L L   I N S T A L L E R        "
echo -e "${RESET}"

# --- 1. Detect Operating System & Package Manager ---
detect_distro() {
    if [ -f /etc/os-release ]; then
        . /etc/os-release
        DISTRO=$ID
        DISTRO_LIKE=$ID_LIKE
    else
        DISTRO="unknown"
    fi
}

detect_distro
echo -e "${BLUE}▶ Detected Linux Distribution:${RESET} ${BOLD}${DISTRO}${RESET}"

# --- 2. Check & Install Dependencies ---
install_dependencies() {
    echo -e "\n${BLUE}▶ Checking system dependencies...${RESET}"

    MISSING_DEPS=0
    if ! pkg-config --exists gtk+-3.0 gtk-layer-shell-0 glib-2.0 cairo pango 2>/dev/null; then
        MISSING_DEPS=1
    fi
    if ! command -v cmake >/dev/null 2>&1 || ! command -v ninja >/dev/null 2>&1; then
        MISSING_DEPS=1
    fi

    if [ "$MISSING_DEPS" -eq 1 ]; then
        echo -e "${YELLOW}⚡ Some required build headers or tools are missing.${RESET}"
        read -rp "Would you like to install required packages now? (Y/n): " confirm_install
        confirm_install=${confirm_install:-Y}

        if [[ "$confirm_install" =~ ^[Yy]$ ]]; then
            case "$DISTRO" in
                arch|manjaro|endeavouros|cachyos)
                    echo -e "${CYAN}Installing dependencies via pacman...${RESET}"
                    sudo pacman -S --needed --noconfirm \
                        base-devel cmake ninja pkgconf \
                        gtk3 gtk-layer-shell cairo pango glib2 nlohmann-json \
                        pipewire wireplumber networkmanager bluez bluez-utils \
                        brightnessctl python-pywal cliphist wl-clipboard \
                        ttf-jetbrains-mono-nerd papirus-icon-theme
                    ;;
                fedora|rhel)
                    echo -e "${CYAN}Installing dependencies via dnf...${RESET}"
                    sudo dnf groupinstall -y "Development Tools"
                    sudo dnf install -y \
                        gcc-c++ cmake ninja-build pkgconf-pkg-config \
                        gtk3-devel gtk-layer-shell-devel cairo-devel pango-devel glib2-devel json-devel \
                        pipewire wireplumber NetworkManager bluez brightnessctl \
                        python3-pywal wl-clipboard
                    ;;
                ubuntu|debian|pop)
                    echo -e "${CYAN}Installing dependencies via apt...${RESET}"
                    sudo apt update
                    sudo apt install -y \
                        build-essential cmake ninja-build pkg-config \
                        libgtk-3-dev libgtk-layer-shell-dev libcairo2-dev libpango1.0-dev libglib2.0-dev nlohmann-json3-dev \
                        pipewire wireplumber network-manager bluez brightnessctl python3-pip wl-clipboard
                    pip3 install --user pywal 2>/dev/null || true
                    ;;
                *)
                    echo -e "${RED}❌ Unknown distribution. Please refer to require_package.md to install dependencies manually.${RESET}"
                    exit 1
                    ;;
            esac
        else
            echo -e "${RED}Cannot proceed without required dependencies.${RESET}"
            exit 1
        fi
    else
        echo -e "${GREEN}✔ All core C++ development headers and build tools are present!${RESET}"
    fi
}

install_dependencies

# --- 3. Compile ZenithShell ---
echo -e "\n${BLUE}▶ Building ZenithShell with CMake & Ninja (Release mode)...${RESET}"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
ninja -C build

echo -e "${GREEN}✔ Compilation successful!${RESET}"

# --- 4. Install Binary & User Configuration ---
echo -e "\n${BLUE}▶ Installing ZenithShell binary and configurations...${RESET}"

INSTALL_DIR="$HOME/.local/bin"
CONFIG_DIR="$HOME/.config/zenithshell"

mkdir -p "$INSTALL_DIR"
mkdir -p "$CONFIG_DIR"
mkdir -p "$CONFIG_DIR/themes"
mkdir -p "$CONFIG_DIR/wallpapers"

# Install binary
cp -f build/zenithshell "$INSTALL_DIR/zenithshell"
chmod +x "$INSTALL_DIR/zenithshell"
echo -e "${GREEN}✔ Installed binary to:${RESET} $INSTALL_DIR/zenithshell"

# Install config.json and style.css (preserves user customizations with backup)
if [ -f "$CONFIG_DIR/config.json" ]; then
    cp "$CONFIG_DIR/config.json" "$CONFIG_DIR/config.json.bak"
    echo -e "${YELLOW}ℹ Backed up existing config.json to config.json.bak${RESET}"
fi
cp config.json "$CONFIG_DIR/config.json"

if [ -f "$CONFIG_DIR/style.css" ]; then
    cp "$CONFIG_DIR/style.css" "$CONFIG_DIR/style.css.bak"
    echo -e "${YELLOW}ℹ Backed up existing style.css to style.css.bak${RESET}"
fi
cp style.css "$CONFIG_DIR/style.css"

# Install wallpaper pack
if [ -d "wallpapers" ]; then
    echo -e "${CYAN}Installing curated wallpaper pack...${RESET}"
    mkdir -p "$CONFIG_DIR/wallpapers"
    cp -rn wallpapers/* "$CONFIG_DIR/wallpapers/" 2>/dev/null || cp -r wallpapers/* "$CONFIG_DIR/wallpapers/" 2>/dev/null || true
    echo -e "${GREEN}✔ Installed wallpapers to:${RESET} $CONFIG_DIR/wallpapers"
fi

echo -e "${GREEN}✔ User configuration and style assets installed to:${RESET} $CONFIG_DIR"

# --- 5. PATH Verification ---
if [[ ":$PATH:" != *":$HOME/.local/bin:"* ]]; then
    echo -e "\n${YELLOW}⚠ Warning: ~/.local/bin is not in your current PATH.${RESET}"
    echo -e "Add the following line to your ~/.bashrc or ~/.zshrc:"
    echo -e "    ${BOLD}export PATH=\"\$HOME/.local/bin:\$PATH\"${RESET}"
fi

# --- 6. Hyprland & Autostart Integration ---
if [ ! -d "$HOME/.config/hypr" ]; then
    if [ -d "hyprland" ]; then
        read -rp "No ~/.config/hypr found. Install full Zenith Hyprland Lua configuration? (Y/n): " install_hypr_dots
        install_hypr_dots=${install_hypr_dots:-Y}
        if [[ "$install_hypr_dots" =~ ^[Yy]$ ]]; then
            mkdir -p "$HOME/.config/hypr"
            cp -r hyprland/* "$HOME/.config/hypr/"
            echo -e "${GREEN}✔ Installed full Zenith Hyprland Lua configuration to ~/.config/hypr${RESET}"
        fi
    fi
fi

if [ -d "$HOME/.config/hypr" ]; then
    echo -e "\n${BLUE}▶ Setting up Hyprland integration & autostart...${RESET}"
    echo "zenithshell" > "$HOME/.config/hypr/ui_mode" 2>/dev/null || true

    # 1. Standard hyprland.conf setup
    if [ -f "$HOME/.config/hypr/hyprland.conf" ]; then
        if ! grep -q "zenithshell" "$HOME/.config/hypr/hyprland.conf"; then
            echo -e "\n# Auto-start ZenithShell\nexec-once = $INSTALL_DIR/zenithshell" >> "$HOME/.config/hypr/hyprland.conf"
            echo -e "${GREEN}✔ Added 'exec-once = zenithshell' to ~/.config/hypr/hyprland.conf${RESET}"
        fi
    fi

    # 2. Modular Lua setup (if using hyprland.lua modular structure)
    if [ -f "$HOME/.config/hypr/modules/autostart.lua" ]; then
        mkdir -p "$HOME/.config/hypr/scripts/ui"
        cat << 'AUTOSCRIPT' > "$HOME/.config/hypr/scripts/ui/autostart_ui.sh"
#!/usr/bin/env bash
set -euo pipefail
STATE_FILE="$HOME/.config/hypr/ui_mode"
MODE="zenithshell"
if [ -f "$STATE_FILE" ]; then
    MODE=$(cat "$STATE_FILE" | tr -d '[:space:]')
fi
if [ "$MODE" = "waybar" ]; then
    pgrep -f swaync >/dev/null 2>&1 || swaync >/dev/null 2>&1 &
    pgrep -f waybar >/dev/null 2>&1 || waybar >/dev/null 2>&1 &
else
    if ! pgrep -f zenithshell >/dev/null 2>&1; then
        if [ -x "$HOME/.local/bin/zenithshell" ]; then
            setsid "$HOME/.local/bin/zenithshell" >/tmp/zenithshell.log 2>&1 &
        fi
    fi
fi
AUTOSCRIPT
        chmod +x "$HOME/.config/hypr/scripts/ui/autostart_ui.sh"
        echo -e "${GREEN}✔ Configured autostart launcher script in ~/.config/hypr/scripts/ui/${RESET}"
    fi
fi

# Optional systemd user service setup
mkdir -p "$HOME/.config/systemd/user"
cat << SERVICE_EOF > "$HOME/.config/systemd/user/zenithshell.service"
[Unit]
Description=ZenithShell Native Desktop Shell
PartOf=graphical-session.target
After=graphical-session.target

[Service]
Type=simple
ExecStart=%h/.local/bin/zenithshell
Restart=on-failure
RestartSec=2s

[Install]
WantedBy=graphical-session.target
SERVICE_EOF
echo -e "${GREEN}✔ Created systemd user service at ~/.config/systemd/user/zenithshell.service${RESET}"

# --- 7. Completion Summary ---
echo -e "\n${GREEN}================================================================${RESET}"
echo -e "${GREEN}🎉 ZenithShell has been successfully installed!${RESET}"
echo -e "${GREEN}================================================================${RESET}"
echo -e "\n${BOLD}Quick Start Guide:${RESET}"
echo -e "  • Start ZenithShell:          ${CYAN}zenithshell &${RESET}"
echo -e "  • Toggle Control Center:       ${CYAN}gdbus call --session --dest dev.zenith.Shell --object-path /dev/zenith/Shell --method dev.zenith.Shell.ToggleControlCenter${RESET}"
echo -e "  • Toggle Spotlight Launcher:  ${CYAN}gdbus call --session --dest dev.zenith.Shell --object-path /dev/zenith/Shell --method dev.zenith.Shell.ToggleSpotlight${RESET}"
echo -e "  • Cycle Wallpaper:            ${CYAN}gdbus call --session --dest dev.zenith.Shell --object-path /dev/zenith/Shell --method dev.zenith.Shell.NextWallpaper${RESET}"
echo -e "  • Cycle Theme:                ${CYAN}gdbus call --session --dest dev.zenith.Shell --object-path /dev/zenith/Shell --method dev.zenith.Shell.NextTheme${RESET}"
echo -e "\nConfiguration: ${BOLD}~/.config/zenithshell/config.json${RESET}"
echo -e "Stylesheet:    ${BOLD}~/.config/zenithshell/style.css${RESET}"
echo ""
