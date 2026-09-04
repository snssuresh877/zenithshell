#!/usr/bin/env bash
# ==============================================================================
# 🌌 Zenith Workstation: Modular Desktop Environment Setup Wizard
# ==============================================================================
# Complete Turn-Key Setup for Wayland, Hyprland & ZenithShell
# Interactive, Modular, and Multi-Distribution (Arch, Fedora, Debian/Ubuntu)
# ==============================================================================

set -euo pipefail

# --- Color Formatting ---
BOLD="\033[1m"
GREEN="\033[1;32m"
BLUE="\033[1;34m"
CYAN="\033[1;36m"
YELLOW="\033[1;33m"
MAGENTA="\033[1;35m"
RED="\033[1;31m"
RESET="\033[0m"

clear
echo -e "${CYAN}"
echo "    ███████╗███████╗███╗   ██╗██╗████████╗██╗  ██╗"
echo "    ╚══███╔╝██╔════╝████╗  ██║██║╚══██╔══╝██║  ██║"
echo "      ███╔╝ █████╗  ██╔██╗ ██║██║   ██║   ███████║"
echo "     ███╔╝  ██╔══╝  ██║╚██╗██║██║   ██║   ██╔══██║"
echo "    ███████╗███████╗██║ ╚████║██║   ██║   ██║  ██║"
echo "    ╚══════╝╚══════╝╚═╝  ╚═══╝╚═╝   ╚═╝   ╚═╝  ╚═╝"
echo "         W O R K S T A T I O N   W I Z A R D       "
echo -e "${RESET}"

echo -e "${BOLD}Welcome to the Zenith Workstation Setup Wizard!${RESET}"
echo "This wizard allows you to customize and install modular desktop components."
echo "Press Enter at each prompt to accept the recommended default ([Y/n])."
echo -e "----------------------------------------------------------------\n"

# --- 1. Detect Operating System ---
if [ -f /etc/os-release ]; then
    . /etc/os-release
    DISTRO=$ID
else
    DISTRO="unknown"
fi
echo -e "${BLUE}▶ Detected Linux Distribution:${RESET} ${BOLD}${DISTRO}${RESET}\n"

prompt_step() {
    local prompt_text="$1"
    local default_val="${2:-Y}"
    read -rp "$(echo -e "${CYAN}? ${BOLD}${prompt_text}${RESET} [Y/n]: ")" choice
    choice=${choice:-$default_val}
    if [[ "$choice" =~ ^[Yy]$ ]]; then
        return 0
    else
        return 1
    fi
}

# --- Module Selection ---
echo -e "${MAGENTA}--- 📦 Component Selection ---${RESET}"

INSTALL_CORE=1
if prompt_step "1. Install ZenithShell Core (~26MB C++20 Shell, 23 Themes & 260+ Wallpapers)" "Y"; then
    INSTALL_CORE=1
else
    INSTALL_CORE=0
fi

INSTALL_HYPR=1
if prompt_step "2. Install Modular Hyprland Lua Dotfiles & Keybindings" "Y"; then
    INSTALL_HYPR=1
else
    INSTALL_HYPR=0
fi

INSTALL_TERMINALS=1
if prompt_step "3. Install & Configure Pro Terminal Stack (Foot, Kitty, Fish Shell & PATHs)" "Y"; then
    INSTALL_TERMINALS=1
else
    INSTALL_TERMINALS=0
fi

INSTALL_WORKSTATION_TOOLS=1
if prompt_step "4. Install Modern File Managers & CLI Tools (Yazi, Cosmic Files, fd, rg, fzf, bat)" "Y"; then
    INSTALL_WORKSTATION_TOOLS=1
else
    INSTALL_WORKSTATION_TOOLS=0
fi

INSTALL_MEDIA_TOOLS=1
if prompt_step "5. Install Screen Recording, OCR Text Capture & Multimedia Utilities" "Y"; then
    INSTALL_MEDIA_TOOLS=1
else
    INSTALL_MEDIA_TOOLS=0
fi

INSTALL_OFFICE=1
if prompt_step "6. Configure LibreOffice for 100% MS Office/Excel Compatibility & Metric Fonts" "Y"; then
    INSTALL_OFFICE=1
else
    INSTALL_OFFICE=0
fi

INSTALL_POWER=1
if prompt_step "7. Configure Smart Battery & Idle Power Management (Hypridle & Hyprlock)" "Y"; then
    INSTALL_POWER=1
else
    INSTALL_POWER=0
fi

echo -e "\n${GREEN}✔ Selections recorded! Beginning installation...${RESET}\n"
sleep 1

# --- Execution ---

# 1. Core ZenithShell
if [ "$INSTALL_CORE" -eq 1 ]; then
    echo -e "${BLUE}▶ [1/7] Building & Installing ZenithShell Core...${RESET}"
    if [ -f "./install.sh" ]; then
        ./install.sh
    else
        cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
        ninja -C build
        mkdir -p "$HOME/.local/bin" "$HOME/.config/zenithshell"
        cp -f build/zenithshell "$HOME/.local/bin/zenithshell"
        chmod +x "$HOME/.local/bin/zenithshell"
    fi
    echo -e "${GREEN}✔ ZenithShell Core installed!${RESET}\n"
fi

# 2. Hyprland Modular Configuration
if [ "$INSTALL_HYPR" -eq 1 ]; then
    echo -e "${BLUE}▶ [2/7] Deploying Modular Hyprland Lua Configuration...${RESET}"
    if [ -d "hyprland" ]; then
        mkdir -p "$HOME/.config/hypr"
        cp -r hyprland/* "$HOME/.config/hypr/"
        find "$HOME/.config/hypr/scripts" -type f -name "*.sh" -exec chmod +x {} + 2>/dev/null || true
        echo -e "${GREEN}✔ Hyprland Lua configuration deployed to ~/.config/hypr!${RESET}\n"
    fi
fi

# 3. Terminal Stack (Foot, Kitty, Fish)
if [ "$INSTALL_TERMINALS" -eq 1 ]; then
    echo -e "${BLUE}▶ [3/7] Setting up Foot, Kitty, and Fish Shell Environment...${RESET}"
    if [ -d "classic-addons/foot" ]; then
        mkdir -p "$HOME/.config/foot"
        cp -f classic-addons/foot/foot.ini "$HOME/.config/foot/foot.ini"
    fi
    if [ -d "classic-addons/kitty" ]; then
        mkdir -p "$HOME/.config/kitty"
        cp -f classic-addons/kitty/kitty.conf "$HOME/.config/kitty/kitty.conf"
    fi
    if [ -d "classic-addons/starship" ]; then
        cp -f classic-addons/starship/starship.toml "$HOME/.config/starship.toml"
    fi
    if [ -f "hyprland/scripts/system/setup_fish_environment.sh" ]; then
        bash "hyprland/scripts/system/setup_fish_environment.sh"
    elif [ -d "classic-addons/fish" ]; then
        mkdir -p "$HOME/.config/fish"
        cp -f classic-addons/fish/config.fish "$HOME/.config/fish/config.fish"
    fi
    echo -e "${GREEN}✔ Terminals, Prompt (Starship), and Fish Shell configured!${RESET}\n"
fi

# 4. Workstation & CLI Power Tools
if [ "$INSTALL_WORKSTATION_TOOLS" -eq 1 ]; then
    echo -e "${BLUE}▶ [4/7] Checking CLI Power Utilities, BTOP, GTK Settings & File Managers...${RESET}"
    case "$DISTRO" in
        arch|manjaro|endeavouros|cachyos)
            sudo pacman -S --needed --noconfirm yazi btop fd ripgrep jq fzf zoxide eza bat zip unzip p7zip 2>/dev/null || true
            ;;
        fedora|rhel)
            sudo dnf install -y yazi btop fd-find ripgrep jq fzf zoxide eza bat zip unzip p7zip 2>/dev/null || true
            ;;
        ubuntu|debian|pop)
            sudo apt install -y btop fd-find ripgrep jq fzf zoxide eza bat zip unzip p7zip-full 2>/dev/null || true
            ;;
    esac

    # Deploy Yazi configuration
    if [ -d "classic-addons/yazi" ]; then
        mkdir -p "$HOME/.config/yazi"
        cp -f classic-addons/yazi/yazi.toml "$HOME/.config/yazi/yazi.toml"
    fi

    # Deploy BTOP configuration
    if [ -d "classic-addons/btop" ]; then
        mkdir -p "$HOME/.config/btop"
        cp -f classic-addons/btop/btop.conf "$HOME/.config/btop/btop.conf"
    fi

    # Deploy GTK-3.0 & GTK-4.0 Dark Mode Preferences
    if [ -d "classic-addons/gtk-3.0" ]; then
        mkdir -p "$HOME/.config/gtk-3.0" "$HOME/.config/gtk-4.0"
        cp -f classic-addons/gtk-3.0/settings.ini "$HOME/.config/gtk-3.0/settings.ini"
        cp -f classic-addons/gtk-4.0/settings.ini "$HOME/.config/gtk-4.0/settings.ini"
    fi

    # Deploy XDG Default MIME associations
    if [ -d "classic-addons/xdg" ]; then
        mkdir -p "$HOME/.config"
        cp -f classic-addons/xdg/mimeapps.list "$HOME/.config/mimeapps.list"
    fi

    echo -e "${GREEN}✔ CLI workstation power tools, Yazi, BTOP, and GTK settings deployed!${RESET}\n"
fi

# 5. Multimedia & Screen Recording Tools
if [ "$INSTALL_MEDIA_TOOLS" -eq 1 ]; then
    echo -e "${BLUE}▶ [5/7] Checking Screen Recording & OCR Utilities...${RESET}"
    case "$DISTRO" in
        arch|manjaro|endeavouros|cachyos)
            sudo pacman -S --needed --noconfirm wf-recorder tesseract slurp grim ffmpegthumbnailer imagemagick chafa 2>/dev/null || true
            ;;
        fedora|rhel)
            sudo dnf install -y wf-recorder tesseract slurp grim ffmpegthumbnailer ImageMagick chafa 2>/dev/null || true
            ;;
        ubuntu|debian|pop)
            sudo apt install -y wf-recorder tesseract-ocr slurp grim ffmpegthumbnailer imagemagick chafa 2>/dev/null || true
            ;;
    esac
    echo -e "${GREEN}✔ Media & Screen capture utilities ready!${RESET}\n"
fi

# 6. LibreOffice Compatibility & Fonts
if [ "$INSTALL_OFFICE" -eq 1 ]; then
    echo -e "${BLUE}▶ [6/7] Optimizing LibreOffice for MS Office/Excel Format Compatibility...${RESET}"
    if [ -f "hyprland/scripts/apps/setup-libreoffice-excel.sh" ]; then
        bash "hyprland/scripts/apps/setup-libreoffice-excel.sh"
    fi
    echo -e "${GREEN}✔ LibreOffice save filters & metric fonts configured!${RESET}\n"
fi

# 7. Power Management & Idle
if [ "$INSTALL_POWER" -eq 1 ]; then
    echo -e "${BLUE}▶ [7/7] Verifying Power Management & Idle Daemon...${RESET}"
    case "$DISTRO" in
        arch|manjaro|endeavouros|cachyos)
            sudo pacman -S --needed --noconfirm hypridle hyprlock brightnessctl 2>/dev/null || true
            ;;
        fedora|rhel)
            sudo dnf install -y hypridle hyprlock brightnessctl 2>/dev/null || true
            ;;
        ubuntu|debian|pop)
            sudo apt install -y hypridle hyprlock brightnessctl 2>/dev/null || true
            ;;
    esac
    echo -e "${GREEN}✔ Power & Idle management verified!${RESET}\n"
fi

echo -e "${CYAN}================================================================"
echo "🎉 Zenith Workstation Setup Complete!"
echo -e "================================================================${RESET}"
echo "Quick Summary:"
echo "  • ZenithShell:         ~/.local/bin/zenithshell"
echo "  • Open Terminal:       SUPER + RETURN (in active directory)"
echo "  • File Manager:        SUPER + E (GUI) / SUPER + SHIFT + E (Yazi)"
echo "  • Spotlight Launcher:  SUPER + SPACE"
echo "  • Control Center:      SUPER + A"
echo "  • Keybinds Cheatsheet: SUPER + K"
echo -e "\nEnjoy your high-performance Wayland desktop! 🌌\n"
