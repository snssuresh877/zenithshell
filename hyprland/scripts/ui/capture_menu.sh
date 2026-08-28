#!/bin/bash
pgrep -x rofi >/dev/null && exit 0

choice=$(printf \
"󰹑 Active Window Screenshot
󰄀 Selected Area Screenshot
󰍹 Full Display Screenshot
󰑋 Record Screen (Video)
󰩐 Extract Text from Region (OCR)
󰈊 Color Picker
󰄵 Scan QR Code from Screen" | rofi -dmenu -i -p "Capture")

[ -z "$choice" ] && exit 0

case "$choice" in
  *Active\ Window*)
    DIR="$HOME/Pictures/Screenshots"
    mkdir -p "$DIR"
    FILE="$DIR/window_$(date +%Y-%m-%d_%H-%M-%S).png"
    GEOM=$(hyprctl activewindow -j | jq -r '"\(.at[0]),\(.at[1]) \(.size[0])x\(.size[1])"')
    grim -g "$GEOM" "$FILE" && wl-copy < "$FILE" && notify-send "Screenshot" "Window screenshot saved & copied"
    ;;
  *Selected\ Area*)
    DIR="$HOME/Pictures/Screenshots"
    mkdir -p "$DIR"
    FILE="$DIR/area_$(date +%Y-%m-%d_%H-%M-%S).png"
    grim -g "$(slurp)" "$FILE" && wl-copy < "$FILE" && notify-send "Screenshot" "Area screenshot saved & copied"
    ;;
  *Full\ Display*)
    DIR="$HOME/Pictures/Screenshots"
    mkdir -p "$DIR"
    FILE="$DIR/screen_$(date +%Y-%m-%d_%H-%M-%S).png"
    grim "$FILE" && wl-copy < "$FILE" && notify-send "Screenshot" "Full screenshot saved & copied"
    ;;
  *Record\ Screen*)
    if command -v wf-recorder >/dev/null 2>&1; then
      if pgrep -x wf-recorder >/dev/null; then
        pkill -INT -x wf-recorder
        notify-send "Screen Recording" "Recording saved to ~/Videos"
      else
        mkdir -p "$HOME/Videos"
        wf-recorder -f "$HOME/Videos/recording_$(date +%Y-%m-%d_%H-%M-%S).mp4" &
        notify-send "Screen Recording" "Recording started... (click again to stop)"
      fi
    else
      notify-send "Screen Recording" "wf-recorder not installed"
    fi
    ;;
  *Extract\ Text*)
    grim -g "$(slurp)" - | tesseract stdin stdout -l eng 2>/dev/null | wl-copy && notify-send "OCR" "Extracted text copied to clipboard"
    ;;
  *Color\ Picker*)
    COLOR=$(hyprpicker -a)
    wl-copy <<< "$COLOR" && notify-send "Color Picker" "Color copied: $COLOR"
    ;;
  *Scan\ QR*)
    grim -g "$(slurp)" - | zbarimg --raw - 2>/dev/null | wl-copy && notify-send "QR Code" "Decoded content copied to clipboard"
    ;;
esac
