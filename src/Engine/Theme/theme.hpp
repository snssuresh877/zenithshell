#pragma once

#include <string>

namespace zenith {

struct Theme {
    std::string name = "zenith-dark";

    // Surfaces & Backgrounds (Theme Controlled)
    std::string background = "#0e0f14";
    std::string surface = "#161720";
    std::string surface_variant = "#1e202c";
    std::string border = "rgba(255, 255, 255, 0.08)";

    // 3 Strict Hierarchy Text Levels (Theme / Contrast Controlled)
    std::string text_primary = "#F2F2F5";
    std::string text_secondary = "#9A9AAF";
    std::string text_disabled = "#5F6070";

    // Accent (Personality & Interaction)
    std::string accent = "#A875FF";
    std::string accent_secondary = "#C084FC";

    // FIXED Semantic Colors (Constant across all themes - NEVER overridden)
    std::string semantic_success = "#35D07F";
    std::string semantic_warning = "#FFB454";
    std::string semantic_error = "#FF5C6C";
    std::string semantic_info = "#55B9FF";

    // Legacy compatibility fields
    std::string text = "#F2F2F5";
    std::string text_muted = "#9A9AAF";
    std::string success = "#35D07F";
    std::string warning = "#FFB454";
    std::string error = "#FF5C6C";

    // Geometry & Layout
    int radius_window = 24;
    int radius_card = 18;
    int radius_pill = 16;
    int radius_button = 12;
    int border_size = 1;
    int spacing = 8;
    int bar_height = 36;

    // Typography
    std::string font_family = "JetBrainsMono Nerd Font, Symbols Nerd Font, sans-serif";
    int font_size = 11;
};

} // namespace zenith
