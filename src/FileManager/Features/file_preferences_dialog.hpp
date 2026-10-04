#pragma once

#include <gtk/gtk.h>
#include <functional>

namespace zenith {

struct FileManagerState;

class FilePreferencesDialog {
public:
    static void show(FileManagerState* state);
};

} // namespace zenith
