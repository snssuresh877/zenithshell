#pragma once

#include "theme/theme.hpp"
#include <string>
#include <vector>
#include <optional>

namespace zenith {

class ThemeLoader {
public:
    static std::optional<Theme> load_from_file(const std::string& filepath);
    static std::optional<Theme> load_omarchy_theme(const std::string& theme_dir, std::vector<std::string>& out_wallpapers);
    static bool save_to_file(const Theme& theme, const std::string& filepath);
    static std::string get_themes_dir();
    static std::vector<std::string> list_custom_themes();
};

} // namespace zenith
