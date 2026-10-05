#pragma once

#include <string>

namespace zenith {

struct WorkspaceChangedEvent {
    int workspace_id = 1;
};

struct WindowTitleChangedEvent {
    std::string title;
};

struct WindowListChangedEvent {};

} // namespace zenith
