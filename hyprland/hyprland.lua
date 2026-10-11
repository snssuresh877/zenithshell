-- ~/.config/hypr/hyprland.lua

------------------------
-- CORE VARIABLES
------------------------

require("modules.variables")

------------------------
-- HARDWARE
------------------------

require("modules.monitors")
require("modules.input")

------------------------
-- SYSTEM ENVIRONMENT
------------------------

require("modules.env")
require("modules.autostart")

------------------------
-- UI / LOOK
------------------------

require("modules.appearance")
require("modules.animations")
require("modules.layouts")

------------------------
-- WORKSPACES & RULES
------------------------

require("modules.workspaces")
require("modules.rules")

------------------------
-- KEYBINDS
------------------------

require("modules.binds")

------------------------
-- NATIVE HYPRLAND CONFIGS
------------------------

------------------------
-- OPTIONAL MODULES
------------------------

-- require("modules.plugins")
