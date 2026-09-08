# 🍥 ZenithShell & Hyprland: The Complete Debian User Guide

This guide is for **Debian** users (Debian 12 Bookworm, Debian 13 Trixie, and Debian Sid / Testing) who want the ultra-fast, modern **ZenithShell (~26MB RAM)** desktop experience paired with **Debian's rock-solid stability**.

---

## ⚡ Quick Start: 1-Click Automated Debian Installer

If you already have a working Debian or Ubuntu installation, you can build and set up ZenithShell with one command:

```bash
git clone https://github.com/snssuresh877/zenithshell.git ~/Projects/zenithshell
cd ~/Projects/zenithshell
chmod +x ./install-debian.sh
./install-debian.sh
```

The script automatically:
* Installs all Debian build dependencies (`libgtk-3-dev`, `libgtk-layer-shell-dev`, `nlohmann-json3-dev`, etc.).
* Sets up PipeWire, WirePlumber, NetworkManager, BlueZ, and UDisks2.
* Dynamic wallpaper theming is **100% native in C++** (using GdkPixbuf) — zero Python or `pywal` needed, completely bypassing Debian 12's PEP 668 restrictions!
* Installs the modern `starship` prompt binary.
* Compiles ZenithShell with CMake & Ninja in `Release` mode.
* Deploys binary to `~/.local/bin/zenithshell`, desktop entry, systemd user service, 23 themes, and wallpapers.

---

## 🧭 Step 1: Getting Hyprland on Debian

ZenithShell is a Wayland layer-shell desktop suite that pairs natively with **Hyprland**. Depending on your Debian release:

### Option A: Debian 13 (Trixie) or Debian Sid (Recommended for Hyprland)
Hyprland is packaged directly in the Debian repositories:
```bash
# On Debian 13 Trixie:
sudo apt install -t trixie-backports hyprland

# On Debian Sid (Unstable):
sudo apt install hyprland
```

### Option B: Debian 12 (Bookworm)
Debian 12's official package repositories freeze older libraries and do not contain Hyprland. You have two rock-solid options:

1. **Install via Nix Package Manager (Easiest & Cleanest):**
   ```bash
   sh <(curl -L https://nixos.org/nix/install) --daemon
   nix-env -iA nixpkgs.hyprland
   ```
2. **Use Pre-Built Community Packages or Build from Source:**
   * Refer to the official [Hyprland Debian Guide](https://wiki.hyprland.org/Getting-Started/Installation/#debian).

---

## 📦 Step 2: Complete 1:1 Debian APT Package List

Run this single `apt` command to install all build tools, desktop infrastructure, audio, network, and utilities:

```bash
sudo apt update && sudo apt install -y \
    build-essential g++ cmake ninja-build pkg-config \
    libgtk-3-dev libgtk-layer-shell-dev libcairo2-dev libpango1.0-dev libglib2.0-dev nlohmann-json3-dev \
    pipewire wireplumber pipewire-pulse pipewire-alsa pavucontrol playerctl \
    network-manager network-manager-gnome bluez blueman \
    brightnessctl \
    udisks2 dosfstools ntfs-3g exfatprogs gvfs gvfs-backends \
    wl-clipboard grim slurp wf-recorder tesseract-ocr libnotify-bin jq \
    foot kitty fish btop ripgrep fd-find fzf zip unzip p7zip-full \
    imagemagick ffmpegthumbnailer chafa \
    fonts-noto fonts-noto-cjk fonts-noto-color-emoji \
    fonts-liberation fonts-carlito fonts-caladea \
    libreoffice pipx
```

### Optional Packages (Available in Debian 13 / Sid):
```bash
sudo apt install -y hypridle hyprlock zoxide 2>/dev/null || true
```

---

## 🔄 Step 3: Debian vs Arch Package Translation Table

Here is how packages map from Arch Linux to Debian:

| Component | 🐧 Arch Linux Package | 🍥 Debian Package | Notes / Alternative |
|---|---|---|---|
| **Layer Shell C++ Headers** | `gtk-layer-shell` | `libgtk-layer-shell-dev` | Native Wayland overlay headers. |
| **JSON Parser** | `nlohmann-json` | `nlohmann-json3-dev` | Modern C++ JSON parsing. |
| **Network Manager GUI** | `network-manager-applet` | `network-manager-gnome` | Provides `nm-connection-editor`. |
| **Fast Find Tool** | `fd` | `fd-find` | Executable named `fdfind` on Debian. |
| **Dynamic Palette Extractor** | Inbuilt C++ (`GdkPixbuf`) | Built-in | Zero Python/Pywal dependency; extracts in < 5ms. |
| **Nerd Fonts** | `ttf-jetbrains-mono-nerd` | Standalone download | Download from Nerd Fonts releases. |
| **Starship Prompt** | `starship` | Standalone binary | Installed via official install script. |
| **TUI File Manager** | `yazi` | Standalone binary / cargo | Available in Sid or via GitHub binary. |
| **Wallpaper Engine** | Native C++ (`gtk-layer-shell`) | Built-in | Inbuilt 60fps Wayland transitions; zero external daemons (`awww`/`swww`). |
| **Clipboard Engine** | Native C++ (`GtkClipboard`) | Built-in | Event-driven capture & reboot JSON persistence; no `cliphist`. |
| **PolicyKit Agent** | `hyprpolkitagent` | `polkit-kde-agent-1` | Available in APT (`/usr/lib/x86_64-linux-gnu/libexec/polkit-kde-authentication-agent-1`). |
| **Lock Screen & Idle** | `hypridle`, `hyprlock` | `hypridle`, `hyprlock` (Trixie) or `swayidle`, `swaylock` (Bookworm) | Clean fallback for lock and sleep. |

---

## 🛠️ Step 4: Bridging Debian Ecosystem Differences

### 1. Inbuilt Dynamic Wallpaper Palette Extraction
Historically, dynamic wallpaper theming required external Python packages (`pywal`), which often failed on Debian due to **PEP 668** package locks.
ZenithShell now features a **pure in-process C++ dynamic palette extractor** directly utilizing `GdkPixbuf` and fast 15-bit color quantization:
- Extracts dominant moods, vivid accents, and readable foregrounds in **< 5ms**.
- Automatically generates Pywal-compatible cache files (`~/.cache/wal/colors.json`, `colors.sh`, `colors`, `wal`) so downstream terminal applications (Foot, Kitty, Neovim) continue to work seamlessly without requiring Python or `pipx`.

### 2. Nerd Fonts for Status Icons (`JetBrains Mono Nerd Font`)
To ensure TopBar and Control Center glyphs display with 100% precision:
```bash
mkdir -p ~/.local/share/fonts
cd /tmp
wget https://github.com/ryanoasis/nerd-fonts/releases/latest/download/JetBrainsMono.tar.xz
tar -xf JetBrainsMono.tar.xz -C ~/.local/share/fonts/
fc-cache -fv
```

### 3. Starship Shell Prompt
```bash
curl -sS https://starship.rs/install.sh | sh -s -- -y
```

### 4. Yazi TUI File Manager (Optional)
If you are on Debian 12 where `yazi` is not yet in the APT repos:
```bash
mkdir -p ~/.local/bin
cd /tmp
wget https://github.com/sxyazi/yazi/releases/latest/download/yazi-x86_64-unknown-linux-musl.zip
unzip yazi-x86_64-unknown-linux-musl.zip
cp yazi-x86_64-unknown-linux-musl/yazi ~/.local/bin/
chmod +x ~/.local/bin/yazi
```

### 5. Wallpaper Daemon on Debian
ZenithShell's Theme Engine communicates with animated wallpaper daemons. On Debian, you can either:
* **Option A (Simplest):** Use `swaybg` (already in APT):
  ```bash
  sudo apt install -y swaybg
  ```
* **Option B (Animated transitions):** Download the static `swww` release from GitHub:
  ```bash
  wget https://github.com/LGFae/swww/releases/latest/download/swww-x86_64-unknown-linux-musl.tar.gz
  tar -xzf swww-x86_64-unknown-linux-musl.tar.gz -C ~/.local/bin/
  ```

---

## ⚙️ Step 5: Enable Essential System Services

Run these commands to ensure network, Bluetooth, and disk auto-mounting start on boot:

```bash
# Enable system daemons
sudo systemctl enable --now NetworkManager
sudo systemctl enable --now bluetooth
sudo systemctl enable --now udisks2

# Enable user audio and desktop services
systemctl --user enable --now pipewire.service wireplumber.service
systemctl --user enable --now zenithshell.service
```

---

## 💡 Key Advantages of Debian for ZenithShell

1. **Rock-Solid Kernel Modules:**  
   Unlike Arch, where a kernel update immediately purges `/usr/lib/modules/` (breaking newly plugged USB drives until reboot), **Debian preserves the running kernel's modules intact across updates**. Newly plugged USB flash drives and devices always initialize reliably.
2. **Zero Dependency Drift:**  
   Debian's core GTK, Cairo, and Pango libraries remain stable, meaning your ZenithShell build will continue to run for years without requiring rebuilds due to shared library ABI bumps.
3. **Low Idle Footprint:**  
   A base Debian install running ZenithShell achieves an astonishing **~250 MB total RAM usage on cold boot** for the entire operating system and desktop environment combined.

---

## 🚀 Running ZenithShell

Once installed, simply start ZenithShell:

```bash
# Run standalone
zenithshell

# Or check IPC responsiveness
gdbus call --session --dest dev.zenith.Shell --object-path /dev/zenith/Shell --method dev.zenith.Shell.GetStats
```

Enjoy an ultra-fast, modern Wayland desktop on rock-solid Debian! 🍥🌌
