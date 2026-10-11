-- ~/.config/hypr/modules/workspaces.lua
--
-- Hyprland 0.55+ Lua workspace configuration
--
-- Laptop:
--   Monitor: eDP-1
--   Main workspaces: 1-5
--   Scratchpads: magic / term / system / files / audio
--
-- Design:
--   1-5     -> persistent main workspaces
--   special -> dedicated utility spaces
--   scratchpads use larger outer gaps for a floating/panel feel

------------------------------------------------------------
-- MAIN WORKSPACES
------------------------------------------------------------

-- Workspace 1
hl.workspace_rule({
	workspace = "1",

	monitor = "eDP-1",
	default = true,
	persistent = true,
})

-- Workspace 2
hl.workspace_rule({
	workspace = "2",

	monitor = "eDP-1",
	persistent = true,
})

-- Workspace 3
hl.workspace_rule({
	workspace = "3",

	monitor = "eDP-1",
	persistent = true,
})

-- Workspace 4
hl.workspace_rule({
	workspace = "4",

	monitor = "eDP-1",
	persistent = true,
})

-- Workspace 5
hl.workspace_rule({
	workspace = "5",

	monitor = "eDP-1",
	persistent = true,
})

------------------------------------------------------------
-- SPECIAL WORKSPACES / SCRATCHPADS
------------------------------------------------------------

-- Magic scratchpad
hl.workspace_rule({
	workspace = "special:magic",

	gaps_out = 40,
	gaps_in = 20,
})

-- Dropdown terminal
hl.workspace_rule({
	workspace = "special:term",

	gaps_out = 40,
	gaps_in = 20,
})

-- System monitor / btop
hl.workspace_rule({
	workspace = "special:system",

	gaps_out = 50,
	gaps_in = 25,
})

-- File manager scratchpad
hl.workspace_rule({
	workspace = "special:files",

	gaps_out = 30,
	gaps_in = 20,
})

-- Audio control
hl.workspace_rule({
	workspace = "special:audio",

	gaps_out = 80,
	gaps_in = 40,
})
