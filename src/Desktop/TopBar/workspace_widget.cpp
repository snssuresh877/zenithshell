#include "Desktop/TopBar/workspace_widget.hpp"
#include "gtk3_compat.hpp"
#include "Platform/Compositor/compositor_manager.hpp"
#include <iostream>
#include <string>

namespace zenith {

static WorkspaceWidget* s_active_instance = nullptr;

WorkspaceWidget::WorkspaceWidget(std::shared_ptr<EventBus> event_bus)
    : event_bus_(std::move(event_bus)) {
    if (!event_bus_) {
        event_bus_ = CompositorManager::instance().get_event_bus();
    }

    s_active_instance = this;
    build_ui();

    // Query initial workspace state from compositor
    current_active_ = CompositorManager::instance().get_active_workspace_id();
    update_active(current_active_);

    // Directly subscribe to typed WorkspaceChangedEvent via EventBus
    if (event_bus_) {
        workspace_subscription_id_ = event_bus_->subscribe<WorkspaceChangedEvent>(
            [this](const WorkspaceChangedEvent& event) {
                update_active(event.workspace_id);
            }
        );
    }
}

WorkspaceWidget::~WorkspaceWidget() {
    if (s_active_instance == this) {
        s_active_instance = nullptr;
    }

    if (event_bus_ && workspace_subscription_id_ != INVALID_SUBSCRIPTION_ID) {
        event_bus_->unsubscribe<WorkspaceChangedEvent>(workspace_subscription_id_);
        workspace_subscription_id_ = INVALID_SUBSCRIPTION_ID;
    }
}

void WorkspaceWidget::build_ui() {
    container_ = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 2);
    gtk_widget_add_css_class(container_, "workspace-container");

    // Left Arrow
    GtkWidget* btn_prev = gtk_button_new();
    gtk_widget_add_css_class(btn_prev, "workspace-btn");
    GtkWidget* lbl_prev = gtk_label_new("❮");
    gtk_container_add(GTK_CONTAINER(btn_prev), lbl_prev);
    g_signal_connect(btn_prev, "clicked", G_CALLBACK(+[](GtkButton*, gpointer) {
        CompositorManager::instance().switch_workspace_relative(-1);
    }), nullptr);
    gtk_box_pack_start(GTK_BOX(container_), btn_prev, FALSE, FALSE, 0);

    // Current Number
    btn_number_ = gtk_button_new();
    gtk_widget_add_css_class(btn_number_, "workspace-btn");
    gtk_widget_add_css_class(btn_number_, "active"); // Always active style
    lbl_number_ = gtk_label_new(std::to_string(current_active_).c_str());
    gtk_container_add(GTK_CONTAINER(btn_number_), lbl_number_);
    gtk_widget_add_events(btn_number_, GDK_SCROLL_MASK);
    g_signal_connect(btn_number_, "scroll-event", G_CALLBACK(+[](GtkWidget*, GdkEventScroll* event, gpointer) -> gboolean {
        if (event->direction == GDK_SCROLL_UP || event->delta_y < 0) {
            CompositorManager::instance().switch_workspace_relative(-1);
            return TRUE;
        } else if (event->direction == GDK_SCROLL_DOWN || event->delta_y > 0) {
            CompositorManager::instance().switch_workspace_relative(1);
            return TRUE;
        }
        return FALSE;
    }), nullptr);
    gtk_box_pack_start(GTK_BOX(container_), btn_number_, FALSE, FALSE, 0);

    // Right Arrow
    GtkWidget* btn_next = gtk_button_new();
    gtk_widget_add_css_class(btn_next, "workspace-btn");
    GtkWidget* lbl_next = gtk_label_new("❯");
    gtk_container_add(GTK_CONTAINER(btn_next), lbl_next);
    g_signal_connect(btn_next, "clicked", G_CALLBACK(+[](GtkButton*, gpointer) {
        CompositorManager::instance().switch_workspace_relative(1);
    }), nullptr);
    gtk_box_pack_start(GTK_BOX(container_), btn_next, FALSE, FALSE, 0);
}

GtkWidget* WorkspaceWidget::create(int /*count*/, std::shared_ptr<EventBus> event_bus) {
    auto* widget = new WorkspaceWidget(std::move(event_bus));
    GtkWidget* root = widget->get_widget();

    // Bind instance lifecycle to GTK container destruction
    g_object_set_data_full(
        G_OBJECT(root),
        "zenith_workspace_widget_instance",
        widget,
        [](gpointer data) {
            delete static_cast<WorkspaceWidget*>(data);
        }
    );

    return root;
}

void WorkspaceWidget::update_active(int active_id) {
    current_active_ = active_id;
    if (lbl_number_) {
        gtk_label_set_text(GTK_LABEL(lbl_number_), std::to_string(active_id).c_str());
    }
}

void WorkspaceWidget::update_active_global(int active_id) {
    if (s_active_instance) {
        s_active_instance->update_active(active_id);
    }
}

} // namespace zenith
