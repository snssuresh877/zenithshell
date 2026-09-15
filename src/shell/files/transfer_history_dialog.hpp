#pragma once

#include <gtk/gtk.h>
#include <string>

namespace zenith {

class TransferHistoryDialog {
public:
    static void show(GtkWindow* parent);
};

} // namespace zenith
