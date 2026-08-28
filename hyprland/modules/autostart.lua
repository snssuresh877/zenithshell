-- ~/.config/hypr/modules/autostart.lua

------------------------------------------------------------
-- HYPRLAND START
------------------------------------------------------------

hl.on("hyprland.start", function()
	--------------------------------------------------------
	-- DBUS / SYSTEMD ENVIRONMENT
	--------------------------------------------------------

	hl.exec_cmd(
		"dbus-update-activation-environment --systemd "
			.. "WAYLAND_DISPLAY DISPLAY XDG_CURRENT_DESKTOP=Hyprland "
			.. "XDG_SESSION_TYPE XDG_SESSION_DESKTOP XDG_DATA_DIRS SSH_AUTH_SOCK"
	)

	--------------------------------------------------------
	-- FLATPAK DESKTOP APPLICATION INDEXER
	--------------------------------------------------------

	hl.exec_cmd("$HOME/.config/hypr/scripts/ui/sync_flatpak_desktop.sh")

	--------------------------------------------------------
	-- POLKIT
	--------------------------------------------------------

	hl.exec_cmd("pgrep -x polkit-kde-authentication-agent-1 >/dev/null " .. "|| /usr/lib/polkit-kde-agent-1")

	--------------------------------------------------------
	-- WALLPAPER
	--------------------------------------------------------

	hl.exec_cmd("pgrep -x awww-daemon >/dev/null || awww-daemon")

	--------------------------------------------------------
	-- DESKTOP UI SHELL (ZENITHSHELL NATIVE C++20)
	--------------------------------------------------------

	hl.exec_cmd("$HOME/.config/hypr/scripts/ui/autostart_ui.sh &")

	--------------------------------------------------------
	-- SYSTEM UPDATES NOTIFIER
	--------------------------------------------------------

	hl.exec_cmd("$HOME/.config/hypr/scripts/ui/check_updates.sh &")

	--------------------------------------------------------
	-- HYPRIDLE
	--------------------------------------------------------

	hl.exec_cmd("pgrep -x hypridle >/dev/null || hypridle")

	--------------------------------------------------------
	-- NETWORK TRAY (Handled natively by Shell D-Bus)
	--------------------------------------------------------

	-- hl.exec_cmd("pgrep -x nm-applet >/dev/null || nm-applet")

	--------------------------------------------------------
	-- KEYBINDINGS DESKTOP ENTRIES SYNC
	--------------------------------------------------------

	hl.exec_cmd("$HOME/.config/hypr/scripts/ui/sync_keybind_desktop_entries.sh &")

	--------------------------------------------------------
	-- CLIPBOARD HISTORY
	--------------------------------------------------------

	hl.exec_cmd("wl-paste --type text --watch cliphist store &")

	hl.exec_cmd("wl-paste --type image --watch cliphist store &")
end)
