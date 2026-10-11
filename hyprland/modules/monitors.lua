-- ~/.config/hypr/modules/monitors.lua
-- See https://wiki.hypr.land/Configuring/Basics/Monitors/

-- Built-in Laptop Display
hl.monitor({
	output = "eDP-1",
	mode = "preferred",
	position = "auto",
	scale = "1",
})

-- Fallback for any connected external display (HDMI-A-1, DP-1, Type-C DisplayPort)
hl.monitor({
	output = "",
	mode = "preferred",
	position = "auto",
	scale = "auto",
})
