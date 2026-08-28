-----------------------
----- PERMISSIONS -----
-----------------------

hl.config({
	ecosystem = {
		enforce_permissions = true,
	},
})

--------------------------------
-- SCREENSHOT TOOLS
--------------------------------

hl.permission(
	"/usr/(bin|local/bin)/grim",
	"screencopy",
	"allow",
)

hl.permission(
	"/usr/(lib|libexec|lib64)/xdg-desktop-portal-hyprland",
	"screencopy",
	"allow",
)

--------------------------------
-- HYPRLAND PLUGINS
--------------------------------

hl.permission(
	"/usr/(bin|local/bin)/hyprpm",
	"plugin",
	"allow",
)

--------------------------------
-- OPTIONAL EXTRA TOOLS
--------------------------------

hl.permission(
	"/usr/(bin|local/bin)/hyprpicker",
	"screencopy",
	"allow",
)
