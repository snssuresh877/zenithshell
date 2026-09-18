#include "shell/bar/workspace_widget.hpp"
#include "gtk3_compat.hpp"
#include "compositors/hyprland_ipc.hpp"
#include <iostream>
#include <string>

namespace zenith {

std::vector<GtkWidget*> WorkspaceWidget::buttons;
int WorkspaceWidget::current_active = 1;

GtkWidget* WorkspaceWidget::create(int count) {
    GtkWidget* box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    gtk_widget_add_css_class(box, "workspace-container");

    buttons.clear();

    GtkWidget* btn = gtk_button_new();
    gtk_widget_add_css_class(btn, "workspace-btn");
    gtk_widget_add_css_class(btn, "active"); // Always active style

    GtkWidget* lbl = gtk_label_new("1");
    gtk_container_add(GTK_CONTAINER(btn), lbl);

    g_signal_connect(btn, "clicked", G_CALLBACK(+[](GtkButton*, gpointer) {
        HyprlandIPC::switch_workspace_relative(1);
    }), nullptr);

    gtk_widget_add_events(btn, GDK_SCROLL_MASK);
    g_signal_connect(btn, "scroll-event", G_CALLBACK(+[](GtkWidget*, GdkEventScroll* event, gpointer) -> gboolean {
        if (event->direction == GDK_SCROLL_UP || event->delta_y < 0) {
            HyprlandIPC::switch_workspace_relative(-1);
            return TRUE;
        } else if (event->direction == GDK_SCROLL_DOWN || event->delta_y > 0) {
            HyprlandIPC::switch_workspace_relative(1);
            return TRUE;
        }
        return FALSE;
    }), nullptr);

    gtk_box_pack_start(GTK_BOX(box), btn, FALSE, FALSE, 0);
    buttons.push_back(btn);

    HyprlandIPC::instance().set_workspace_callback([](int active_id) {
        update_active(active_id);
    });

    return box;
}

void WorkspaceWidget::update_active(int active_id) {
    current_active = active_id;
    if (!buttons.empty()) {
        GtkWidget* child = gtk_bin_get_child(GTK_BIN(buttons[0]));
        if (child) {
            gtk_label_set_text(GTK_LABEL(child), std::to_string(active_id).c_str());
        }
    }
}

void WorkspaceWidget::on_workspace_clicked(GtkButton*, gpointer user_data) {
    // Unused now
}

} // namespace zenith
