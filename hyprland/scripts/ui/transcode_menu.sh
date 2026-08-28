#!/usr/bin/env bash
if command -v ffmpeg >/dev/null 2>&1; then
  notify-send "Transcode" "FFmpeg is available for media transcoding"
else
  notify-send "Transcode" "ffmpeg not installed"
fi
