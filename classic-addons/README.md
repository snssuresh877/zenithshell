# 📦 Classic Addons: Waybar, Rofi & SwayNC (Optional Alternative Stack)

> [!WARNING]
> **⚠️ Performance Notice: Do NOT Run Alongside ZenithShell**
> - **ZenithShell** is an ultra-low memory (~26MB RAM) native C++20 shell that already integrates a Status Bar, Spotlight Launcher, Notification Daemon, Control Center, Reminders, and Clipboard Manager.
> - **Running Waybar, Rofi, or SwayNC at the same time as ZenithShell is strongly NOT recommended.** Doing so creates duplicate D-Bus listeners and adds unnecessary system overhead and CPU load.
> - This directory (`classic-addons/`) is provided **only for users who want a fallback modular stack when ZenithShell is stopped**, or you can safely **delete this folder**.

---

## 🔄 Dual UI Mode Switching (`SUPER + ALT + U`)

The bundled Hyprland configuration includes a dynamic UI mode switcher:
- **ZenithShell Mode (Default)**: Runs the native C++20 shell with 23 dynamic themes, hero cards, and instant D-Bus response.
- **Classic Waybar Mode**: Launches Waybar + Rofi + SwayNC notification center for a classic modular workflow.

Press **`SUPER + ALT + U`** or run:
```bash
~/.config/hypr/scripts/ui/toggle_ui_mode.sh
```

---

## 📂 Directory Structure

```text
classic-addons/
├── kitty/                     # Pro-grade Kitty terminal configuration (Pywal dynamic theme sync)
│   └── kitty.conf             # JetBrainsMono Nerd Font, powerline tabs, smart clipboard
├── foot/                      # Ultra-fast lightweight Wayland native terminal configuration
│   └── foot.ini               # High-FPS rendering, beam cursor, 20K scrollback
├── rofi/                      # App launcher and clipboard picker styles
│   ├── config.rasi            # Modern obsidian app launcher styling
│   ├── colors.rasi            # Dynamic color token mappings
│   └── config-cliphist.rasi   # Clean clipboard history selector style
├── waybar/                    # Classic status bar configuration
│   ├── config.jsonc           # Glass bar module layout (workspaces, sysinfo, audio, tray)
│   ├── style.css              # Executive dark glass stylesheet
│   └── scripts/
│       └── updates.sh         # Package update counter and upgrade executor
└── swaync/                    # Classic notification center
    ├── config.json            # 2x4 quick settings grid, volume/backlight sliders, media player
    └── style.css              # Obsidian dark styling matching Pywal palettes
```

---

## 📜 Detailed Script & Utility Breakdown

The following scripts support both the classic stack and modular Hyprland desktop workflows:

### 1. Status Bar & UI Engine
| Script | Path | Purpose |
|---|---|---|
| **`toggle_ui_mode.sh`** | `hyprland/scripts/ui/toggle_ui_mode.sh` | Hot-swaps between **ZenithShell** and **Waybar + SwayNC** seamlessly without restarting Hyprland. |
| **`toggle_waybar.sh`** | `hyprland/scripts/ui/toggle_waybar.sh` | Toggles the visibility of Waybar (`killall -SIGUSR1 waybar`). |
| **`updates.sh`** | `classic-addons/waybar/scripts/updates.sh` | Queries `checkupdates` & `yay` for pending packages; opens an interactive terminal to upgrade when clicked. |

### 2. Launchers & System Panels
| Script | Path | Purpose |
|---|---|---|
| **`open_launcher.sh`** | `hyprland/scripts/ui/open_launcher.sh` | Opens **Zenith Spotlight** in ZenithShell mode or **Rofi drun** in Classic mode. |
| **`open_controlpanel.sh`** | `hyprland/scripts/ui/open_controlpanel.sh` | Opens **Zenith Control Center** in ZenithShell mode or **SwayNC / Rofi Control Panel** in Classic mode. |
| **`open_notifications.sh`**| `hyprland/scripts/ui/open_notifications.sh` | Opens Zenith Notifications panel or toggles SwayNC. |
| **`open_clipboard.sh`** | `hyprland/scripts/ui/open_clipboard.sh` | Opens **Zenith Clipboard History** or **Rofi Cliphist** picker. |
| **`powermenu.sh`** | `hyprland/scripts/ui/powermenu.sh` | Opens Zenith Power Menu or interactive Rofi session shutdown/reboot menu. |
| **`show_keybinds.sh`** | `hyprland/scripts/ui/show_keybinds.sh` | Displays an interactive searchable cheat sheet of all configured Hyprland hotkeys. |

### 3. Quick Actions & Hardware Controls
| Script | Path | Purpose |
|---|---|---|
| **`rofi_network.sh`** | `hyprland/scripts/ui/rofi_network.sh` | Network menu: shows Wi-Fi status, saved passwords, QR sharing, latency test, and connects to networks. |
| **`capture_menu.sh`** | `hyprland/scripts/ui/capture_menu.sh` | Unified screen capture menu: active window screenshot, region screenshot, screen recording (`wf-recorder`), OCR text extraction (`tesseract`), color picker (`hyprpicker`), and QR scanning. |
| **`hardware_menu.sh`** | `hyprland/scripts/ui/hardware_menu.sh` | Quick hardware reset menu: restarts PipeWire audio, BlueZ Bluetooth, Wi-Fi drivers, trackpad, and reloads monitors. |
| **`toggle_darkmode.sh`** | `hyprland/scripts/ui/toggle_darkmode.sh` | Toggles GNOME / GTK3 color scheme between light and dark mode. |
| **`toggle_nightlight.sh`**| `hyprland/scripts/ui/toggle_nightlight.sh`| Toggles blue-light filter (`hyprsunset` / `gammastep` / `wlsunset`). |
| **`toggle_dnd.sh`** | `hyprland/scripts/ui/toggle_dnd.sh` | Toggles Do Not Disturb mode for notifications. |
| **`switch_wallpaper.sh`** | `hyprland/scripts/ui/switch_wallpaper.sh`| Cycles wallpapers across the curated pack with smooth animated transitions. |
| **`ai_explain_selection.sh`**| `hyprland/scripts/ui/ai_explain_selection.sh`| Copies currently highlighted text on screen and requests an instant explanation from a local AI assistant. |

---

## 🗑️ How to Keep or Remove

### To Keep and Install:
```bash
cp -r classic-addons/kitty ~/.config/
cp -r classic-addons/foot ~/.config/
cp -r classic-addons/rofi ~/.config/
cp -r classic-addons/waybar ~/.config/
cp -r classic-addons/swaync ~/.config/
```

### To Remove:
If you exclusively use **ZenithShell** and do not want these fallback configs:
```bash
rm -rf classic-addons/
```
ZenithShell will continue running independently with zero issues.
