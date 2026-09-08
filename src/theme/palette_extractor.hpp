#pragma once

#include "theme/theme.hpp"
#include <string>
#include <vector>
#include <optional>

namespace zenith {

struct ExtractedPalette {
    std::string wallpaper_path;
    std::string background;
    std::string foreground;
    std::string accent;
    std::string accent_secondary;
    std::vector<std::string> colors; // 16 ANSI colors
};

class PaletteExtractor {
public:
    // Extracts dynamic Theme and writes ~/.cache/wal compatibility files
    static std::optional<Theme> extract_from_image(const std::string& image_path);

    // Raw palette extraction logic
    static std::optional<ExtractedPalette> extract_raw_palette(const std::string& image_path);

    // Write out standard ~/.cache/wal cache files
    static bool emit_wal_cache(const ExtractedPalette& palette);
};

} // namespace zenith
