#pragma once

#include <string>
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>

namespace zenith {

struct HSL {
    double h; // 0..360
    double s; // 0..1
    double l; // 0..1
};

struct RGB {
    int r; // 0..255
    int g; // 0..255
    int b; // 0..255
};

class ColorUtils {
public:
    static RGB hex_to_rgb(const std::string& hex) {
        if (hex.length() < 7 || hex[0] != '#') return {14, 15, 20};
        try {
            int r = std::stoi(hex.substr(1, 2), nullptr, 16);
            int g = std::stoi(hex.substr(3, 2), nullptr, 16);
            int b = std::stoi(hex.substr(5, 2), nullptr, 16);
            return {r, g, b};
        } catch (...) {
            return {14, 15, 20};
        }
    }

    static std::string rgb_to_hex(const RGB& rgb) {
        std::stringstream ss;
        ss << "#" << std::hex << std::setfill('0')
           << std::setw(2) << std::clamp(rgb.r, 0, 255)
           << std::setw(2) << std::clamp(rgb.g, 0, 255)
           << std::setw(2) << std::clamp(rgb.b, 0, 255);
        return ss.str();
    }

    static HSL rgb_to_hsl(const RGB& rgb) {
        double r = rgb.r / 255.0;
        double g = rgb.g / 255.0;
        double b = rgb.b / 255.0;

        double max_val = std::max({r, g, b});
        double min_val = std::min({r, g, b});
        double delta = max_val - min_val;

        double h = 0.0;
        double s = 0.0;
        double l = (max_val + min_val) / 2.0;

        if (delta > 1e-5) {
            s = (l > 0.5) ? (delta / (2.0 - max_val - min_val)) : (delta / (max_val + min_val));

            if (max_val == r) {
                h = ((g - b) / delta) + (g < b ? 6.0 : 0.0);
            } else if (max_val == g) {
                h = ((b - r) / delta) + 2.0;
            } else {
                h = ((r - g) / delta) + 4.0;
            }
            h *= 60.0;
        }

        return {h, s, l};
    }

    static double hue_to_rgb_helper(double p, double q, double t) {
        if (t < 0.0) t += 1.0;
        if (t > 1.0) t -= 1.0;
        if (t < 1.0 / 6.0) return p + (q - p) * 6.0 * t;
        if (t < 1.0 / 2.0) return q;
        if (t < 2.0 / 3.0) return p + (q - p) * (2.0 / 3.0 - t) * 6.0;
        return p;
    }

    static RGB hsl_to_rgb(const HSL& hsl) {
        double h = hsl.h / 360.0;
        double s = std::clamp(hsl.s, 0.0, 1.0);
        double l = std::clamp(hsl.l, 0.0, 1.0);

        if (s < 1e-5) {
            int gray = static_cast<int>(std::round(l * 255.0));
            return {gray, gray, gray};
        }

        double q = (l < 0.5) ? (l * (1.0 + s)) : (l + s - l * s);
        double p = 2.0 * l - q;

        int r = static_cast<int>(std::round(hue_to_rgb_helper(p, q, h + 1.0 / 3.0) * 255.0));
        int g = static_cast<int>(std::round(hue_to_rgb_helper(p, q, h) * 255.0));
        int b = static_cast<int>(std::round(hue_to_rgb_helper(p, q, h - 1.0 / 3.0) * 255.0));

        return {std::clamp(r, 0, 255), std::clamp(g, 0, 255), std::clamp(b, 0, 255)};
    }

    // Adapt text color to wallpaper/theme with guaranteed high contrast
    static std::string derive_text_primary(const std::string& base_hex) {
        RGB rgb = hex_to_rgb(base_hex);
        HSL hsl = rgb_to_hsl(rgb);
        // Force high luminance and gentle desaturation for perfect readability
        hsl.l = 0.94;
        hsl.s = std::clamp(hsl.s * 0.40, 0.06, 0.25);
        return rgb_to_hex(hsl_to_rgb(hsl));
    }

    static std::string derive_text_secondary(const std::string& base_hex) {
        RGB rgb = hex_to_rgb(base_hex);
        HSL hsl = rgb_to_hsl(rgb);
        hsl.l = 0.70;
        hsl.s = std::clamp(hsl.s * 0.35, 0.06, 0.20);
        return rgb_to_hex(hsl_to_rgb(hsl));
    }

    static std::string derive_text_tertiary(const std::string& base_hex) {
        RGB rgb = hex_to_rgb(base_hex);
        HSL hsl = rgb_to_hsl(rgb);
        hsl.l = 0.50;
        hsl.s = std::clamp(hsl.s * 0.30, 0.04, 0.16);
        return rgb_to_hex(hsl_to_rgb(hsl));
    }

    static std::string derive_text_disabled(const std::string& base_hex) {
        RGB rgb = hex_to_rgb(base_hex);
        HSL hsl = rgb_to_hsl(rgb);
        hsl.l = 0.36;
        hsl.s = std::clamp(hsl.s * 0.25, 0.02, 0.12);
        return rgb_to_hex(hsl_to_rgb(hsl));
    }

    // Force surfaces to remain deep dark with subtle wallpaper tint
    static std::string derive_dark_background(const std::string& base_hex) {
        RGB rgb = hex_to_rgb(base_hex);
        HSL hsl = rgb_to_hsl(rgb);
        hsl.l = 0.055; // ~#0B0C12
        hsl.s = std::clamp(hsl.s * 0.35, 0.08, 0.24);
        return rgb_to_hex(hsl_to_rgb(hsl));
    }

    static std::string derive_dark_surface(const std::string& base_hex) {
        RGB rgb = hex_to_rgb(base_hex);
        HSL hsl = rgb_to_hsl(rgb);
        hsl.l = 0.085; // ~#11121A
        hsl.s = std::clamp(hsl.s * 0.35, 0.08, 0.24);
        return rgb_to_hex(hsl_to_rgb(hsl));
    }

    static std::string derive_dark_surface_variant(const std::string& base_hex) {
        RGB rgb = hex_to_rgb(base_hex);
        HSL hsl = rgb_to_hsl(rgb);
        hsl.l = 0.115; // ~#161722
        hsl.s = std::clamp(hsl.s * 0.35, 0.08, 0.24);
        return rgb_to_hex(hsl_to_rgb(hsl));
    }

    // Constrain accent saturation & luminance so it's vivid yet sophisticated
    static std::string sanitize_accent(const std::string& raw_hex) {
        RGB rgb = hex_to_rgb(raw_hex);
        HSL hsl = rgb_to_hsl(rgb);
        hsl.s = std::clamp(hsl.s, 0.50, 0.85);
        hsl.l = std::clamp(hsl.l, 0.58, 0.72);
        return rgb_to_hex(hsl_to_rgb(hsl));
    }

    static std::string get_rgb_csv(const std::string& hex) {
        RGB rgb = hex_to_rgb(hex);
        return std::to_string(rgb.r) + ", " + std::to_string(rgb.g) + ", " + std::to_string(rgb.b);
    }
};

} // namespace zenith
