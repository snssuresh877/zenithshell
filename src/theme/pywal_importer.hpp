#pragma once

#include "theme/theme.hpp"
#include <string>
#include <optional>

namespace zenith {

class PywalImporter {
public:
    static std::optional<Theme> import_from_cache(const std::string& custom_path = "");
    static bool is_available();
};

} // namespace zenith
