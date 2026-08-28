#include "shell/bar/workspace_widget.hpp"
#include "gtk3_compat.hpp"
#include "compositors/hyprland_ipc.hpp"
#include <iostream>

namespace zenith {

std::vector<GtkWidget*> WorkspaceWidget::buttons;
int WorkspaceWidget::current_active = 1;

GtkWidget* WorkspaceWidget::create(int count) {
    int total_workspaces = (count > 0 && count <= 5) ? count : 5;

    GtkWidget* box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    gtk_widget_add_css_class(box, "workspace-container");

    buttons.clear();

    for (int i = 1; i <= total_workspaces; ++i) {
        GtkWidget* btn = gtk_button_new();
        gtk_widget_add_css_class(btn, "workspace-btn");

        GtkWidget* lbl = gtk_label_new(i == 1 ? "󰮯" : "󰧞");
        gtk_container_add(GTK_CONTAINER(btn), lbl);

        if (i == 1) {
            gtk_widget_add_css_class(btn, "active");
        } else {
            gtk_widget_add_css_class(btn, "inactive-dot");
        }

        g_signal_connect(btn, "clicked", G_CALLBACK(on_workspace_clicked), GINT_TO_POINTER(i));

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
    }

    HyprlandIPC::instance().set_workspace_callback([](int active_id) {
        update_active(active_id);
    });

    return box;
}

void WorkspaceWidget::update_active(int active_id) {
    current_active = active_id;
    for (size_t i = 0; i < buttons.size(); ++i) {
        int id = static_cast<int>(i + 1);
        GtkWidget* child = gtk_bin_get_child(GTK_BIN(buttons[i]));
        if (!child) continue;

        if (id == active_id) {
            gtk_label_set_text(GTK_LABEL(child), "󰮯");
            gtk_widget_remove_css_class(buttons[i], "inactive-dot");
            gtk_widget_add_css_class(buttons[i], "active");
        } else {
            gtk_label_set_text(GTK_LABEL(child), "󰧞");
            gtk_widget_remove_css_class(buttons[i], "active");
            gtk_widget_add_css_class(buttons[i], "inactive-dot");
        }
    }
}

void WorkspaceWidget::on_workspace_clicked(GtkButton*, gpointer user_data) {
    int ws_id = GPOINTER_TO_INT(user_data);
    std::cout << "[WorkspaceWidget] Clicked workspace: " << ws_id << std::endl;
    HyprlandIPC::switch_workspace(ws_id);
    update_active(ws_id);
}

} // namespace zenith
