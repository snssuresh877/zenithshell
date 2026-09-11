#pragma once

#include <gtk/gtk.h>
#include <string>
#include <functional>
#include <vector>

namespace zenith {

struct PaletteAction {
    std::string id;
    std::string title;
    std::string category;
    std::string icon_name;
    std::string shortcut_id;
};

class CommandPalette {
public:
    using ActionHandler = std::function<void(const std::string& action_id)>;

    static void show(GtkWindow* parent, ActionHandler handler);
};

} // namespace zenith
