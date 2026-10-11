#!/usr/bin/env bash

export PATH="/usr/local/bin:/usr/bin:/bin"

# ---- Count updates ----

pac=$(checkupdates 2>/dev/null | wc -l)

if command -v yay >/dev/null; then
  aur=$(yay -Qua 2>/dev/null | wc -l)
else
  aur=0
fi

total=$((pac + aur))

# ---- Run mode ----

if [ "$1" = "run" ]; then
  /usr/bin/kitty -e bash -c '
echo "Updating system..."
echo ""

sudo pacman -Syu

if command -v yay >/dev/null; then
yay -Sua
fi

echo ""
echo "Done. Press ENTER to close."
read
'
  exit 0
fi

# ---- Waybar output ----

echo " $total"
