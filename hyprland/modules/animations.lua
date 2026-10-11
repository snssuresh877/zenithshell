-- ~/.config/hypr/modules/animations.lua

hl.config({
	animations = {
		enabled = true,

		bezier = {
			{
				name = "ease",
				points = { 0.25, 0.1, 0.25, 1.0 },
			},
		},

		animation = {
			{
				name = "windows",
				enabled = true,
				speed = 7,
				curve = "ease",
				style = "popin 80%",
			},

			{
				name = "border",
				enabled = true,
				speed = 8,
				curve = "ease",
			},

			{
				name = "fade",
				enabled = true,
				speed = 7,
				curve = "ease",
			},

			{
				name = "workspaces",
				enabled = true,
				speed = 6,
				curve = "ease",
				style = "slide",
			},
		},
	},
})
