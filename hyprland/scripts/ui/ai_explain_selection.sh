#!/usr/bin/env bash
set -euo pipefail

# Captures active primary selection (or clipboard) and queries Ollama
SELECTED=$(wl-paste --primary 2>/dev/null || wl-paste 2>/dev/null || true)

if [ -z "$SELECTED" ]; then
    notify-send -i dialog-information "AI Assistant" "Please select/highlight some text or code first."
    exit 0
fi

foot --app-id=ai_assistant_float -T "Zenith AI Code & Text Explainer" -e sh -c '
    printf "\033[1;35m┌──────────────────────────────────────────────┐\033[0m\n"
    printf "\033[1;35m│  🧠 Analyzing selection with Qwen2.5 Coder   │\033[0m\n"
    printf "\033[1;35m└──────────────────────────────────────────────┘\033[0m\n\n"
    wl-paste --primary 2>/dev/null | ai "Explain what this code/text does clearly, summarize key points, and suggest fixes if there are errors:"
    printf "\n\033[1;36m[Press Enter or Ctrl+C to close]\033[0m"
    read -r _
'
