#pragma once
#include <gtk/gtk.h>
#include <string>
#include <vector>
#include <functional>

namespace zenith {

class BulkRenameDialog {
public:
    static void show(GtkWindow* parent, const std::vector<std::string>& paths, std::function<void()> on_complete);
};

} // namespace zenith
