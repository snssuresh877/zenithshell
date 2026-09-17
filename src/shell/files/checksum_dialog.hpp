#pragma once
#include <string>
#include <gtk/gtk.h>

namespace zenith {

class ChecksumDialog {
public:
    static void show(GtkWindow* parent, const std::string& filepath);
};

} // namespace zenith
