#include "shell/bar/workspace_widget.hpp"
#include "gtk3_compat.hpp"
#include "compositors/hyprland_ipc.hpp"
#include <iostream>
#include <string>

namespace zenith {

std::vector<GtkWidget*> WorkspaceWidget::buttons;
int WorkspaceWidget::current_active = 1;

GtkWidget* WorkspaceWidget::create(int count) {
    GtkWidget* box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 2);
    gtk_widget_add_css_class(box, "workspace-container");

    buttons.clear();

    // Left Arrow
    GtkWidget* btn_prev = gtk_button_new();
    gtk_widget_add_css_class(btn_prev, "workspace-btn");
    GtkWidget* lbl_prev = gtk_label_new("❮");
    gtk_container_add(GTK_CONTAINER(btn_prev), lbl_prev);
    g_signal_connect(btn_prev, "clicked", G_CALLBACK(+[](GtkButton*, gpointer) {
        HyprlandIPC::switch_workspace_relative(-1);
    }), nullptr);
    gtk_box_pack_start(GTK_BOX(box), btn_prev, FALSE, FALSE, 0);

    // Current Number
    GtkWidget* btn = gtk_button_new();
    gtk_widget_add_css_class(btn, "workspace-btn");
    gtk_widget_add_css_class(btn, "active"); // Always active style
    GtkWidget* lbl = gtk_label_new("1");
    gtk_container_add(GTK_CONTAINER(btn), lbl);
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

    // Right Arrow
    GtkWidget* btn_next = gtk_button_new();
    gtk_widget_add_css_class(btn_next, "workspace-btn");
    GtkWidget* lbl_next = gtk_label_new("❯");
    gtk_container_add(GTK_CONTAINER(btn_next), lbl_next);
    g_signal_connect(btn_next, "clicked", G_CALLBACK(+[](GtkButton*, gpointer) {
        HyprlandIPC::switch_workspace_relative(1);
    }), nullptr);
    gtk_box_pack_start(GTK_BOX(box), btn_next, FALSE, FALSE, 0);

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
