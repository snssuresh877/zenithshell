-- ~/.config/hypr/modules/variables.lua
--
-- Central Hyprland variables
--
-- Design:
--   • Namespaced configuration
--   • Applications in Apps
--   • Launchers in Launcher
--   • UI components in UI
--   • Commands grouped by function
--   • Pywal remains the source of dynamic colors
--
-- Other modules should consume these values.
-- This file contains configuration values only.

------------------------------------------------------------
-- MOD KEY
------------------------------------------------------------

MOD = "SUPER"

------------------------------------------------------------
-- APPLICATIONS
------------------------------------------------------------

Apps = {

	-- Core applications
	terminal = "foot",

	secondTerminal = "kitty",

	browser = "firefox",

	fileManager = "cosmic-files",

	codeEditor = "foot -e nvim",

	textEditor = "foot -e nvim",

	officeSoftware = "onlyoffice",

	settingsApp = "nwg-look",

	volumeMixer = "pavucontrol",

	taskManager = "foot -e btop",

	-- Productivity
	notesApp = "obsidian",

	taskApp = "flatpak run com.super_productivity.SuperProductivity",

	-- Communication
	wechatApp = "flatpak run com.tencent.WeChat",
}

------------------------------------------------------------
-- UI BACKENDS
------------------------------------------------------------

UI = {

	launcher = "zenithshell",

	-- Keep Rofi available for menus that still use it.
	rofi = "rofi",

	bar = "zenithshell",

	notifications = "zenithshell",

	wallpaper = "zenithshell",
}

------------------------------------------------------------
-- LAUNCHERS
------------------------------------------------------------

Launchers = {

	--------------------------------------------------------
	-- ZENITHSHELL (NATIVE SPOTLIGHT, CLIPBOARD, CONTROL CENTER, & KEYBINDS)
	--------------------------------------------------------

	zenithshell = {
		app = "zenithctl launcher",
		run = "zenithctl launcher",
		window = "zenithctl launcher",
		clipboard = "zenithctl clipboard",
		controlCenter = "zenithctl cc",
		keybinds = "zenithctl keybinds",
		network = "zenithctl network",
		reminders = "zenithctl reminders",
		activeApps = "zenithctl active-apps",
	},

	--------------------------------------------------------
	-- ROFI
	--------------------------------------------------------

	rofi = {
		app = "rofi -show drun",
		run = "rofi -show run",
		window = "rofi -show window",
		clipboard = "cliphist list | rofi -dmenu | cliphist decode | wl-copy",
		emoji = "rofimoji",
	},

	--------------------------------------------------------
	-- POWER
	--------------------------------------------------------

	power = "zenithctl power 2>/dev/null || wlogout",
}
------------------------------------------------------------
-- NOTIFICATIONS
------------------------------------------------------------

Notifications = {

	daemon = "zenithshell",

	toggleDND = "zenithctl dnd toggle 2>/dev/null || swaync-client -d -sw",

	dismissAll = "zenithctl nc clear 2>/dev/null || swaync-client -C",

	restore = "zenithctl nc 2>/dev/null || swaync-client -t -sw",
}

------------------------------------------------------------
-- SCREENSHOTS
------------------------------------------------------------

Screenshots = {

	region = "hyprshot --freeze --clipboard-only --mode region --silent",

	fullscreen = "grim - | wl-copy",

	save = "grim ~/Pictures/Screenshots/Screenshot-$(date +%F-%T).png",
}

------------------------------------------------------------
-- WALLPAPER
------------------------------------------------------------

Wallpaper = {

	directory = "$HOME/Pictures/wallpapers",

	script = "zenithctl wallpaper cycle",

	pywalCache = "$HOME/.cache/wal",
}

------------------------------------------------------------
-- WAYBAR
------------------------------------------------------------

Waybar = {
	toggle = "zenithctl bar 2>/dev/null || sh -c 'if pgrep -x waybar >/dev/null; then pkill -x waybar; else waybar >/dev/null 2>&1 & fi'",
}

------------------------------------------------------------
-- SESSION
------------------------------------------------------------

Session = {

	lock = "$HOME/.config/hypr/scripts/ui/lock_screen.sh",

	logout = "zenithctl power 2>/dev/null || $HOME/.config/hypr/scripts/ui/logout_session.sh",

	suspend = "loginctl lock-session; systemctl suspend",

	reboot = "$HOME/.config/hypr/scripts/ui/reboot_system.sh",

	shutdown = "$HOME/.config/hypr/scripts/ui/power_off.sh",
}

------------------------------------------------------------
-- MEDIA
------------------------------------------------------------

Media = {

	playPause = "zenithctl media play-pause 2>/dev/null || playerctl play-pause",

	next = "zenithctl media next 2>/dev/null || playerctl next",

	previous = "zenithctl media prev 2>/dev/null || playerctl previous",
}

------------------------------------------------------------
-- AUDIO
------------------------------------------------------------

Audio = {

	volumeUp = "zenithctl vol +5 2>/dev/null || wpctl set-volume -l 1 @DEFAULT_AUDIO_SINK@ 5%+",

	volumeDown = "zenithctl vol -5 2>/dev/null || wpctl set-volume @DEFAULT_AUDIO_SINK@ 5%-",

	mute = "zenithctl vol mute 2>/dev/null || wpctl set-mute @DEFAULT_AUDIO_SINK@ toggle",

	micMute = "zenithctl mic mute 2>/dev/null || wpctl set-mute @DEFAULT_AUDIO_SOURCE@ toggle",
}

------------------------------------------------------------
-- BRIGHTNESS
------------------------------------------------------------

Brightness = {

	up = "zenithctl bri +5 2>/dev/null || brightnessctl set 5%+",

	down = "zenithctl bri -5 2>/dev/null || brightnessctl set 5%-",
}

------------------------------------------------------------
-- SYSTEM
------------------------------------------------------------

System = {

	update = "$HOME/.config/hypr/scripts/system/system.sh",

	clean = "$HOME/.config/hypr/scripts/system/clean.sh",

	memory = "$HOME/.config/hypr/scripts/system/optimize-memory.sh",

	monitor = "foot -e btop",

	bluetooth = "blueman-manager",

	network = "zenithctl network 2>/dev/null || nm-connection-editor",
}

------------------------------------------------------------
-- HYPRLAND
------------------------------------------------------------

Hyprland = {

	reload = "hyprctl reload",

	exit = "zenithctl power 2>/dev/null || hyprctl dispatch exit",
}

------------------------------------------------------------
-- CURSOR
------------------------------------------------------------

Cursor = {

	theme = "Bibata-Modern-Ice",

	size = 24,
}
