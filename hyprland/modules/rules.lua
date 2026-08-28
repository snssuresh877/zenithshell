-- ~/.config/hypr/modules/rules.lua
--
-- Hyprland 0.55+ window and layer rules
--
-- Design:
--   Global Maximize Suppression -> Keeps all windows strictly within Hyprland tiling & rounding
--   Normal applications         -> tiled with clean 14px rounding
--   Utilities / Popups          -> floating & centered
--   Dialogs / File Pickers      -> floating & centered
--   PIP                         -> floating + pinned
--   Office Suites               -> guaranteed 14px rounding + proper dialog floating

------------------------------------------------------------
-- 00. GLOBAL BEHAVIOR RULES
------------------------------------------------------------

-- Prevent applications (LibreOffice, OnlyOffice, Electron, Chromium) from auto-maximizing to fullscreen monocle
hl.window_rule({
	match = {
		class = ".*",
	},
	suppress_event = "maximize",
})

-- Fix XWayland empty ghost drag-and-drop surfaces
hl.window_rule({
	match = {
		class = "^$",
		title = "^$",
		xwayland = true,
		float = true,
		fullscreen = false,
		pin = false,
	},
	no_focus = true,
})

------------------------------------------------------------
-- 01. SPECIAL WORKSPACES
------------------------------------------------------------

hl.window_rule({
	match = {
		workspace = "special:.*",
	},

	float = true,
	center = true,
	opacity = "1.0 override 0.96 override 1.0 override",
})

------------------------------------------------------------
-- 02. SYSTEM UTILITIES & DIALOGS (FLOATING)
------------------------------------------------------------

-- Bluetooth Manager
hl.window_rule({
	match = {
		class = "^(blueman-manager|blueman-services|blueman-adapters|Blueman-manager)$",
	},

	float = true,
	center = true,
	size = {
		480,
		540,
	},
	rounding = 14,
	opacity = "0.98 override 0.95 override 1.0 override",
})

-- Audio Mixer (Pavucontrol)
hl.window_rule({
	match = {
		class = "^(pavucontrol|org\\.pulseaudio\\.pavucontrol)$",
	},

	float = true,
	center = true,
	size = {
		660,
		490,
	},
	rounding = 14,
})

-- Calculators
hl.window_rule({
	match = {
		class = "^(kcalc|galculator|gnome-calculator|org\\.gnome\\.Calculator|qalculate-gtk)$",
	},

	float = true,
	center = true,
	size = {
		380,
		500,
	},
	rounding = 14,
})

-- System Monitor (Floating mode)
hl.window_rule({
	match = {
		class = "^btop_float$",
	},

	float = true,
	center = true,
	size = {
		"(monitor_w*0.75)",
		"(monitor_h*0.75)",
	},
	rounding = 14,
	opacity = "0.96 override 0.92 override 1.0 override",
})

-- Local AI Assistant Modal (Ollama)
hl.window_rule({
	match = {
		class = "^ai_assistant_float$",
	},

	float = true,
	center = true,
	size = {
		780,
		580,
	},
	rounding = 14,
	opacity = "0.98 override 0.95 override 1.0 override",
})

-- System Controls Floating Modals (wiremix, bluetui, transcode)
hl.window_rule({
	match = {
		class = "^(wiremix_float|bluetui_float|transcode_float)$",
	},

	float = true,
	center = true,
	size = {
		680,
		460,
	},
	rounding = 14,
	opacity = "0.98 override 0.95 override 1.0 override",
})

-- LocalSend Floating Window
hl.window_rule({
	match = {
		class = "^(localsend|org\\.localsend\\.localsend_app)$",
	},

	float = true,
	center = true,
	size = {
		780,
		560,
	},
	rounding = 14,
})

-- Theme Settings (nwg-look)
hl.window_rule({
	match = {
		class = "^nwg-look$",
	},

	float = true,
	center = true,
	size = {
		"(monitor_w*0.82)",
		"(monitor_h*0.82)",
	},
	rounding = 14,
})

-- General File Pickers & Open/Save Dialogs
hl.window_rule({
	match = {
		title = "^(Open File|Save As|Open Folder|File Upload|Choose Files|Export As|Print).*$",
	},

	float = true,
	center = true,
	size = {
		980,
		640,
	},
	rounding = 14,
})

-- XDG Desktop Portal File Chooser
hl.window_rule({
	match = {
		class = "^(xdg-desktop-portal-gtk|xdg-desktop-portal-hyprland)$",
	},

	float = true,
	center = true,
	size = {
		980,
		640,
	},
	rounding = 14,
})

------------------------------------------------------------
-- 03. PICTURE IN PICTURE
------------------------------------------------------------

hl.window_rule({
	match = {
		title = "^([Pp]icture[-\\s]?[Ii]n[-\\s]?[Pp]icture).*$",
	},

	float = true,
	pin = true,
	keep_aspect_ratio = true,
	move = {
		"(monitor_w*0.73)",
		"(monitor_h*0.72)",
	},
	size = {
		"(monitor_w*0.25)",
		"(monitor_h*0.25)",
	},
	rounding = 14,
})

------------------------------------------------------------
-- 04. SOCIAL & PRIVACY APPS
------------------------------------------------------------

-- WeChat
hl.window_rule({
	match = {
		class = "^(com\\.tencent\\.WeChat|wechat|WeChat)$",
	},

	rounding = 14,
	opacity = "1.0 override 0.97 override 1.0 override",
	no_dim = true,
})

-- Proton VPN
hl.window_rule({
	match = {
		class = "^(protonvpn-app|proton\\.vpn\\.app\\.gtk)$",
	},

	float = true,
	center = true,
	size = {
		420,
		620,
	},
	rounding = 14,
})

------------------------------------------------------------
-- 05. FULLSCREEN (GAMES & MEDIA)
------------------------------------------------------------

hl.window_rule({
	match = {
		fullscreen = true,
	},

	rounding = 0,
	opacity = "1.0 override 1.0 override 1.0 override",
})

hl.window_rule({
	match = {
		float = false,
	},

	no_shadow = true,
})

------------------------------------------------------------
-- 06. OFFICE SUITES (LIBREOFFICE & ONLYOFFICE)
------------------------------------------------------------
-- Placed after Fullscreen rule to guarantee 14px rounding!

-- Main Document Windows
hl.window_rule({
	match = {
		class = "^(libreoffice.*|soffice\\.bin|ONLYOFFICE|DesktopEditors|DesktopEditors\\.bin|onlyoffice.*)$",
	},

	rounding = 14,
	opacity = "1.0 override 0.98 override 1.0 override",
	suppress_event = "maximize",
})

-- Office Suite Internal Dialogs & Popups (Properties, Options, Formatting)
hl.window_rule({
	match = {
		class = "^(libreoffice.*|soffice\\.bin|ONLYOFFICE|DesktopEditors)$",
		title = "^(Open.*|Save.*|Export.*|Print.*|Choose.*|Document Properties|Options|Preferences|Page Setup|Paragraph|Character|Format Cells|Insert .*|Special Characters).*$",
	},

	float = true,
	center = true,
	size = {
		880,
		600,
	},
	rounding = 14,
})

------------------------------------------------------------
-- 07. LAYER SHELL BLUR RULES (ZENITHSHELL & WAYBAR)
------------------------------------------------------------

-- ZenithShell Desktop Overlays & Bar
hl.layer_rule({
	match = {
		namespace = "gtk-layer-shell",
	},

	blur = true,
	ignore_alpha = 0.3,
})

-- Waybar fallback
hl.layer_rule({
	match = {
		namespace = "waybar",
	},

	blur = true,
	ignore_alpha = 0.5,
})
