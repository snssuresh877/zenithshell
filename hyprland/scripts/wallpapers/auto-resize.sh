#!/usr/bin/env bash

SOURCE="$HOME/Pictures/wallpapers"
OUTPUT="$HOME/Pictures/wallpapers/resized"

mkdir -p "$OUTPUT"

echo "Watching wallpapers folder..."

inotifywait -m -e create -e moved_to "$SOURCE" |
while read -r directory event file; do

    case "$file" in
        *.jpg|*.jpeg|*.png|*.webp|*.JPG|*.JPEG|*.PNG|*.WEBP)

            INPUT="$SOURCE/$file"
            OUT="$OUTPUT/$file"

            echo "Optimizing: $file"

            magick "$INPUT" \
                -resize 1920x1080\> \
                -strip \
                -quality 88 \
                "$OUT"

            echo "Saved -> $OUT"
            ;;
    esac

done
