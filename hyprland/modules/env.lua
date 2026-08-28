-- ~/.config/hypr/modules/env.lua
--
-- System Environment Configuration for Wayland, GTK, Qt, Electron & Office Suites

hl.config({
	env = {
		----------------------------------------------------
		-- DESKTOP / SESSION
		----------------------------------------------------

		"XDG_CURRENT_DESKTOP,Hyprland",
		"XDG_SESSION_TYPE,wayland",
		"XDG_SESSION_DESKTOP,Hyprland",

		----------------------------------------------------
		-- QT
		----------------------------------------------------

		"QT_QPA_PLATFORM,wayland;xcb",
		"QT_QPA_PLATFORMTHEME,gtk3",
		"QT_WAYLAND_DISABLE_WINDOWDECORATION,1",

		----------------------------------------------------
		-- ELECTRON & CHROMIUM (ONLYOFFICE, VSCODE, SLACK)
		----------------------------------------------------

		"ELECTRON_OZONE_PLATFORM_HINT,wayland",
		"OZONE_PLATFORM,wayland",

		----------------------------------------------------
		-- FIREFOX
		----------------------------------------------------

		"MOZ_ENABLE_WAYLAND,1",

		----------------------------------------------------
		-- GTK, ICONS & OFFICE SUITES (LIBREOFFICE)
		----------------------------------------------------

		"GDK_BACKEND,wayland,x11,*",
		"XDG_ICON_THEME,Papirus-Dark",
		"SAL_USE_VCLPLUGIN,gtk3",

		----------------------------------------------------
		-- SDL & GAMES
		----------------------------------------------------

		"SDL_VIDEODRIVER,wayland",

		----------------------------------------------------
		-- CURSOR
		----------------------------------------------------

		"XCURSOR_THEME,Bibata-Modern-Ice",
		"XCURSOR_SIZE,24",
	},
})

-- Direct compositor runtime environment export (if supported by hyprlua)
if hl.env then
	hl.env("SAL_USE_VCLPLUGIN", "gtk3")
	hl.env("ELECTRON_OZONE_PLATFORM_HINT", "wayland")
	hl.env("OZONE_PLATFORM", "wayland")
	hl.env("QT_QPA_PLATFORM", "wayland;xcb")
	hl.env("QT_QPA_PLATFORMTHEME", "gtk3")
end
