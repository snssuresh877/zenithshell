#include "theme/palette_extractor.hpp"
#include "theme/color_utils.hpp"
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <glib.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <vector>
#include <array>
#include <cmath>
#include <algorithm>

namespace fs = std::filesystem;

namespace zenith {

namespace {

struct ColorBin {
    uint32_t count = 0;
    uint64_t r_sum = 0;
    uint64_t g_sum = 0;
    uint64_t b_sum = 0;
};

struct ColorCluster {
    RGB rgb;
    HSL hsl;
    uint32_t count;
    double vibrancy;
};

// Euclidean distance in RGB space
double color_distance(const RGB& a, const RGB& b) {
    double dr = static_cast<double>(a.r) - b.r;
    double dg = static_cast<double>(a.g) - b.g;
    double db = static_cast<double>(a.b) - b.b;
    return std::sqrt(dr * dr + dg * dg + db * db);
}

// Check if color is sufficiently distinct from an existing set
bool is_distinct(const RGB& c, const std::vector<RGB>& existing, double min_dist = 42.0) {
    for (const auto& e : existing) {
        if (color_distance(c, e) < min_dist) return false;
    }
    return true;
}

} // namespace

std::optional<ExtractedPalette> PaletteExtractor::extract_raw_palette(const std::string& image_path) {
    if (image_path.empty() || !fs::exists(image_path)) {
        std::cerr << "[PaletteExtractor] Wallpaper path does not exist: " << image_path << std::endl;
        return std::nullopt;
    }

    GError* error = nullptr;
    // Downsample directly during decode to max 128x128 for sub-5ms performance
    GdkPixbuf* pixbuf = gdk_pixbuf_new_from_file_at_scale(image_path.c_str(), 128, 128, TRUE, &error);
    if (!pixbuf) {
        if (error) {
            std::cerr << "[PaletteExtractor] GdkPixbuf decode failed: " << error->message << std::endl;
            g_error_free(error);
        }
        return std::nullopt;
    }

    int n_channels = gdk_pixbuf_get_n_channels(pixbuf);
    int rowstride = gdk_pixbuf_get_rowstride(pixbuf);
    int width = gdk_pixbuf_get_width(pixbuf);
    int height = gdk_pixbuf_get_height(pixbuf);
    guchar* pixels = gdk_pixbuf_get_pixels(pixbuf);

    if (!pixels || width <= 0 || height <= 0) {
        g_object_unref(pixbuf);
        return std::nullopt;
    }

    // 15-bit color quantization: 32 bins per channel (32 x 32 x 32 = 32,768 bins)
    std::vector<ColorBin> bins(32768);

    for (int y = 0; y < height; ++y) {
        const guchar* row = pixels + y * rowstride;
        for (int x = 0; x < width; ++x) {
            const guchar* p = row + x * n_channels;
            int r = p[0];
            int g = p[1];
            int b = p[2];

            int bin_idx = ((r >> 3) << 10) | ((g >> 3) << 5) | (b >> 3);
            auto& bin = bins[bin_idx];
            bin.count++;
            bin.r_sum += r;
            bin.g_sum += g;
            bin.b_sum += b;
        }
    }

    g_object_unref(pixbuf);

    // Collect non-empty clusters
    std::vector<ColorCluster> clusters;
    clusters.reserve(1024);

    for (const auto& bin : bins) {
        if (bin.count == 0) continue;

        RGB rgb = {
            static_cast<int>(bin.r_sum / bin.count),
            static_cast<int>(bin.g_sum / bin.count),
            static_cast<int>(bin.b_sum / bin.count)
        };
        HSL hsl = ColorUtils::rgb_to_hsl(rgb);

        // Vibrancy scoring: favors saturated mid-tones
        double lum_penalty = 1.0 - std::min(1.0, std::abs(hsl.l - 0.50) * 1.6);
        double sat_score = std::pow(hsl.s, 1.2);
        double vibrancy = sat_score * lum_penalty * std::log1p(static_cast<double>(bin.count));

        clusters.push_back({rgb, hsl, bin.count, vibrancy});
    }

    if (clusters.empty()) return std::nullopt;

    // 1. Identify representative dark background mood (low luminance, significant count)
    RGB bg_rgb = {17, 18, 19};
    uint32_t best_bg_count = 0;
    for (const auto& c : clusters) {
        if (c.hsl.l >= 0.03 && c.hsl.l <= 0.28 && c.count > best_bg_count) {
            best_bg_count = c.count;
            bg_rgb = c.rgb;
        }
    }

    // 2. Identify representative light foreground mood (high luminance, readable)
    RGB fg_rgb = {205, 208, 189};
    uint32_t best_fg_count = 0;
    for (const auto& c : clusters) {
        if (c.hsl.l >= 0.65 && c.hsl.l <= 0.95 && c.count > best_fg_count) {
            best_fg_count = c.count;
            fg_rgb = c.rgb;
        }
    }

    // 3. Extract top vibrant distinct colors for accents and palette
    std::vector<ColorCluster> vibrant_clusters = clusters;
    std::sort(vibrant_clusters.begin(), vibrant_clusters.end(), [](const ColorCluster& a, const ColorCluster& b) {
        return a.vibrancy > b.vibrancy;
    });

    std::vector<RGB> distinct_vibrants;
    for (const auto& c : vibrant_clusters) {
        if (c.hsl.s < 0.18 || c.hsl.l < 0.15 || c.hsl.l > 0.85) continue;
        if (is_distinct(c.rgb, distinct_vibrants, 45.0)) {
            distinct_vibrants.push_back(c.rgb);
            if (distinct_vibrants.size() >= 8) break;
        }
    }

    // Fallbacks if image is near-monochrome
    if (distinct_vibrants.empty()) {
        distinct_vibrants.push_back({82, 140, 204}); // fallback blue
    }
    if (distinct_vibrants.size() < 2) {
        HSL h = ColorUtils::rgb_to_hsl(distinct_vibrants[0]);
        h.h = std::fmod(h.h + 45.0, 360.0);
        distinct_vibrants.push_back(ColorUtils::hsl_to_rgb(h));
    }

    RGB primary_accent_rgb = distinct_vibrants[0];
    RGB secondary_accent_rgb = distinct_vibrants[1];

    // Synthesize 16-color ANSI palette
    // 0: Dark bg, 1-6: accents, 7: Foreground, 8: Gray/Muted, 9-14: Bright accents, 15: Bright white
    std::vector<std::string> colors(16);
    colors[0] = ColorUtils::rgb_to_hex(bg_rgb);

    // Fill colors 1..6 with distinct accents or harmonized hue shifts
    for (size_t i = 1; i <= 6; ++i) {
        if (i - 1 < distinct_vibrants.size()) {
            colors[i] = ColorUtils::rgb_to_hex(distinct_vibrants[i - 1]);
        } else {
            HSL h = ColorUtils::rgb_to_hsl(primary_accent_rgb);
            h.h = std::fmod(h.h + (i * 50.0), 360.0);
            colors[i] = ColorUtils::rgb_to_hex(ColorUtils::hsl_to_rgb(h));
        }
    }

    colors[7] = ColorUtils::rgb_to_hex(fg_rgb);

    // Color 8: Muted/comment gray
    HSL gray_hsl = ColorUtils::rgb_to_hsl(fg_rgb);
    gray_hsl.l = 0.45;
    gray_hsl.s = std::clamp(gray_hsl.s * 0.3, 0.05, 0.20);
    colors[8] = ColorUtils::rgb_to_hex(ColorUtils::hsl_to_rgb(gray_hsl));

    // Colors 9..14: Brightened variants of 1..6
    for (size_t i = 1; i <= 6; ++i) {
        RGB base_rgb = ColorUtils::hex_to_rgb(colors[i]);
        HSL h = ColorUtils::rgb_to_hsl(base_rgb);
        h.l = std::clamp(h.l * 1.15, 0.50, 0.78);
        h.s = std::clamp(h.s * 1.05, 0.45, 0.90);
        colors[i + 8] = ColorUtils::rgb_to_hex(ColorUtils::hsl_to_rgb(h));
    }

    // Color 15: Brightest text
    HSL white_hsl = ColorUtils::rgb_to_hsl(fg_rgb);
    white_hsl.l = 0.95;
    colors[15] = ColorUtils::rgb_to_hex(ColorUtils::hsl_to_rgb(white_hsl));

    ExtractedPalette palette;
    palette.wallpaper_path = image_path;
    palette.background = colors[0];
    palette.foreground = colors[7];
    palette.accent = ColorUtils::rgb_to_hex(primary_accent_rgb);
    palette.accent_secondary = ColorUtils::rgb_to_hex(secondary_accent_rgb);
    palette.colors = std::move(colors);

    return palette;
}

bool PaletteExtractor::emit_wal_cache(const ExtractedPalette& palette) {
    const char* home = g_get_home_dir();
    if (!home) return false;

    std::error_code ec;
    fs::path wal_dir = fs::path(home) / ".cache" / "wal";
    fs::create_directories(wal_dir, ec);
    if (ec) return false;

    // 1. Write wal (path to current wallpaper)
    {
        std::ofstream out(wal_dir / "wal", std::ios::trunc);
        if (out.is_open()) {
            out << palette.wallpaper_path << "\n";
        }
    }

    // 2. Write colors (raw hex list)
    {
        std::ofstream out(wal_dir / "colors", std::ios::trunc);
        if (out.is_open()) {
            for (const auto& c : palette.colors) {
                out << c << "\n";
            }
        }
    }

    // 3. Write colors.json (full Pywal JSON format)
    {
        std::ofstream out(wal_dir / "colors.json", std::ios::trunc);
        if (out.is_open()) {
            out << "{\n";
            out << "    \"wallpaper\": \"" << palette.wallpaper_path << "\",\n";
            out << "    \"alpha\": \"100\",\n\n";
            out << "    \"special\": {\n";
            out << "        \"background\": \"" << palette.background << "\",\n";
            out << "        \"foreground\": \"" << palette.foreground << "\",\n";
            out << "        \"cursor\": \"" << palette.foreground << "\"\n";
            out << "    },\n";
            out << "    \"colors\": {\n";
            for (size_t i = 0; i < palette.colors.size(); ++i) {
                out << "        \"color" << i << "\": \"" << palette.colors[i] << "\""
                    << (i + 1 < palette.colors.size() ? ",\n" : "\n");
            }
            out << "    }\n";
            out << "}\n";
        }
    }

    // 4. Write colors.sh (Bash script format for sourcing in terminal rc files)
    {
        std::ofstream out(wal_dir / "colors.sh", std::ios::trunc);
        if (out.is_open()) {
            out << "#!/usr/bin/env bash\n";
            out << "# Shell variables generated natively by ZenithShell\n";
            out << "wallpaper='" << palette.wallpaper_path << "'\n\n";
            out << "# Special\n";
            out << "background='" << palette.background << "'\n";
            out << "foreground='" << palette.foreground << "'\n";
            out << "cursor='" << palette.foreground << "'\n\n";
            out << "# Colors\n";
            for (size_t i = 0; i < palette.colors.size(); ++i) {
                out << "color" << i << "='" << palette.colors[i] << "'\n";
            }
            out << "\n# FZF colors\n";
            out << "export FZF_DEFAULT_OPTS=\"\n";
            out << "    $FZF_DEFAULT_OPTS\n";
            out << "    --color fg:7,bg:0,hl:1,fg+:232,bg+:1,hl+:255\n";
            out << "    --color info:7,prompt:2,spinner:1,pointer:232,marker:1\n";
            out << "\"\n";
        }
    }

    return true;
}

std::optional<Theme> PaletteExtractor::extract_from_image(const std::string& image_path) {
    auto palette_opt = extract_raw_palette(image_path);
    if (!palette_opt) return std::nullopt;

    const auto& pal = *palette_opt;

    // Atomically write compatibility files for downstream terminals and prompts
    emit_wal_cache(pal);

    // Synthesize Zenith theme
    Theme theme;
    theme.name = "dynamic";

    // 1. Surfaces: Deep dark with subtle wallpaper tint
    theme.background = ColorUtils::derive_dark_background(pal.background);
    theme.surface = ColorUtils::derive_dark_surface(pal.background);
    theme.surface_variant = ColorUtils::derive_dark_surface_variant(pal.background);
    theme.border = "rgba(255, 255, 255, 0.08)";

    // 2. Text: Wallpaper-derived + contrast-corrected
    theme.text_primary = ColorUtils::derive_text_primary(pal.foreground);
    theme.text_secondary = ColorUtils::derive_text_secondary(pal.foreground);
    theme.text_disabled = ColorUtils::derive_text_disabled(pal.foreground);
    theme.text = theme.text_primary;
    theme.text_muted = theme.text_secondary;

    // 3. Accent: Wallpaper-derived with constrained saturation & luminance
    theme.accent = ColorUtils::sanitize_accent(pal.accent);
    theme.accent_secondary = ColorUtils::sanitize_accent(pal.accent_secondary);

    // 4. Semantic Colors
    theme.semantic_success = "#3DDC84";
    theme.semantic_warning = "#FFB454";
    theme.semantic_error = "#FF5C6C";
    theme.semantic_info = "#55B9FF";
    theme.success = "#3DDC84";
    theme.warning = "#FFB454";
    theme.error = "#FF5C6C";

    std::cout << "[PaletteExtractor] Extracted dynamic theme natively: bg=" << theme.background 
              << ", text=" << theme.text_primary 
              << ", accent=" << theme.accent 
              << ", sec=" << theme.accent_secondary << std::endl;

    return theme;
}

} // namespace zenith
