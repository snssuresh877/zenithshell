#pragma once

namespace zenith {

struct NotificationHistoryChangedEvent {
    int count = 0;
};

struct DndChangedEvent {
    bool dnd_enabled = false;
};

} // namespace zenith
