#include "shell/notification/notification_panel.hpp"
#include "gtk3_compat.hpp"
#include <gtk-layer-shell/gtk-layer-shell.h>
#include <gdk/gdk.h>
#include <gdk/gdkkeysyms.h>
#include <cairo.h>
#include <iostream>

namespace zenith {

GtkWidget* NotificationPanel::window = nullptr;
GtkWidget* NotificationPanel::list_box = nullptr;
GtkWidget* NotificationPanel::count_badge = nullptr;
GtkWidget* NotificationPanel::clear_btn = nullptr;
GtkWidget* NotificationPanel::empty_box = nullptr;
GtkWidget* NotificationPanel::scroll_window = nullptr;
GtkWidget* NotificationPanel::content_stack = nullptr;

void NotificationPanel::init(GtkApplication* app) {
    create_window(app);
    NotificationManager::set_history_changed_callback([]() {
        g_idle_add([](gpointer) -> gboolean {
            NotificationPanel::refresh();
            return FALSE;
        }, nullptr);
    });
}

void NotificationPanel::create_window(GtkApplication* app) {
    window = gtk_application_window_new(app);
    gtk_widget_add_css_class(window, "notif-panel-window");

    // Enable true RGBA transparency so rounded corners don't render grey/black box artifacts
    GdkScreen* screen = gtk_widget_get_screen(window);
    GdkVisual* visual = gdk_screen_get_rgba_visual(screen);
    if (visual) {
        gtk_widget_set_visual(window, visual);
    }
    gtk_widget_set_app_paintable(window, TRUE);

    g_signal_connect(window, "draw", G_CALLBACK(+[](GtkWidget*, cairo_t* cr, gpointer) -> gboolean {
        cairo_save(cr);
        cairo_set_operator(cr, CAIRO_OPERATOR_CLEAR);
        cairo_paint(cr);
        cairo_restore(cr);
        return FALSE;
    }), nullptr);

    // Full-screen overlay layer shell like Spotlight
    gtk_layer_init_for_window(GTK_WINDOW(window));
    gtk_layer_set_layer(GTK_WINDOW(window), GTK_LAYER_SHELL_LAYER_OVERLAY);
    gtk_layer_set_keyboard_mode(GTK_WINDOW(window), GTK_LAYER_SHELL_KEYBOARD_MODE_NONE);

    gtk_layer_set_anchor(GTK_WINDOW(window), GTK_LAYER_SHELL_EDGE_TOP, TRUE);
    gtk_layer_set_anchor(GTK_WINDOW(window), GTK_LAYER_SHELL_EDGE_BOTTOM, TRUE);
    gtk_layer_set_anchor(GTK_WINDOW(window), GTK_LAYER_SHELL_EDGE_LEFT, TRUE);
    gtk_layer_set_anchor(GTK_WINDOW(window), GTK_LAYER_SHELL_EDGE_RIGHT, TRUE);

    // Clickable transparent backdrop
    GtkWidget* backdrop = gtk_event_box_new();
    gtk_event_box_set_visible_window(GTK_EVENT_BOX(backdrop), FALSE);
    gtk_widget_add_css_class(backdrop, "notif-panel-backdrop");

    // Outer layout to position the card below topbar (margin-top: 48px, margin-end: 12px)
    GtkWidget* outer_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_halign(outer_box, GTK_ALIGN_END);
    gtk_widget_set_valign(outer_box, GTK_ALIGN_START);
    gtk_widget_set_margin_top(outer_box, 48);
    gtk_widget_set_margin_end(outer_box, 12);

    GtkWidget* main_card = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
    gtk_widget_add_css_class(main_card, "notif-panel-card");
    gtk_widget_set_size_request(main_card, 380, 520);

    auto on_backdrop_clicked = +[](GtkWidget* widget, GdkEventButton* event, gpointer user_data) -> gboolean {
        GtkWidget* card = static_cast<GtkWidget*>(user_data);
        if (!card) return FALSE;
        GtkAllocation alloc;
        gtk_widget_get_allocation(card, &alloc);
        int wx = 0, wy = 0;
        gtk_widget_translate_coordinates(widget, card, static_cast<int>(event->x), static_cast<int>(event->y), &wx, &wy);
        if (wx >= 0 && wx < alloc.width && wy >= 0 && wy < alloc.height) {
            return FALSE; // Click is inside the card
        }
        NotificationPanel::hide();
        return TRUE; // Click is outside the card
    };
    g_signal_connect(backdrop, "button-press-event", G_CALLBACK(on_backdrop_clicked), main_card);

    // Header Row: [󰂚 Notifications  (N)] ------ [󰃢 Clear All] [󰅖]
    GtkWidget* header_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_widget_add_css_class(header_box, "notif-panel-header");

    GtkWidget* title_icon = gtk_label_new("󰂚");
    gtk_widget_add_css_class(title_icon, "notif-panel-title-icon");

    GtkWidget* title_lbl = gtk_label_new("Notifications");
    gtk_widget_add_css_class(title_lbl, "notif-panel-title");

    count_badge = gtk_label_new("0");
    gtk_widget_add_css_class(count_badge, "notif-panel-badge");

    gtk_box_pack_start(GTK_BOX(header_box), title_icon, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(header_box), title_lbl, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(header_box), count_badge, FALSE, FALSE, 0);

    // Action Buttons on Right
    GtkWidget* right_actions = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);

    clear_btn = gtk_button_new_with_label("󰃢 Clear All");
    gtk_widget_add_css_class(clear_btn, "notif-clear-all-btn");
    g_signal_connect(clear_btn, "clicked", G_CALLBACK(on_clear_all_clicked), nullptr);

    GtkWidget* close_btn = gtk_button_new_with_label("󰅖");
    gtk_widget_add_css_class(close_btn, "notif-panel-close-btn");
    g_signal_connect(close_btn, "clicked", G_CALLBACK(+[](GtkButton*, gpointer) {
        NotificationPanel::hide();
    }), nullptr);

    gtk_box_pack_start(GTK_BOX(right_actions), clear_btn, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(right_actions), close_btn, FALSE, FALSE, 0);
    gtk_box_pack_end(GTK_BOX(header_box), right_actions, FALSE, FALSE, 0);

    gtk_box_pack_start(GTK_BOX(main_card), header_box, FALSE, FALSE, 0);

    // Scrollable Notifications List
    scroll_window = gtk_scrolled_window_new(nullptr, nullptr);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll_window), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_widget_set_vexpand(scroll_window, TRUE);

    list_box = gtk_list_box_new();
    gtk_widget_add_css_class(list_box, "notif-history-list");
    gtk_list_box_set_selection_mode(GTK_LIST_BOX(list_box), GTK_SELECTION_NONE);
    gtk_container_add(GTK_CONTAINER(scroll_window), list_box);

    // Empty State Widget
    empty_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_widget_add_css_class(empty_box, "notif-empty-box");
    gtk_widget_set_valign(empty_box, GTK_ALIGN_CENTER);
    gtk_widget_set_halign(empty_box, GTK_ALIGN_CENTER);
    gtk_widget_set_vexpand(empty_box, TRUE);

    GtkWidget* empty_icon = gtk_label_new("󰂛");
    gtk_widget_add_css_class(empty_icon, "notif-empty-icon");

    GtkWidget* empty_title = gtk_label_new("No Notifications");
    gtk_widget_add_css_class(empty_title, "notif-empty-title");

    GtkWidget* empty_sub = gtk_label_new("You're all caught up!");
    gtk_widget_add_css_class(empty_sub, "notif-empty-sub");

    gtk_box_pack_start(GTK_BOX(empty_box), empty_icon, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(empty_box), empty_title, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(empty_box), empty_sub, FALSE, FALSE, 0);

    // Content Stack: Switches between "list" and "empty" cleanly
    content_stack = gtk_stack_new();
    gtk_stack_set_transition_type(GTK_STACK(content_stack), GTK_STACK_TRANSITION_TYPE_CROSSFADE);
    gtk_stack_set_transition_duration(GTK_STACK(content_stack), 150);
    gtk_widget_set_vexpand(content_stack, TRUE);

    gtk_stack_add_named(GTK_STACK(content_stack), scroll_window, "list");
    gtk_stack_add_named(GTK_STACK(content_stack), empty_box, "empty");

    gtk_box_pack_start(GTK_BOX(main_card), content_stack, TRUE, TRUE, 0);

    gtk_box_pack_start(GTK_BOX(outer_box), main_card, FALSE, FALSE, 0);
    gtk_container_add(GTK_CONTAINER(backdrop), outer_box);
    gtk_container_add(GTK_CONTAINER(window), backdrop);

    g_signal_connect(window, "key-press-event", G_CALLBACK(+[](GtkWidget*, GdkEventKey* event, gpointer) -> gboolean {
        if (event->keyval == GDK_KEY_Escape) {
            NotificationPanel::hide();
            return TRUE;
        }
        return FALSE;
    }), nullptr);

    gtk_widget_show_all(window);
    gtk_widget_hide(window);
}

static int64_t last_notif_hide_time_ms = 0;

void NotificationPanel::toggle() {
    if (!window) return;
    int64_t now = g_get_monotonic_time() / 1000;
    if (now - last_notif_hide_time_ms < 250) {
        return;
    }
    if (gtk_widget_get_visible(window)) {
        hide();
    } else {
        show();
    }
}

void NotificationPanel::show() {
    if (!window) return;
    refresh();
    gtk_widget_show(window);
    gtk_window_present(GTK_WINDOW(window));
}

void NotificationPanel::hide() {
    if (!window) return;
    last_notif_hide_time_ms = g_get_monotonic_time() / 1000;
    gtk_widget_hide(window);
}

void NotificationPanel::refresh() {
    if (!list_box || !count_badge || !content_stack) return;

    // Clear list
    GList* children = gtk_container_get_children(GTK_CONTAINER(list_box));
    for (GList* l = children; l != nullptr; l = l->next) {
        gtk_widget_destroy(GTK_WIDGET(l->data));
    }
    g_list_free(children);

    const auto& history = NotificationManager::get_history();
    int count = static_cast<int>(history.size());

    char count_str[16];
    snprintf(count_str, sizeof(count_str), "%d", count);
    gtk_label_set_text(GTK_LABEL(count_badge), count_str);

    if (clear_btn) {
        gtk_widget_set_sensitive(clear_btn, count > 0);
    }

    if (count == 0) {
        gtk_stack_set_visible_child_name(GTK_STACK(content_stack), "empty");
    } else {
        for (const auto& item : history) {
            build_notification_item(item);
        }
        gtk_widget_show_all(list_box);
        gtk_stack_set_visible_child_name(GTK_STACK(content_stack), "list");
    }
}

void NotificationPanel::build_notification_item(const NotificationItem& item) {
    GtkWidget* card = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    gtk_widget_add_css_class(card, "notif-item-card");

    // Header inside item: [Icon] [App Name] ------- [Time] [󰅖]
    GtkWidget* item_header = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);

    GtkWidget* icon_lbl = gtk_label_new(item.app_icon.empty() ? "󰂚" : item.app_icon.c_str());
    gtk_widget_add_css_class(icon_lbl, "notif-item-icon");

    GtkWidget* app_name_lbl = gtk_label_new(item.app_name.empty() ? "System" : item.app_name.c_str());
    gtk_widget_add_css_class(app_name_lbl, "notif-item-app-name");
    gtk_widget_set_halign(app_name_lbl, GTK_ALIGN_START);

    GtkWidget* right_meta = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    GtkWidget* time_lbl = gtk_label_new(item.timestamp.c_str());
    gtk_widget_add_css_class(time_lbl, "notif-item-time");

    GtkWidget* dismiss_btn = gtk_button_new_with_label("󰅖");
    gtk_widget_add_css_class(dismiss_btn, "notif-item-dismiss-btn");
    gtk_widget_set_tooltip_text(dismiss_btn, "Dismiss notification");

    uint32_t notif_id = item.id;
    g_signal_connect(dismiss_btn, "clicked", G_CALLBACK(+[](GtkButton*, gpointer data) {
        uint32_t id = GPOINTER_TO_UINT(data);
        NotificationManager::remove_notification(id);
    }), GUINT_TO_POINTER(notif_id));

    gtk_box_pack_start(GTK_BOX(right_meta), time_lbl, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(right_meta), dismiss_btn, FALSE, FALSE, 0);

    gtk_box_pack_start(GTK_BOX(item_header), icon_lbl, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(item_header), app_name_lbl, FALSE, FALSE, 0);
    gtk_box_pack_end(GTK_BOX(item_header), right_meta, FALSE, FALSE, 0);

    gtk_box_pack_start(GTK_BOX(card), item_header, FALSE, FALSE, 0);

    // Summary Title
    if (!item.summary.empty()) {
        GtkWidget* sum_lbl = gtk_label_new(item.summary.c_str());
        gtk_widget_add_css_class(sum_lbl, "notif-item-summary");
        gtk_widget_set_halign(sum_lbl, GTK_ALIGN_START);
        gtk_label_set_line_wrap(GTK_LABEL(sum_lbl), TRUE);
        gtk_box_pack_start(GTK_BOX(card), sum_lbl, FALSE, FALSE, 0);
    }

    // Body Text
    if (!item.body.empty()) {
        GtkWidget* body_lbl = gtk_label_new(item.body.c_str());
        gtk_widget_add_css_class(body_lbl, "notif-item-body");
        gtk_widget_set_halign(body_lbl, GTK_ALIGN_START);
        gtk_label_set_line_wrap(GTK_LABEL(body_lbl), TRUE);
        gtk_label_set_max_width_chars(GTK_LABEL(body_lbl), 36);
        gtk_box_pack_start(GTK_BOX(card), body_lbl, FALSE, FALSE, 0);
    }

    gtk_container_add(GTK_CONTAINER(list_box), card);
}

void NotificationPanel::on_clear_all_clicked(GtkButton*, gpointer) {
    NotificationManager::clear_history();
    NotificationPanel::refresh();
}

} // namespace zenith
