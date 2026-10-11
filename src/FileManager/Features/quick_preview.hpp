#pragma once

#include <gtk/gtk.h>
#include <string>

namespace zenith {

class QuickPreview {
public:
    static void toggle(GtkWindow* parent, const std::string& file_path);
    static void close();
    static bool is_open();
};

} // namespace zenith
