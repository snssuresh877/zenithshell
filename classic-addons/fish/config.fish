# ~/.config/fish/config.fish
# =========================================================
# 🐟 Zenith Pro Workstation Fish Configuration
# Fast • Modern • Wayland Native • Universal PATHs
# =========================================================

# Exit if not interactive
status is-interactive || exit

# =========================================================
# 🌐 ENVIRONMENT & DEFAULT TOOLS
# =========================================================

set -gx EDITOR nvim
set -gx VISUAL nvim
set -gx LESS '-R -F -X'

# Set universal application discovery directories (XDG / Flatpak)
set -gx XDG_DATA_DIRS "$HOME/.local/share/flatpak/exports/share:/var/lib/flatpak/exports/share:/usr/local/share:/usr/share"

# =========================================================
# 🚀 UNIVERSAL PATH SYNCHRONIZATION
# =========================================================

# Local user binaries & packages
test -d $HOME/.local/bin; and fish_add_path -m $HOME/.local/bin
test -d $HOME/.cargo/bin; and fish_add_path -m $HOME/.cargo/bin
test -d $HOME/.local/share/nvim/mason/bin; and fish_add_path -m $HOME/.local/share/nvim/mason/bin
test -d $HOME/.local/share/flatpak/exports/bin; and fish_add_path -m $HOME/.local/share/flatpak/exports/bin
test -d /var/lib/flatpak/exports/bin; and fish_add_path -m /var/lib/flatpak/exports/bin
test -d /usr/lib/rustup/bin; and fish_add_path -m /usr/lib/rustup/bin
test -d /usr/local/sbin; and fish_add_path -m /usr/local/sbin
test -d /usr/local/bin; and fish_add_path -m /usr/local/bin
test -d /usr/bin; and fish_add_path -m /usr/bin

# Android SDK (if present)
if test -d /opt/android-sdk
    set -gx ANDROID_HOME /opt/android-sdk
    set -gx ANDROID_SDK_ROOT /opt/android-sdk
    fish_add_path -m /opt/android-sdk/platform-tools
    fish_add_path -m /opt/android-sdk/cmdline-tools/latest/bin
end

# Go binaries (if present)
if test -d $HOME/go/bin
    fish_add_path -m $HOME/go/bin
end

# Node/NPM global binaries (if present)
if test -d $HOME/.npm-global/bin
    fish_add_path -m $HOME/.npm-global/bin
end

# =========================================================
# 🎨 PYWAL DYNAMIC COLOR SEQUENCES
# =========================================================

if test -f $HOME/.cache/wal/sequences
    cat $HOME/.cache/wal/sequences 2>/dev/null
end

# =========================================================
# 🛠️ PROMPT & NAVIGATION HELPERS
# =========================================================

# Starship prompt
if type -q starship
    starship init fish | source
end

# Zoxide smart directory jumper
if type -q zoxide
    zoxide init fish | source
end

# FZF file search
if type -q fd
    set -gx FZF_DEFAULT_COMMAND 'fd --type f --hidden --exclude .git'
    set -gx FZF_CTRL_T_COMMAND "$FZF_DEFAULT_COMMAND"
end

# =========================================================
# 📂 FILE LISTING & SEARCH ALIASES
# =========================================================

if type -q eza
    alias ls='eza --icons --group-directories-first'
    alias ll='eza -lah --icons --group-directories-first'
    alias la='eza -A --icons'
    alias tree='eza --tree --icons'
else
    alias ls='ls --color=auto -hF'
    alias ll='ls -lah'
    alias la='ls -A'
end

# Directory navigation
alias ..='cd ..'
alias ...='cd ../..'
alias ....='cd ../../..'
alias home='cd ~'
alias dl='cd ~/Downloads'
alias docs='cd ~/Documents'
alias proj='cd ~/Projects'
alias cfg='cd ~/.config'

# Search aliases
alias ff='fd --type f | fzf'
alias fs='rg'
alias frp="rg --column --line-number --no-heading --color=always --smart-case -- . | fzf --ansi --delimiter : --preview 'bat --style=numbers --color=always --highlight-line {2} {1}'"

# =========================================================
# 🌌 ZENITHSHELL & DESKTOP CONTROLS
# =========================================================

alias zenith='$HOME/.local/bin/zenithshell &'
alias zenith-restart='pkill -9 -f zenithshell; and sleep 0.4; and setsid $HOME/.local/bin/zenithshell >/tmp/zenithshell.log 2>&1 &'
alias zenith-stats='gdbus call --session --dest dev.zenith.Shell --object-path /dev/zenith/Shell --method dev.zenith.Shell.GetStats'
alias zenith-theme='gdbus call --session --dest dev.zenith.Shell --object-path /dev/zenith/Shell --method dev.zenith.Shell.ToggleTheme'
alias zenith-wall='gdbus call --session --dest dev.zenith.Shell --object-path /dev/zenith/Shell --method dev.zenith.Shell.NextWallpaper'
alias yazi='yazi'

# =========================================================
# 📦 GIT & SYSTEM SHORTCUTS
# =========================================================

alias gs='git status'
alias ga='git add .'
alias gc='git commit -m'
alias gacp='git add .; and git commit -m'
alias gp='git push'
alias gl='git pull'
alias glog='git log --oneline --graph --decorate'

alias update='sudo pacman -Syu; and flatpak update -y'
alias clean-cache='sudo paccache -rk2'
alias backup='sudo timeshift --create --comments "manual backup" --tags D'

alias cpu='htop'
alias mem='free -h'
alias disk='df -h'
alias temp='sensors'

alias batt='upower -i (upower -e | grep BAT | head -n1)'
alias battery='sudo tlp-stat -b'

# Quick configuration edits
alias fishconf='$EDITOR ~/.config/fish/config.fish'
alias hyprconf='$EDITOR ~/.config/hypr/hyprland.lua'
alias kittyconf='$EDITOR ~/.config/kitty/kitty.conf'
alias footconf='$EDITOR ~/.config/foot/foot.ini'
alias reload='exec fish'

# Safety
alias rm='rm -i'
alias cp='cp -i'
alias mv='mv -i'
