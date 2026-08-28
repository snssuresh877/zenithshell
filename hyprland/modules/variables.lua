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

	wallpaper = "awww",
}

------------------------------------------------------------
-- LAUNCHERS
------------------------------------------------------------

Launchers = {

	--------------------------------------------------------
	-- ZENITHSHELL (SPOTLIGHT + CLIPBOARD + NOTIFICATIONS)
	--------------------------------------------------------

	zenithshell = {
		app = "$HOME/.config/hypr/scripts/ui/open_launcher.sh",
		run = "$HOME/.config/hypr/scripts/ui/open_launcher.sh",
		window = "$HOME/.config/hypr/scripts/ui/open_launcher.sh",
		clipboard = "$HOME/.config/hypr/scripts/ui/open_clipboard.sh",
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

	power = "wlogout",
}
------------------------------------------------------------
-- NOTIFICATIONS
------------------------------------------------------------

Notifications = {

	daemon = "swaync",

	toggleDND = "swaync-client -d -sw",

	dismissAll = "swaync-client -C",

	restore = "swaync-client -t -sw",
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

	directory = "$HOME/Pictures/wallpapers/resized",

	script = "$HOME/.config/hypr/scripts/ui/switch_wallpaper.sh",

	pywalCache = "$HOME/.cache/wal",
}

------------------------------------------------------------
-- WAYBAR
------------------------------------------------------------

Waybar = {
	toggle = "sh -c 'if pgrep -x waybar >/dev/null; then pkill -x waybar; else waybar >/dev/null 2>&1 & fi'",
}

------------------------------------------------------------
-- SESSION
------------------------------------------------------------

Session = {

	lock = "$HOME/.config/hypr/scripts/ui/lock_screen.sh",

	logout = "$HOME/.config/hypr/scripts/ui/logout_session.sh",

	suspend = "loginctl lock-session; systemctl suspend",

	reboot = "$HOME/.config/hypr/scripts/ui/reboot_system.sh",

	shutdown = "$HOME/.config/hypr/scripts/ui/power_off.sh",
}

------------------------------------------------------------
-- MEDIA
------------------------------------------------------------

Media = {

	playPause = "playerctl play-pause",

	next = "playerctl next",

	previous = "playerctl previous",
}

------------------------------------------------------------
-- AUDIO
------------------------------------------------------------

Audio = {

	volumeUp = "wpctl set-volume -l 1 @DEFAULT_AUDIO_SINK@ 5%+",

	volumeDown = "wpctl set-volume @DEFAULT_AUDIO_SINK@ 5%-",

	mute = "wpctl set-mute @DEFAULT_AUDIO_SINK@ toggle",

	micMute = "wpctl set-mute @DEFAULT_AUDIO_SOURCE@ toggle",
}

------------------------------------------------------------
-- BRIGHTNESS
------------------------------------------------------------

Brightness = {

	up = "brightnessctl set 5%+",

	down = "brightnessctl set 5%-",
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

	network = "nm-connection-editor",
}

------------------------------------------------------------
-- HYPRLAND
------------------------------------------------------------

Hyprland = {

	reload = "hyprctl reload",

	exit = "hyprctl dispatch exit",
}

------------------------------------------------------------
-- CURSOR
------------------------------------------------------------

Cursor = {

	theme = "Bibata-Modern-Ice",

	size = 24,
}
