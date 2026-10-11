#!/usr/bin/env bash
set -euo pipefail

TARGET_DIR="$HOME/.local/share/applications/keybinds"
SCRIPTS_DIR="$HOME/.config/hypr/scripts/ui"
mkdir -p "$TARGET_DIR"

cat << DESKTOP_EOF > "$TARGET_DIR/keybind-1.desktop"
[Desktop Entry]
Type=Application
Name=SUPER + SPACE : Launch Application Launcher
Comment=Hyprland System Keybinding
Exec=$SCRIPTS_DIR/open_launcher.sh
Icon=preferences-desktop-launcher
Terminal=false
Categories=System;Utility;
DESKTOP_EOF

cat << DESKTOP_EOF > "$TARGET_DIR/keybind-2.desktop"
[Desktop Entry]
Type=Application
Name=SUPER + A : Open Control Panel & Quick Settings
Comment=Hyprland System Keybinding
Exec=$SCRIPTS_DIR/open_controlpanel.sh
Icon=preferences-system
Terminal=false
Categories=System;Utility;
DESKTOP_EOF

cat << DESKTOP_EOF > "$TARGET_DIR/keybind-3.desktop"
[Desktop Entry]
Type=Application
Name=SUPER + N : Open Notification Center
Comment=Hyprland System Keybinding
Exec=$SCRIPTS_DIR/open_notifications.sh
Icon=preferences-desktop-notification
Terminal=false
Categories=System;Utility;
DESKTOP_EOF

cat << DESKTOP_EOF > "$TARGET_DIR/keybind-4.desktop"
[Desktop Entry]
Type=Application
Name=SUPER + V : Open Clipboard History
Comment=Hyprland System Keybinding
Exec=$SCRIPTS_DIR/open_clipboard.sh
Icon=edit-paste
Terminal=false
Categories=System;Utility;
DESKTOP_EOF

cat << DESKTOP_EOF > "$TARGET_DIR/keybind-5.desktop"
[Desktop Entry]
Type=Application
Name=SUPER + W : Toggle Status Bar Visibility
Comment=Hyprland System Keybinding
Exec=$SCRIPTS_DIR/toggle_waybar.sh
Icon=panel
Terminal=false
Categories=System;Utility;
DESKTOP_EOF

cat << DESKTOP_EOF > "$TARGET_DIR/keybind-6.desktop"
[Desktop Entry]
Type=Application
Name=SUPER + ALT + U : Toggle UI Engine
Comment=Hyprland System Keybinding
Exec=$SCRIPTS_DIR/toggle_ui_mode.sh
Icon=preferences-desktop-theme
Terminal=false
Categories=System;Utility;
DESKTOP_EOF

cat << DESKTOP_EOF > "$TARGET_DIR/keybind-7.desktop"
[Desktop Entry]
Type=Application
Name=SUPER + P : Open Pavucontrol Audio Mixer
Comment=Hyprland System Keybinding
Exec=pavucontrol
Icon=audio-card
Terminal=false
Categories=System;Audio;
DESKTOP_EOF

cat << DESKTOP_EOF > "$TARGET_DIR/keybind-8.desktop"
[Desktop Entry]
Type=Application
Name=SUPER + SHIFT + P : Open Interactive Power Menu
Comment=Hyprland System Keybinding
Exec=$SCRIPTS_DIR/powermenu.sh
Icon=system-shutdown
Terminal=false
Categories=System;Utility;
DESKTOP_EOF

cat << DESKTOP_EOF > "$TARGET_DIR/keybind-9.desktop"
[Desktop Entry]
Type=Application
Name=SUPER + K : Keybindings Cheatsheet
Comment=Hyprland System Keybinding
Exec=$SCRIPTS_DIR/show_keybinds.sh
Icon=dialog-information
Terminal=false
Categories=System;Utility;
DESKTOP_EOF

echo "Keybindings desktop entries generated in $TARGET_DIR"
