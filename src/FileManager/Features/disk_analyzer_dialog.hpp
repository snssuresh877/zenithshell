#pragma once
#include <string>
#include <gtk/gtk.h>

namespace zenith {

class DiskAnalyzerDialog {
public:
    static void show(GtkWindow* parent, const std::string& directory_path);
};

} // namespace zenith
