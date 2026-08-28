-- ~/.config/hypr/modules/input.lua
--
-- Hyprland 0.55+ input configuration
--
-- Laptop touchpad & keyboard:
--   Fast responsive typing (repeat rate 40, delay 250)
--   Natural scrolling & tap-to-click
--   Two-finger right click & three-finger middle click
--   Disable while typing

hl.config({
	input = {

		----------------------------------------------------
		-- KEYBOARD
		----------------------------------------------------

		kb_layout = "us",
		repeat_rate = 40,
		repeat_delay = 250,
		numlock_by_default = true,

		----------------------------------------------------
		-- POINTER
		----------------------------------------------------

		follow_mouse = 1,
		sensitivity = 0.0,

		----------------------------------------------------
		-- TOUCHPAD
		----------------------------------------------------

		touchpad = {

			-- Mac-style natural scrolling.
			natural_scroll = true,

			-- Tap anywhere to left-click.
			tap_to_click = true,

			-- Tap and drag to move windows/text.
			tap_and_drag = true,

			-- Don't lock dragging after the initial click.
			drag_lock = false,

			-- Prevent accidental cursor movement while typing.
			disable_while_typing = true,

			-- Two-finger click = right click.
			clickfinger_behavior = true,

			-- Smooth controlled scrolling.
			scroll_factor = 0.85,

			-- Middle button click emulation (3-finger tap).
			middle_button_emulation = true,
		},
	},
})
