--- ~/.config/hypr/modules/binds.lua
--
-- Hyprland 0.55+ Lua keybindings
--
-- ZenithShell integrated for TopBar, Spotlight, Keybinds, & Control Center

local MOD = MOD

-- WORKSPACE KEY CODES

local numberkey = {
	10,
	11,
	12,
	13,
	14,
	15,
	16,
	17,
	18,
	19,
}

-- COMMAND CENTER / LAUNCHER

hl.bind(MOD .. " + A", hl.dsp.exec_cmd("$HOME/.config/hypr/scripts/ui/open_controlpanel.sh"), {
	description = "Interactive Control Panel",
})

hl.bind(MOD .. " + SPACE", hl.dsp.exec_cmd("$HOME/.config/hypr/scripts/ui/open_launcher.sh"), {
	description = "Application launcher",
})

hl.bind(MOD .. " + K", hl.dsp.exec_cmd("$HOME/.config/hypr/scripts/ui/show_keybinds.sh"), {
	description = "Keybindings Cheatsheet",
})

hl.bind(MOD .. " + I", hl.dsp.exec_cmd("$HOME/.config/hypr/scripts/ui/open_network.sh"), {
	description = "Network & Wi-Fi Management",
})

-- APPLICATIONS

hl.bind(MOD .. " + RETURN", hl.dsp.exec_cmd("$HOME/.config/hypr/scripts/ui/open_terminal_here.sh foot"), {
	description = "Terminal in Current Directory",
})

hl.bind(MOD .. " + SHIFT + RETURN", hl.dsp.exec_cmd("$HOME/.config/hypr/scripts/ui/open_terminal_here.sh kitty"), {
	description = "Kitty Terminal in Current Directory",
})

hl.bind(MOD .. " + Q", hl.dsp.exec_cmd(Apps.secondTerminal), {
	description = "Second terminal",
})

hl.bind(MOD .. " + E", hl.dsp.exec_cmd("$HOME/.config/hypr/scripts/ui/open_filemanager.sh gui"), {
	description = "Cosmic / GUI File Manager",
})

hl.bind(MOD .. " + SHIFT + E", hl.dsp.exec_cmd("$HOME/.config/hypr/scripts/ui/open_filemanager.sh yazi"), {
	description = "Yazi TUI File Manager",
})

hl.bind(MOD .. " + B", hl.dsp.exec_cmd(Apps.browser), {
	description = "Browser",
})

-- LOCAL AI ASSISTANT (OLLAMA)
hl.bind(MOD .. " + O", hl.dsp.exec_cmd("foot --app-id=ai_assistant_float -T 'Zenith AI Assistant' -e $HOME/.local/bin/ai"), {
	description = "Local AI Assistant",
})

hl.bind(MOD .. " + SHIFT + A", hl.dsp.exec_cmd("$HOME/.config/hypr/scripts/ui/ai_explain_selection.sh"), {
	description = "AI Explain Selection",
})

-- DO NOT DISTURB
hl.bind(
	MOD .. " + SHIFT + N",
	hl.dsp.exec_cmd([[
sh -c '
notify-send -u low "Do Not Disturb" "Toggled"
'
]]),
	{
		description = "Toggle do not disturb",
	}
)

-- POWER & SESSION CONTROLS

hl.bind(MOD .. " + L", hl.dsp.exec_cmd(Session.lock), {
	description = "Lock screen",
})

hl.bind(MOD .. " + P", hl.dsp.exec_cmd("pavucontrol"), {
	description = "Audio control mixer",
})

hl.bind(MOD .. " + SHIFT + P", hl.dsp.exec_cmd("$HOME/.config/hypr/scripts/ui/powermenu.sh"), {
	description = "Interactive Power menu",
})

hl.bind(MOD .. " + SHIFT + R", hl.dsp.exec_cmd(Session.reboot), {
	description = "Reboot system",
})

hl.bind(MOD .. " + SHIFT + X", hl.dsp.exit(), {
	description = "Logout session",
})

-- WINDOW CONTROL

hl.bind(
	MOD .. " + F",
	hl.dsp.window.float({
		action = "toggle",
	}),
	{
		description = "Toggle floating",
	}
)

hl.bind(MOD .. " + C", hl.dsp.window.close(), {
	description = "Close window",
})

hl.bind(MOD .. " + CTRL + Q", hl.dsp.exit(), {
	description = "Exit Hyprland",
})

-- LAYOUT CONTROL

hl.bind(MOD .. " + J", hl.dsp.layout("togglesplit"), {
	description = "Toggle split",
})

hl.bind(MOD .. " + D", hl.dsp.layout("layout dwindle"), {
	description = "Dwindle layout",
})

hl.bind(MOD .. " + M", hl.dsp.layout("layout master"), {
	description = "Master layout",
})

hl.bind(MOD .. " + G", hl.dsp.layout("layout scroll"), {
	description = "Scrolling layout",
})

hl.bind(MOD .. " + SHIFT + F", hl.dsp.layout("layout monocle"), {
	description = "Monocle layout",
})

-- TOPBAR (WAYBAR)
hl.bind(MOD .. " + W", hl.dsp.exec_cmd("$HOME/.config/hypr/scripts/ui/toggle_waybar.sh"), {
	description = "Toggle Waybar Status Bar",
})

-- DESKTOP UI ENGINE SWITCHER (ZENITHSHELL / QUICKSHELL / WAYBAR)
hl.bind(MOD .. " + ALT + U", hl.dsp.exec_cmd("$HOME/.config/hypr/scripts/ui/toggle_ui_mode.sh"), {
	description = "Cycle Desktop UI Engine (ZenithShell / Quickshell / Waybar)",
})

-- WALLPAPER

hl.bind(MOD .. " + SHIFT + W", hl.dsp.exec_cmd(Wallpaper.script), {
	description = "Change wallpaper",
})

-- CLIPBOARD
hl.bind(MOD .. " + V", hl.dsp.exec_cmd("$HOME/.config/hypr/scripts/ui/open_clipboard.sh"), {
	description = "Clipboard Manager",
})

-- SCREENSHOTS

-- Active window screenshot

hl.bind(
	"Print",
	hl.dsp.exec_cmd([[
sh -c '
DIR="$HOME/Pictures/Screenshots"
mkdir -p "$DIR"

        FILE="$DIR/window_$(date +%Y-%m-%d_%H-%M-%S).png"

        GEOM=$(hyprctl activewindow -j | \
            jq -r "\"\(.at[0]),\(.at[1]) \(.size[0])x\(.size[1])\"")

        grim -g "$GEOM" "$FILE" &&
        wl-copy < "$FILE" &&
        notify-send "Screenshot" "Window screenshot saved"
    '
]]),
	{
		description = "Screenshot window",
	}
)

-- Area screenshot

hl.bind(
	"SHIFT + Print",
	hl.dsp.exec_cmd([[
sh -c '
DIR="$HOME/Pictures/Screenshots"
mkdir -p "$DIR"

        FILE="$DIR/area_$(date +%Y-%m-%d_%H-%M-%S).png"

        grim -g "$(slurp)" "$FILE" &&
        wl-copy < "$FILE" &&
        notify-send "Screenshot" "Area screenshot saved"
    '
]]),
	{
		description = "Screenshot area",
	}
)

-- UTILITIES

-- Color picker

hl.bind(
	MOD .. " + SHIFT + C",
	hl.dsp.exec_cmd([[
sh -c '
COLOR=$(hyprpicker -a)
wl-copy <<< "$COLOR"
notify-send "Color Picker" "Color copied: $COLOR"
'
]]),
	{
		description = "Color picker",
	}
)

-- NOTIFICATION CENTER

hl.bind(MOD .. " + N", hl.dsp.exec_cmd("$HOME/.config/hypr/scripts/ui/open_notifications.sh"), {
	description = "Toggle Notification Center",
})

-- REMINDERS

hl.bind(MOD .. " + R", hl.dsp.exec_cmd("gdbus call --session --dest dev.zenith.Shell --object-path /dev/zenith/Shell --method dev.zenith.Shell.ToggleReminders"), {
	description = "ZenithShell Reminder Overlay",
})

-- OCR

hl.bind(
	MOD .. " + SHIFT + T",
	hl.dsp.exec_cmd([[
sh -c '
grim -g "$(slurp)" - |
tesseract stdin stdout 
-l eng 
--oem 1 
--psm 6 2>/dev/null |
wl-copy &&
notify-send "OCR" "Text copied"
'
]]),
	{
		description = "OCR selection",
	}
)

-- SCREEN RECORDING

hl.bind(
	MOD .. " + SHIFT + R",
	hl.dsp.exec_cmd([[
sh -c '
PIDFILE="/tmp/wf-recorder.pid"

        if [ -f "$PIDFILE" ]; then
            kill "$(cat "$PIDFILE")"
            rm "$PIDFILE"
            notify-send "Screen Recording" "Recording stopped"
        else
            FILE="$HOME/Videos/recording_$(date +%Y-%m-%d_%H-%M-%S).mp4"

            wf-recorder -g "$(slurp -d)" -f "$FILE" &
            echo $! > "$PIDFILE"

            notify-send "Screen Recording" "Recording started"
        fi
    '
]]),
	{
		description = "Toggle screen recording",
	}
)

-- FOCUS MOVEMENT

hl.bind(
	MOD .. " + left",
	hl.dsp.focus({
		direction = "l",
	}),
	{
		description = "Focus left",
	}
)

hl.bind(
	MOD .. " + right",
	hl.dsp.focus({
		direction = "r",
	}),
	{
		description = "Focus right",
	}
)

hl.bind(
	MOD .. " + up",
	hl.dsp.focus({
		direction = "u",
	}),
	{
		description = "Focus up",
	}
)

hl.bind(
	MOD .. " + down",
	hl.dsp.focus({
		direction = "d",
	}),
	{
		description = "Focus down",
	}
)

-- MOVE WINDOWS

hl.bind(
	MOD .. " + SHIFT + left",
	hl.dsp.window.move({
		direction = "l",
	}),
	{
		description = "Move window left",
	}
)

hl.bind(
	MOD .. " + SHIFT + right",
	hl.dsp.window.move({
		direction = "r",
	}),
	{
		description = "Move window right",
	}
)

hl.bind(
	MOD .. " + SHIFT + up",
	hl.dsp.window.move({
		direction = "u",
	}),
	{
		description = "Move window up",
	}
)

hl.bind(
	MOD .. " + SHIFT + down",
	hl.dsp.window.move({
		direction = "d",
	}),
	{
		description = "Move window down",
	}
)

-- WORKSPACES

for i = 1, 10 do
	hl.bind(
		"SUPER + code:" .. numberkey[i],
		hl.dsp.focus({
			workspace = i,
		}),
		{
			description = "Workspace " .. i,
		}
	)
end

-- MOVE WINDOW TO WORKSPACE

for i = 1, 10 do
	hl.bind(
		"SUPER + SHIFT + code:" .. numberkey[i],
		hl.dsp.window.move({
			workspace = i,
			follow = false,
		}),
		{
			description = "Move window to workspace " .. i,
		}
	)
end

-- MOUSE WORKSPACE NAVIGATION

hl.bind(MOD .. " + mouse_down", hl.dsp.exec_cmd("hyprctl dispatch workspace e+1"), {
	mouse = true,
	description = "Next workspace",
})

hl.bind(MOD .. " + mouse_up", hl.dsp.exec_cmd("hyprctl dispatch workspace e-1"), {
	mouse = true,
	description = "Previous workspace",
})

-- MOUSE WINDOW CONTROL

hl.bind(MOD .. " + mouse:272", hl.dsp.window.drag(), {
	mouse = true,
	description = "Move window",
})

hl.bind(MOD .. " + mouse:273", hl.dsp.window.resize(), {
	mouse = true,
	description = "Resize window",
})

-- AUDIO

hl.bind("XF86AudioRaiseVolume", hl.dsp.exec_cmd(Audio.volumeUp), {
	locked = true,
	repeating = true,
})

hl.bind("XF86AudioLowerVolume", hl.dsp.exec_cmd(Audio.volumeDown), {
	locked = true,
	repeating = true,
})

hl.bind("XF86AudioMute", hl.dsp.exec_cmd(Audio.mute), {
	locked = true,
})

hl.bind("XF86AudioMicMute", hl.dsp.exec_cmd(Audio.micMute), {
	locked = true,
})

-- BRIGHTNESS

hl.bind("XF86MonBrightnessUp", hl.dsp.exec_cmd(Brightness.up), {
	locked = true,
	repeating = true,
})

hl.bind("XF86MonBrightnessDown", hl.dsp.exec_cmd(Brightness.down), {
	locked = true,
	repeating = true,
})

-- MEDIA

hl.bind("XF86AudioNext", hl.dsp.exec_cmd(Media.next), {
	locked = true,
})

hl.bind("XF86AudioPause", hl.dsp.exec_cmd(Media.playPause), {
	locked = true,
})

hl.bind("XF86AudioPlay", hl.dsp.exec_cmd(Media.playPause), {
	locked = true,
})

hl.bind("XF86AudioPrev", hl.dsp.exec_cmd(Media.previous), {
	locked = true,
})

-- SYSTEM SCRIPTS

hl.bind(MOD .. " + U", hl.dsp.exec_cmd(System.update), {
	description = "System updates",
})

hl.bind(MOD .. " + Y", hl.dsp.exec_cmd(System.clean), {
	description = "System cleanup",
})

-- Yazi
hl.bind(MOD .. " + CTRL + E", hl.dsp.exec_cmd("foot -e yazi"), {
	description = "Yazi",
})

------------------------------------------------------------
-- SYSTEM CONTROLS (TUI & UTILITY HOTKEYS)
------------------------------------------------------------

-- Super + Ctrl + A -> Audio Routing & Settings (ZenithShell Control Center)
hl.bind(MOD .. " + CTRL + A", hl.dsp.exec_cmd("gdbus call --session --dest dev.zenith.Shell --object-path /dev/zenith/Shell --method dev.zenith.Shell.ToggleAudio"), {
	description = "Audio Settings & Routing (Control Center)",
})

-- Super + Ctrl + B -> Bluetooth controls (bluetui)
hl.bind(MOD .. " + CTRL + B", hl.dsp.exec_cmd("foot --app-id=bluetui_float -T 'Bluetooth Manager' -e bluetui"), {
	description = "Bluetooth controls (bluetui)",
})

-- Super + Ctrl + W -> Network & Wi-Fi Manager (ZenithShell nmcli GUI)
hl.bind(MOD .. " + CTRL + W", hl.dsp.exec_cmd("gdbus call --session --dest dev.zenith.Shell --object-path /dev/zenith/Shell --method dev.zenith.Shell.ToggleNetwork"), {
	description = "Network & Wi-Fi Manager (nmcli)",
})

-- Super + Ctrl + S -> Share menu (via LocalSend)
hl.bind(MOD .. " + CTRL + S", hl.dsp.exec_cmd("localsend"), {
	description = "Share menu (LocalSend)",
})

-- Super + Ctrl + T -> Activity (btop)
hl.bind(MOD .. " + CTRL + T", hl.dsp.exec_cmd("foot --app-id=btop_float -T 'System Activity' -e btop"), {
	description = "Activity monitor (btop)",
})

-- Super + Ctrl + C -> Capture controls
hl.bind(MOD .. " + CTRL + C", hl.dsp.exec_cmd("$HOME/.config/hypr/scripts/ui/capture_menu.sh"), {
	description = "Capture controls menu",
})

-- Super + Ctrl + O -> Toggle menu
hl.bind(MOD .. " + CTRL + O", hl.dsp.exec_cmd("$HOME/.config/hypr/scripts/ui/toggle_menu.sh"), {
	description = "System toggle menu",
})

-- Super + Ctrl + H -> Hardware reload menu
hl.bind(MOD .. " + CTRL + H", hl.dsp.exec_cmd("$HOME/.config/hypr/scripts/ui/hardware_menu.sh"), {
	description = "Hardware reload menu",
})

-- Super + Ctrl + . -> Transcoding menu
hl.bind(MOD .. " + CTRL + period", hl.dsp.exec_cmd("$HOME/.config/hypr/scripts/ui/transcode_menu.sh"), {
	description = "Transcoding menu",
})

------------------------------------------------------------
-- SCRATCHPADS
------------------------------------------------------------

-- Dropdown terminal
hl.bind(MOD .. " + grave", function()
	hl.dispatch(hl.dsp.workspace.toggle_special("term"))
end, {
	description = "Dropdown terminal",
})
