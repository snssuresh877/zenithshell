-- ~/.config/hypr/modules/layouts.lua
--
-- Hyprland 0.55+ layout configuration
--
-- Primary layout:
--   Dwindle
--
-- Secondary layout:
--   Master
--
-- Laptop focused:
--   smart splits
--   predictable resizing
--   active-window-aware placement

------------------------------------------------------------
-- DWINDLE
------------------------------------------------------------

hl.config({
	dwindle = {

		-- Preserve the existing split direction.
		preserve_split = true,

		-- Automatically choose a sensible split direction.
		smart_split = true,

		-- Intelligent mouse resizing.
		smart_resizing = true,

		-- Favor wider horizontal splits on the laptop display.
		split_width_multiplier = 1.4,

		-- Use the active window when creating a new split.
		use_active_for_splits = true,

		-- Slightly favor the larger/main window.
		default_split_ratio = 1.2,
	},

	--------------------------------------------------------
	-- MASTER
	--------------------------------------------------------

	master = {

		-- New windows enter the slave stack.
		new_status = "slave",

		-- Master occupies approximately 60%.
		mfact = 0.60,

		-- Intelligent resizing.
		smart_resizing = true,

		-- Preserve normal slave ordering.
		new_on_top = false,

		-- Master on the left.
		orientation = "left",
	},
})
