#pragma once

#include <gtk/gtk.h>
#include <memory>
#include "Core/EventBus/event_bus.hpp"
#include "Core/Events/compositor_events.hpp"

namespace zenith {

class WorkspaceWidget {
public:
    explicit WorkspaceWidget(std::shared_ptr<EventBus> event_bus = nullptr);
    ~WorkspaceWidget();

    WorkspaceWidget(const WorkspaceWidget&) = delete;
    WorkspaceWidget& operator=(const WorkspaceWidget&) = delete;
    WorkspaceWidget(WorkspaceWidget&&) = delete;
    WorkspaceWidget& operator=(WorkspaceWidget&&) = delete;

    GtkWidget* get_widget() const { return container_; }
    int get_active_workspace() const { return current_active_; }

    static GtkWidget* create(int count = 5, std::shared_ptr<EventBus> event_bus = nullptr);
    void update_active(int active_id);

    static void update_active_global(int active_id);

private:
    std::shared_ptr<EventBus> event_bus_;
    SubscriptionId workspace_subscription_id_ = INVALID_SUBSCRIPTION_ID;

    GtkWidget* container_ = nullptr;
    GtkWidget* btn_number_ = nullptr;
    GtkWidget* lbl_number_ = nullptr;
    int current_active_ = 1;

    void build_ui();
};

} // namespace zenith
