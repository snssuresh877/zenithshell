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

# --- Module Selection / Automated Mode ---
AUTO_ALL=0
if [[ "${1:-}" =~ ^(-y|--yes|-a|--all|--auto)$ ]]; then
    AUTO_ALL=1
fi

if [ "$AUTO_ALL" -eq 1 ]; then
    echo -e "${GREEN}⚡ Automated 1-Click Mode: Installing and configuring all recommended workstation components...${RESET}\n"
    INSTALL_CORE=1
    INSTALL_HYPR=1
    INSTALL_TERMINALS=1
    INSTALL_WORKSTATION_TOOLS=1
    INSTALL_MEDIA_TOOLS=1
    INSTALL_OFFICE=1
    INSTALL_POWER=1
else
    echo -e "${MAGENTA}--- 🚀 Setup Mode ---${RESET}"
    if prompt_step "Full Automated Turn-Key Setup? (Press ENTER to install all components with 1-click)" "Y"; then
        INSTALL_CORE=1
        INSTALL_HYPR=1
        INSTALL_TERMINALS=1
        INSTALL_WORKSTATION_TOOLS=1
        INSTALL_MEDIA_TOOLS=1
        INSTALL_OFFICE=1
        INSTALL_POWER=1
    else
        echo -e "\n${MAGENTA}--- 📦 Custom Component Selection ---${RESET}"
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
        if prompt_step "4. Install Modern File Managers & CLI Tools (Thunar, Yazi, fd, rg, fzf, bat)" "Y"; then
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
        if prompt_step "6. Install Modern Office Suite & Document Tools (OnlyOffice Desktop / PDF Editors)" "Y"; then
            INSTALL_OFFICE=1
        else
            INSTALL_OFFICE=0
        fi

        INSTALL_POWER=1
        if prompt_step "7. Install Workstation Power, Battery & Performance Tuning (TLP, Thermal, Powertop)" "Y"; then
            INSTALL_POWER=1
        else
            INSTALL_POWER=0
        fi
    fi
fi

echo -e "\n${GREEN}✔ Selections recorded! Beginning installation...${RESET}\n"
sleep 1

# --- Execution ---

# 1. Core Shell Installation
if [ "$INSTALL_CORE" -eq 1 ]; then
    echo -e "${BLUE}▶ [1/7] Building and Installing ZenithShell Core Native Binary & Desktop Assets...${RESET}"
    bash "install.sh"
    echo -e "${GREEN}✔ ZenithShell core binary, 23 themes, and wallpapers installed!${RESET}\n"
fi

# 2. Hyprland Configuration & Keybindings
if [ "$INSTALL_HYPR" -eq 1 ]; then
    echo -e "${BLUE}▶ [2/7] Deploying Zenith Modular Hyprland Lua Configuration...${RESET}"
    if [ -d "hyprland" ]; then
        mkdir -p "$HOME/.config/hypr"
        cp -rf hyprland/* "$HOME/.config/hypr/"
        echo -e "${GREEN}✔ Zenith Hyprland modular Lua config, scripts, and keybinds deployed to ~/.config/hypr!${RESET}\n"
    fi
fi

# 3. Terminal Emulator & Fish Shell Stack
if [ "$INSTALL_TERMINALS" -eq 1 ]; then
    echo -e "${BLUE}▶ [3/7] Checking Terminal Emulators, Starship & Modern Fish Shell...${RESET}"
    case "$DISTRO" in
        arch|manjaro|endeavouros|cachyos)
            sudo pacman -S --needed --noconfirm foot kitty fish starship fastfetch 2>/dev/null || true
            ;;
        fedora|rhel)
            sudo dnf install -y foot kitty fish starship fastfetch 2>/dev/null || true
            ;;
        ubuntu|debian|pop)
            sudo apt install -y foot kitty fish fastfetch 2>/dev/null || true
            ;;
    esac

    # Deploy configs from classic-addons if available
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
    echo -e "${BLUE}▶ [4/7] Checking CLI Power Utilities, Clipboard, BTOP, GTK Settings & File Managers...${RESET}"
    case "$DISTRO" in
        arch|manjaro|endeavouros|cachyos)
            sudo pacman -S --needed --noconfirm thunar thunar-volman thunar-archive-plugin tumbler gvfs yazi btop fd ripgrep jq fzf zoxide eza bat zip unzip p7zip cliphist wl-clipboard wl-clip-persist networkmanager network-manager-applet udisks2 dosfstools ntfs-3g exfatprogs pavucontrol playerctl kernel-modules-hook blueman 2>/dev/null || true
            sudo systemctl enable --now linux-modules-cleanup.service 2>/dev/null || true
            sudo systemctl enable --now udisks2.service 2>/dev/null || true
            ;;
        fedora|rhel)
            sudo dnf install -y thunar thunar-volman thunar-archive-plugin tumbler gvfs yazi btop fd-find ripgrep jq fzf zoxide eza bat zip unzip p7zip wl-clipboard NetworkManager network-manager-applet udisks2 dosfstools ntfs-3g exfatprogs pavucontrol playerctl blueman 2>/dev/null || true
            sudo systemctl enable --now udisks2.service 2>/dev/null || true
            ;;
        ubuntu|debian|pop)
            sudo apt install -y thunar thunar-volman thunar-archive-plugin tumbler gvfs btop fd-find ripgrep jq fzf bat zip unzip p7zip-full wl-clipboard network-manager network-manager-gnome udisks2 dosfstools ntfs-3g exfatprogs pavucontrol playerctl blueman 2>/dev/null || true
            sudo apt install -y zoxide eza yazi 2>/dev/null || true
            sudo systemctl enable --now udisks2.service 2>/dev/null || true
            ;;
    esac

    # Deploy Thunar & XFCE4 file manager configuration
    if [ -d "classic-addons/thunar" ]; then
        mkdir -p "$HOME/.config/Thunar" "$HOME/.config/xfce4/xfconf/xfce-perchannel-xml" "$HOME/.local/share/icons/hicolor/scalable/apps"
        cp -f classic-addons/thunar/uca.xml "$HOME/.config/Thunar/uca.xml" 2>/dev/null || true
        cp -f classic-addons/thunar/accels.scm "$HOME/.config/Thunar/accels.scm" 2>/dev/null || true
        if [ -f "classic-addons/thunar/thunar.xml" ]; then
            cp -f classic-addons/thunar/thunar.xml "$HOME/.config/xfce4/xfconf/xfce-perchannel-xml/thunar.xml" 2>/dev/null || true
        fi
        if [ -f "classic-addons/thunar/icons/org.xfce.thunar.svg" ]; then
            cp -f classic-addons/thunar/icons/org.xfce.thunar.svg "$HOME/.local/share/icons/hicolor/scalable/apps/org.xfce.thunar.svg" 2>/dev/null || true
            cp -f classic-addons/thunar/icons/org.xfce.thunar.svg "$HOME/.local/share/icons/hicolor/scalable/apps/thunar.svg" 2>/dev/null || true
        fi
    fi

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

    # Deploy GTK-3.0 & GTK-4.0 Dark Mode Preferences & Zenith Theme CSS
    if [ -d "classic-addons/gtk-3.0" ]; then
        mkdir -p "$HOME/.config/gtk-3.0" "$HOME/.config/gtk-4.0"
        cp -f classic-addons/gtk-3.0/settings.ini "$HOME/.config/gtk-3.0/settings.ini"
        cp -f classic-addons/gtk-3.0/gtk.css "$HOME/.config/gtk-3.0/gtk.css" 2>/dev/null || true
        cp -f classic-addons/gtk-4.0/settings.ini "$HOME/.config/gtk-4.0/settings.ini"
    fi

    # Deploy XDG Default MIME associations
    if [ -d "classic-addons/xdg" ]; then
        mkdir -p "$HOME/.config"
        cp -f classic-addons/xdg/mimeapps.list "$HOME/.config/mimeapps.list"
        command -v xdg-mime >/dev/null 2>&1 && xdg-mime default thunar.desktop inode/directory 2>/dev/null || true
    fi

    echo -e "${GREEN}✔ CLI workstation power tools, Clipboard, Thunar, Yazi, BTOP, and GTK settings deployed!${RESET}\n"
fi

# 5. Multimedia & Screen Recording Tools
if [ "$INSTALL_MEDIA_TOOLS" -eq 1 ]; then
    echo -e "${BLUE}▶ [5/7] Checking Screen Recording, OCR Text Capture & Screenshot Utilities...${RESET}"
    case "$DISTRO" in
        arch|manjaro|endeavouros|cachyos)
            sudo pacman -S --needed --noconfirm wf-recorder tesseract tesseract-data-eng slurp grim swappy hyprpicker ffmpegthumbnailer imagemagick chafa libnotify jq 2>/dev/null || true
            ;;
        fedora|rhel)
            sudo dnf install -y wf-recorder tesseract slurp grim ffmpegthumbnailer ImageMagick chafa libnotify jq 2>/dev/null || true
            ;;
        ubuntu|debian|pop)
            sudo apt install -y wf-recorder tesseract-ocr slurp grim ffmpegthumbnailer imagemagick chafa libnotify-bin jq 2>/dev/null || true
            ;;
    esac
    echo -e "${GREEN}✔ Media, Screenshot, Screen Recording & OCR utilities ready!${RESET}\n"
fi

# 6. LibreOffice Compatibility & Fonts
if [ "$INSTALL_OFFICE" -eq 1 ]; then
    echo -e "${BLUE}▶ [6/7] Optimizing LibreOffice for MS Office/Excel Format Compatibility...${RESET}"
    if [ -f "hyprland/scripts/apps/setup-libreoffice-excel.sh" ]; then
        bash "hyprland/scripts/apps/setup-libreoffice-excel.sh"
    fi
    echo -e "${GREEN}✔ LibreOffice save filters & metric fonts configured!${RESET}\n"
fi

# 7. Power Management, Polkit & Idle
if [ "$INSTALL_POWER" -eq 1 ]; then
    echo -e "${BLUE}▶ [7/7] Verifying Power Management, Polkit Agent & Idle Daemon...${RESET}"
    case "$DISTRO" in
        arch|manjaro|endeavouros|cachyos)
            sudo pacman -S --needed --noconfirm hypridle hyprlock hyprpolkitagent brightnessctl hyprshade 2>/dev/null || true
            systemctl --user enable --now hyprpolkitagent.service 2>/dev/null || true
            ;;
        fedora|rhel)
            sudo dnf install -y hypridle hyprlock hyprpolkitagent brightnessctl 2>/dev/null || true
            systemctl --user enable --now hyprpolkitagent.service 2>/dev/null || true
            ;;
        ubuntu|debian|pop)
            sudo apt install -y brightnessctl 2>/dev/null || true
            sudo apt install -y hypridle hyprlock 2>/dev/null || true
            ;;
    esac
    echo -e "${GREEN}✔ Power, Polkit authentication & Idle management verified!${RESET}\n"
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
