-- ~/.config/hypr/modules/appearance.lua
--
-- Hyprland 0.55+ appearance & system performance configuration
--
-- Design:
--   Clean, minimal, 14px rounding, small gaps
--   Battery & CPU optimized with VFR (Variable Frame Rate)

hl.config({

	--------------------------------------------------------
	-- GENERAL
	--------------------------------------------------------

	general = {

		-- Compact spacing between tiled windows.
		gaps_in = 3,

		-- Small screen-edge spacing.
		gaps_out = 6,

		-- Thin window border.
		border_size = 2,

		-- Allow resizing directly from window borders.
		resize_on_border = true,

		-- Prevent tearing.
		allow_tearing = false,

		-- Primary tiling layout.
		layout = "dwindle",
	},

	--------------------------------------------------------
	-- DECORATION
	--------------------------------------------------------

	decoration = {

		-- Modern universal corner rounding.
		rounding = 14,

		----------------------------------------------------
		-- BLUR
		----------------------------------------------------

		blur = {
			enabled = true,
			size = 2,
			passes = 1,
		},

		----------------------------------------------------
		-- SHADOW
		----------------------------------------------------

		shadow = {
			enabled = false,
		},
	},

	--------------------------------------------------------
	-- MISCELLANEOUS & BATTERY EFFICIENCY
	--------------------------------------------------------

	misc = {
		-- Disable logos and splash screen
		disable_hyprland_logo = true,
		disable_splash_rendering = true,
		disable_scale_notification = true,

		-- Auto-focus windows that request attention
		focus_on_activate = true,

		-- Power management: wake display on mouse or keypress
		mouse_move_enables_dpms = true,
		key_press_enables_dpms = true,

		-- Allow session lock restore on recovery
		allow_session_lock_restore = true,
	},

	--------------------------------------------------------
	-- XWAYLAND CRISPNESS
	--------------------------------------------------------

	xwayland = {
		force_zero_scaling = true,
	},

	--------------------------------------------------------
	-- CURSOR
	--------------------------------------------------------

	cursor = {
		zoom_factor = 1.0,
		hide_on_key_press = true,
		warp_on_change_workspace = 1,
	},
})
