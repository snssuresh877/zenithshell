#!/bin/bash
set -e

echo "🔥 Firefox Optimization Script (Arch + Hyprland)"
echo "---------------------------------------------"

# Find Firefox profile
PROFILE_DIR=$(find ~/.mozilla/firefox -maxdepth 1 -type d -name "*.default-release" | head -n 1)

if [ -z "$PROFILE_DIR" ]; then
  echo "❌ Firefox profile not found. Launch Firefox once and retry."
  exit 1
fi

USERJS="$PROFILE_DIR/user.js"

echo "📁 Firefox profile found:"
echo "   $PROFILE_DIR"

# Backup existing user.js
if [ -f "$USERJS" ]; then
  cp "$USERJS" "$USERJS.bak.$(date +%s)"
  echo "🧷 Existing user.js backed up"
fi

echo "✍️ Writing optimized user.js..."

cat > "$USERJS" <<'EOF'
// ===============================
// 🔥 Performance + Low RAM Tweaks
// ===============================

// ---- Process Control ----
user_pref("dom.ipc.processCount", 4);
user_pref("dom.ipc.processCount.webIsolated", 2);
user_pref("browser.tabs.remote.autostart", true);
user_pref("browser.tabs.remote.autostart.2", true);

// ---- Tab & Session Discipline ----
user_pref("browser.tabs.unloadOnLowMemory", true);
user_pref("browser.sessionstore.interval", 300000);
user_pref("browser.sessionstore.max_tabs_undo", 5);
user_pref("browser.sessionstore.max_windows_undo", 1);

// ---- Cache (RAM > Disk) ----
user_pref("browser.cache.disk.enable", false);
user_pref("browser.cache.memory.enable", true);
user_pref("browser.cache.memory.capacity", 131072);

// ---- Media & Autoplay ----
user_pref("media.autoplay.default", 5);
user_pref("media.autoplay.blocking_policy", 2);
user_pref("media.hardware-video-decoding.enabled", true);
user_pref("media.ffmpeg.vaapi.enabled", true);

// ---- Image Memory ----
user_pref("image.mem.decode_bytes_at_a_time", 65536);
user_pref("image.mem.shared.unmap.min_expiration_ms", 120000);

// ---- Network ----
user_pref("network.http.max-persistent-connections-per-server", 4);
user_pref("network.http.max-connections", 256);

// ---- Telemetry & Background Noise ----
user_pref("toolkit.telemetry.enabled", false);
user_pref("toolkit.telemetry.unified", false);
user_pref("toolkit.telemetry.server", "");
user_pref("browser.ping-centre.telemetry", false);
user_pref("datareporting.healthreport.uploadEnabled", false);
user_pref("app.shield.optoutstudies.enabled", false);

// ---- Smooth Rendering (Wayland) ----
user_pref("gfx.webrender.all", true);
user_pref("gfx.webrender.software", false);
user_pref("widget.wayland-dmabuf-vaapi.enabled", true);
EOF

echo "✅ user.js applied"

# Create firefox-lite launcher
LAUNCHER="$HOME/.local/bin/firefox-lite"

mkdir -p "$HOME/.local/bin"

cat > "$LAUNCHER" <<'EOF'
#!/bin/bash
MOZ_ENABLE_WAYLAND=1 \
MOZ_WEBRENDER=1 \
firefox --no-remote --new-instance
EOF

chmod +x "$LAUNCHER"

echo "🚀 firefox-lite launcher created:"
echo "   ~/.local/bin/firefox-lite"

echo
echo "🎯 DONE!"
echo "➡️ Restart Firefox and launch via rofi using: firefox-lite"
echo "➡️ RAM usage will improve after first restart"
