# ZenithShell Configuration & Dotfile Manual

Welcome to ZenithShell! ZenithShell is designed with a **Zero-Setup Philosophy**: upon first launch, all necessary configuration files, stylesheets, and directories are automatically scaffolded into your home directory (`~/.config/zenithshell/`) if they do not already exist.

This manual documents the configuration system, file formats, dynamic theme styling, and command-line utilities.

---

## Table of Contents
1. [Dotfile Locations & Hierarchy](#1-dotfile-locations--hierarchy)
2. [CLI Configuration Suite (`zenithctl config`)](#2-cli-configuration-suite-zenithctl-config)
3. [Configuration Schema (`config.json`)](#3-configuration-schema-configjson)
4. [Styling & GTK CSS Reference (`style.css`)](#4-styling--gtk-css-reference-stylecss)
5. [Dynamic CSS Theme Variables](#5-dynamic-css-theme-variables)
6. [Hyprland & Compositor Keybindings](#6-hyprland--compositor-keybindings)
7. [Live Reloading](#7-live-reloading)

---

## 1. Dotfile Locations & Hierarchy

When ZenithShell boots, it resolves configuration files in the following order:

1. **User Dotfiles** (Highest priority):
   - `~/.config/zenithshell/config.json` - Main options & widget flags
   - `~/.config/zenithshell/style.css` - Custom GTK3 CSS styling
   - `~/.config/zenithshell/themes/` - User theme overrides
2. **Current Working Directory** (Development / local run):
   - `./config.json`
   - `./style.css`
3. **System Defaults** (System package install):
   - `/usr/share/zenithshell/config.json`
   - `/usr/share/zenithshell/style.css`

> **Note**: If `~/.config/zenithshell/config.json` or `~/.config/zenithshell/style.css` do not exist, ZenithShell automatically creates the directory and scaffolds fully-formed default files on boot.

---

## 2. CLI Configuration Suite (`zenithctl config`)

Manage your ZenithShell dotfiles directly from the terminal:

```bash
# Print configuration directory path
zenithctl config path

# Re-scaffold or initialize dotfiles if missing
zenithctl config init

# Open config.json in your preferred $EDITOR (falls back to nano)
zenithctl config edit

# Open style.css in your preferred $EDITOR
zenithctl config style

# Display terminal quick manual
zenithctl config manual

# Hot-reload configuration and stylesheet live without restarting ZenithShell
zenithctl reload
```

---

## 3. Configuration Schema (`config.json`)

Here is an example `~/.config/zenithshell/config.json`:

```json
{
    "position": "top",
    "height": 28,
    "margin_top": 4,
    "margin_bottom": 0,
    "margin_left": 12,
    "margin_right": 12,
    "exclusive_zone": true,
    "workspaces": {
        "count": 10,
        "show_icons": true
    },
    "clock": {
        "format": "📅 %a %b %d  🕒 %H:%M"
    },
    "sys_info": {
        "update_interval_ms": 1500,
        "show_cpu": true,
        "show_ram": true,
        "show_battery": true
    },
    "wallpaper_dir": "~/Pictures/wallpapers"
}
```

### Options Description

| Key | Type | Default | Description |
| :--- | :--- | :--- | :--- |
| `position` | string | `"top"` | Placement of the status bar: `"top"` or `"bottom"`. |
| `height` | integer | `28` | Bar height in pixels. |
| `margin_top` | integer | `4` | Top margin in pixels. |
| `margin_bottom` | integer | `0` | Bottom margin in pixels. |
| `margin_left` | integer | `12` | Left outer margin. |
| `margin_right` | integer | `12` | Right outer margin. |
| `exclusive_zone` | boolean | `true` | When `true`, reserves space on the monitor so windows do not overlap the bar. |
| `workspaces.count` | integer | `10` | Number of workspace indicators displayed. |
| `workspaces.show_icons` | boolean | `true` | Show window/workspace glyphs. |
| `clock.format` | string | `"📅 %a %b %d  🕒 %H:%M"` | Standard C `strftime` format string for the top bar clock widget. |
| `sys_info.update_interval_ms` | integer | `1500` | Refresh interval in milliseconds for CPU, RAM, and Battery monitors. |
| `sys_info.show_cpu` | boolean | `true` | Display CPU usage percentage on the bar. |
| `sys_info.show_ram` | boolean | `true` | Display RAM usage on the bar. |
| `sys_info.show_battery` | boolean | `true` | Display battery status indicator. |
| `wallpaper_dir` | string | `"~/Pictures/wallpapers"` | Directory scanned by the Wallpaper Selector UI. |

---

## 4. Styling & GTK CSS Reference (`style.css`)

ZenithShell uses GTK3 CSS for all UI modules. You can edit `~/.config/zenithshell/style.css` to completely customize fonts, roundness, borders, and animations.

### Key CSS Selectors

| Selector | UI Element |
| :--- | :--- |
| `.zenith-bar` | Main desktop top bar / status bar container |
| `.workspace-pill` | Workspace indicator pill |
| `.workspace-pill.active` | Currently focused workspace |
| `.workspace-pill.occupied` | Workspace with running windows |
| `.clock-widget` | Clock & calendar button |
| `.sys-widget` | Hardware metric meters (CPU, RAM, Net) |
| `.quick-toggle-btn` | Control Center quick setting toggle buttons |
| `.quick-toggle-btn.active` | Enabled quick toggle state (WiFi, BT, DND) |
| `.spotlight-window` | App launcher & command palette window |
| `.spotlight-search` | Search input bar inside app launcher |
| `.notification-popup` | Floating notification toast card |
| `.notification-center-window`| Notification history sidebar drawer |
| `.zenith-osd` | On-Screen Display overlay (Volume & Brightness) |
| `.clipboard-window` | Clipboard manager history popup |
| `.active-apps-drawer` | Running applications dock / drawer |

---

## 5. Dynamic CSS Theme Variables

ZenithShell's `ThemeEngine` injects real-time CSS color definitions into GTK styling:

```css
@define-color zenith_bg             rgba(18, 18, 24, 0.85);
@define-color zenith_surface        rgba(30, 30, 42, 0.70);
@define-color zenith_surface_hover  rgba(45, 45, 62, 0.85);
@define-color zenith_accent         #7aa2f7;
@define-color zenith_text_primary   #c0caf5;
@define-color zenith_text_secondary #7982a9;
@define-color zenith_border         rgba(255, 255, 255, 0.08);
```

You can use these `@zenith_*` colors anywhere in your custom `style.css` for consistent, reactive theming.

---

## 6. Hyprland & Compositor Keybindings

To bind ZenithShell modules in `~/.config/hypr/hyprland.conf`:

```ini
# ZenithShell Quick Launchers & Overlays
bind = SUPER, SPACE, exec, zenithctl launcher
bind = SUPER, V, exec, zenithctl clipboard
bind = SUPER, C, exec, zenithctl control-center
bind = SUPER, N, exec, zenithctl notifications
bind = SUPER, K, exec, zenithctl keybinds
bind = SUPER, R, exec, zenithctl reminders
bind = SUPER, TAB, exec, zenithctl active-apps
bind = SUPER, E, exec, zenithctl files

# Volume and Brightness OSD keys
bindel = , XF86AudioRaiseVolume, exec, zenithctl volume +5
bindel = , XF86AudioLowerVolume, exec, zenithctl volume -5
bindl  = , XF86AudioMute,        exec, zenithctl volume mute
bindl  = , XF86AudioMicMute,     exec, zenithctl mic
bindel = , XF86MonBrightnessUp,   exec, zenithctl brightness +5
bindel = , XF86MonBrightnessDown, exec, zenithctl brightness -5

# Media Controls
bindl = , XF86AudioPlay, exec, zenithctl media play-pause
bindl = , XF86AudioNext, exec, zenithctl media next
bindl = , XF86AudioPrev, exec, zenithctl media prev
```

### Autostarting ZenithShell
In your `hyprland.conf`:
```ini
exec-once = zenithshell
```

---

## 7. Live Reloading

Whenever you modify `~/.config/zenithshell/config.json` or `~/.config/zenithshell/style.css`:
- `style.css` will **automatically hot-reload** via GTK file monitoring.
- To force an immediate reload of both config and stylesheet without restarting the daemon, execute:
  ```bash
  zenithctl reload
  ```
