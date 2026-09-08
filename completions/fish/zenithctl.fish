# Fish completion for zenithctl & zenithshell
# Dynamically queries available themes and commands

function __zenithctl_themes
    zenithctl __complete themes 2>/dev/null
end

function __zenithctl_modules
    zenithctl __complete modules 2>/dev/null
end

for cmd in zenithctl zenithshell
    complete -c $cmd -f

    # --- Top-Level Commands ---
    complete -c $cmd -n "__fish_use_subcommand" -a wallpaper -d "Wallpaper management & 60fps crossfade"
    complete -c $cmd -n "__fish_use_subcommand" -a wp -d "Wallpaper management (shorthand)"
    complete -c $cmd -n "__fish_use_subcommand" -a theme -d "Theming engine & color extraction"
    complete -c $cmd -n "__fish_use_subcommand" -a brightness -d "Hardware screen backlight control"
    complete -c $cmd -n "__fish_use_subcommand" -a bri -d "Hardware screen backlight control (shorthand)"
    complete -c $cmd -n "__fish_use_subcommand" -a volume -d "Audio volume & PipeWire control"
    complete -c $cmd -n "__fish_use_subcommand" -a vol -d "Audio volume (shorthand)"
    complete -c $cmd -n "__fish_use_subcommand" -a mic -d "Microphone mute toggle"
    complete -c $cmd -n "__fish_use_subcommand" -a clipboard -d "Clipboard history & overlay"
    complete -c $cmd -n "__fish_use_subcommand" -a clip -d "Clipboard history (shorthand)"
    complete -c $cmd -n "__fish_use_subcommand" -a toggle -d "Toggle shell modules & overlays"
    complete -c $cmd -n "__fish_use_subcommand" -a bar -d "Toggle top status bar"
    complete -c $cmd -n "__fish_use_subcommand" -a launcher -d "Toggle Spotlight app launcher"
    complete -c $cmd -n "__fish_use_subcommand" -a spotlight -d "Toggle Spotlight app launcher"
    complete -c $cmd -n "__fish_use_subcommand" -a control-center -d "Toggle Quick Settings / Control Center"
    complete -c $cmd -n "__fish_use_subcommand" -a cc -d "Toggle Quick Settings / Control Center (shorthand)"
    complete -c $cmd -n "__fish_use_subcommand" -a notifications -d "Toggle notification history center"
    complete -c $cmd -n "__fish_use_subcommand" -a nc -d "Toggle notification history center (shorthand)"
    complete -c $cmd -n "__fish_use_subcommand" -a reminders -d "Toggle reminders manager"
    complete -c $cmd -n "__fish_use_subcommand" -a active-apps -d "Toggle active applications drawer"
    complete -c $cmd -n "__fish_use_subcommand" -a keybinds -d "Toggle Hyprland cheatsheet overlay"
    complete -c $cmd -n "__fish_use_subcommand" -a network -d "Open Wi-Fi / Network dialog"
    complete -c $cmd -n "__fish_use_subcommand" -a audio -d "Open Audio control modal"
    complete -c $cmd -n "__fish_use_subcommand" -a power -d "Open Power & Session menu"
    complete -c $cmd -n "__fish_use_subcommand" -a stats -d "Display live system performance metrics"
    complete -c $cmd -n "__fish_use_subcommand" -a reload -d "Reload CSS stylesheets and configuration"
    complete -c $cmd -n "__fish_use_subcommand" -a help -d "Show command help manual"
    complete -c $cmd -n "__fish_use_subcommand" -a version -d "Show version"

    # --- Wallpaper Subcommands ---
    complete -c $cmd -n "__fish_seen_subcommand_from wallpaper wp" -a cycle -d "Cycle to next wallpaper"
    complete -c $cmd -n "__fish_seen_subcommand_from wallpaper wp" -a next -d "Cycle to next wallpaper"
    complete -c $cmd -n "__fish_seen_subcommand_from wallpaper wp" -a current -d "Show current wallpaper path"
    complete -c $cmd -n "__fish_seen_subcommand_from wallpaper wp" -a set -d "Set desktop wallpaper" -F
    complete -c $cmd -n "__fish_seen_subcommand_from wallpaper wp" -a dir -d "Set wallpaper directory" -a "(__fish_complete_directories)"

    # --- Theme Subcommands ---
    complete -c $cmd -n "__fish_seen_subcommand_from theme" -a set -d "Apply theme"
    complete -c $cmd -n "__fish_seen_subcommand_from theme" -a next -d "Cycle to next theme"
    complete -c $cmd -n "__fish_seen_subcommand_from theme" -a current -d "Show active theme"
    complete -c $cmd -n "__fish_seen_subcommand_from theme" -a list -d "List available themes"
    complete -c $cmd -n "__fish_seen_subcommand_from theme; and __fish_seen_subcommand_from set" -a "(__zenithctl_themes)"
    complete -c $cmd -n "__fish_seen_subcommand_from theme; and not __fish_seen_subcommand_from set next current list" -a "(__zenithctl_themes)"

    # --- Brightness Subcommands ---
    complete -c $cmd -n "__fish_seen_subcommand_from brightness bri" -a set -d "Set brightness percentage"
    complete -c $cmd -n "__fish_seen_subcommand_from brightness bri" -a up -d "Increase brightness"
    complete -c $cmd -n "__fish_seen_subcommand_from brightness bri" -a down -d "Decrease brightness"
    complete -c $cmd -n "__fish_seen_subcommand_from brightness bri" -a current -d "Print current brightness percentage"

    # --- Volume Subcommands ---
    complete -c $cmd -n "__fish_seen_subcommand_from volume vol" -a set -d "Set volume percentage"
    complete -c $cmd -n "__fish_seen_subcommand_from volume vol" -a up -d "Increase volume"
    complete -c $cmd -n "__fish_seen_subcommand_from volume vol" -a down -d "Decrease volume"
    complete -c $cmd -n "__fish_seen_subcommand_from volume vol" -a mute -d "Toggle audio mute"
    complete -c $cmd -n "__fish_seen_subcommand_from volume vol" -a current -d "Print current volume percentage"

    # --- Mic Subcommands ---
    complete -c $cmd -n "__fish_seen_subcommand_from mic" -a mute -d "Toggle microphone mute"

    # --- Clipboard Subcommands ---
    complete -c $cmd -n "__fish_seen_subcommand_from clipboard clip" -a clear -d "Wipe clipboard history"
    complete -c $cmd -n "__fish_seen_subcommand_from clipboard clip" -a store -d "Save stdin to clipboard history"

    # --- Toggle Subcommands ---
    complete -c $cmd -n "__fish_seen_subcommand_from toggle" -a "(__zenithctl_modules)"

    # --- Stats Options ---
    complete -c $cmd -n "__fish_seen_subcommand_from stats" -l json -d "Output raw JSON metrics"
end
