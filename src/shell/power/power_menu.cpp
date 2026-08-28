#include "shell/power/power_menu.hpp"
#include "gtk3_compat.hpp"
#include <gtk-layer-shell/gtk-layer-shell.h>
#include <gdk/gdk.h>
#include <gdk/gdkkeysyms.h>
#include <cairo.h>
#include <sys/sysinfo.h>
#include <unistd.h>
#include <ctime>
#include <iostream>
#include <fstream>

namespace zenith {

GtkWidget* PowerMenu::window = nullptr;
GtkWidget* PowerMenu::uptime_lbl = nullptr;
std::vector<GtkWidget*> PowerMenu::action_buttons;
std::vector<PowerAction> PowerMenu::actions;

void PowerMenu::init(GtkApplication* app) {
    setup_actions();
    create_window(app);
}

void PowerMenu::setup_actions() {
    actions = {
        {"lock", "", "Lock", "Secure Session", "L", "tile-lock", "loginctl lock-session 2>/dev/null || hyprlock 2>/dev/null &"},
        {"suspend", "󰤄", "Sleep", "Suspend to RAM", "S", "tile-suspend", "loginctl lock-session; systemctl suspend 2>/dev/null &"},
        {"logout", "󰍃", "Logout", "Exit Hyprland", "E", "tile-logout", "hyprctl dispatch exit 2>/dev/null &"},
        {"reboot", "󰜉", "Restart", "Reboot System", "R", "tile-reboot", "systemctl reboot 2>/dev/null &"},
        {"poweroff", "", "Shut Down", "Power Off PC", "P", "tile-power", "systemctl poweroff 2>/dev/null &"}
    };
}

std::string PowerMenu::get_uptime_string() {
    struct sysinfo s_info;
    if (sysinfo(&s_info) == 0) {
        long sec = s_info.uptime;
        long days = sec / 86400;
        long hours = (sec % 86400) / 3600;
        long mins = (sec % 3600) / 60;
        if (days > 0) {
            return std::to_string(days) + "d " + std::to_string(hours) + "h " + std::to_string(mins) + "m";
        }
        return std::to_string(hours) + "h " + std::to_string(mins) + "m";
    }
    return "Unknown";
}

void PowerMenu::execute_action(size_t index) {
    if (index >= actions.size()) return;
    hide();
    std::string cmd = actions[index].command;
    if (!cmd.empty()) {
        system(cmd.c_str());
    }
}

void PowerMenu::create_window(GtkApplication* app) {
    if (window) return;

    window = gtk_application_window_new(app);
    gtk_widget_add_css_class(window, "powermenu-window");

    // Enable true RGBA transparency
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

    gtk_layer_init_for_window(GTK_WINDOW(window));
    gtk_layer_set_layer(GTK_WINDOW(window), GTK_LAYER_SHELL_LAYER_TOP);
    gtk_layer_set_keyboard_mode(GTK_WINDOW(window), GTK_LAYER_SHELL_KEYBOARD_MODE_EXCLUSIVE);

    // Full screen overlay with blurred/dimmed backdrop
    gtk_layer_set_anchor(GTK_WINDOW(window), GTK_LAYER_SHELL_EDGE_TOP, TRUE);
    gtk_layer_set_anchor(GTK_WINDOW(window), GTK_LAYER_SHELL_EDGE_BOTTOM, TRUE);
    gtk_layer_set_anchor(GTK_WINDOW(window), GTK_LAYER_SHELL_EDGE_LEFT, TRUE);
    gtk_layer_set_anchor(GTK_WINDOW(window), GTK_LAYER_SHELL_EDGE_RIGHT, TRUE);

    GtkWidget* backdrop = gtk_event_box_new();
    gtk_event_box_set_visible_window(GTK_EVENT_BOX(backdrop), FALSE);
    gtk_widget_add_css_class(backdrop, "powermenu-backdrop");

    // Center card layout
    GtkWidget* center_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_halign(center_box, GTK_ALIGN_CENTER);
    gtk_widget_set_valign(center_box, GTK_ALIGN_CENTER);

    GtkWidget* card = gtk_box_new(GTK_ORIENTATION_VERTICAL, 22);
    gtk_widget_add_css_class(card, "powermenu-card");
    gtk_widget_set_size_request(card, 720, 310);

    auto on_backdrop_clicked = +[](GtkWidget* widget, GdkEventButton* event, gpointer user_data) -> gboolean {
        GtkWidget* card_w = static_cast<GtkWidget*>(user_data);
        if (!card_w) return FALSE;
        GtkAllocation alloc;
        gtk_widget_get_allocation(card_w, &alloc);
        int wx = 0, wy = 0;
        gtk_widget_translate_coordinates(widget, card_w, static_cast<int>(event->x), static_cast<int>(event->y), &wx, &wy);
        if (wx >= 0 && wx < alloc.width && wy >= 0 && wy < alloc.height) {
            return FALSE;
        }
        PowerMenu::hide();
        return TRUE;
    };
    g_signal_connect(backdrop, "button-press-event", G_CALLBACK(on_backdrop_clicked), card);

    // ─── Header: [Identity • Host] ──────── [Time • Uptime • Close] ───
    GtkWidget* header_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
    gtk_widget_add_css_class(header_box, "powermenu-header");

    // Left: Avatar Icon + User/Host
    GtkWidget* left_header = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    GtkWidget* avatar_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_widget_add_css_class(avatar_box, "powermenu-avatar-box");
    GtkWidget* avatar_icon = gtk_label_new("󰌢");
    gtk_widget_add_css_class(avatar_icon, "powermenu-avatar-icon");
    gtk_container_add(GTK_CONTAINER(avatar_box), avatar_icon);

    GtkWidget* user_vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
    const char* cur_user = g_get_user_name();
    char hostname[64] = "arch";
    gethostname(hostname, sizeof(hostname));
    std::string user_host = (cur_user ? std::string(cur_user) : "user") + "@" + hostname;
    GtkWidget* user_title = gtk_label_new(user_host.c_str());
    gtk_widget_add_css_class(user_title, "powermenu-user-title");
    gtk_widget_set_halign(user_title, GTK_ALIGN_START);

    GtkWidget* session_sub = gtk_label_new("Hyprland Desktop Session");
    gtk_widget_add_css_class(session_sub, "powermenu-session-sub");
    gtk_widget_set_halign(session_sub, GTK_ALIGN_START);

    gtk_box_pack_start(GTK_BOX(user_vbox), user_title, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(user_vbox), session_sub, FALSE, FALSE, 0);

    gtk_box_pack_start(GTK_BOX(left_header), avatar_box, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(left_header), user_vbox, FALSE, FALSE, 0);

    // Right: Status Capsule + Close Button
    GtkWidget* right_header = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    uptime_lbl = gtk_label_new("󰔚 --");
    gtk_widget_add_css_class(uptime_lbl, "powermenu-status-capsule");

    GtkWidget* close_btn = gtk_button_new_with_label("󰅖");
    gtk_widget_add_css_class(close_btn, "powermenu-close-btn");
    gtk_widget_set_tooltip_text(close_btn, "Close (Esc)");
    g_signal_connect(close_btn, "clicked", G_CALLBACK(+[](GtkButton*, gpointer) {
        PowerMenu::hide();
    }), nullptr);

    gtk_box_pack_start(GTK_BOX(right_header), uptime_lbl, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(right_header), close_btn, FALSE, FALSE, 0);

    gtk_box_pack_start(GTK_BOX(header_box), left_header, FALSE, FALSE, 0);
    gtk_box_pack_end(GTK_BOX(header_box), right_header, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(card), header_box, FALSE, FALSE, 0);

    // ─── Action Tiles Row (5 High-End Cards) ───
    GtkWidget* tiles_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 14);
    gtk_widget_set_halign(tiles_row, GTK_ALIGN_CENTER);
    gtk_widget_set_hexpand(tiles_row, TRUE);

    action_buttons.clear();
    for (size_t i = 0; i < actions.size(); ++i) {
        const auto& act = actions[i];

        GtkWidget* btn = gtk_button_new();
        gtk_widget_add_css_class(btn, "powermenu-tile");
        gtk_widget_add_css_class(btn, act.css_class.c_str());
        gtk_widget_set_size_request(btn, 122, 160);

        GtkWidget* tile_content = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
        gtk_widget_set_valign(tile_content, GTK_ALIGN_FILL);
        gtk_widget_set_halign(tile_content, GTK_ALIGN_FILL);

        // Top Row inside Tile: Hotkey Keycap Badge
        GtkWidget* top_tile_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
        GtkWidget* badge_lbl = gtk_label_new(act.hotkey.c_str());
        gtk_widget_add_css_class(badge_lbl, "powermenu-keycap");
        gtk_box_pack_end(GTK_BOX(top_tile_row), badge_lbl, FALSE, FALSE, 0);

        // Middle: Floating Icon Orb
        GtkWidget* orb_container = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
        gtk_widget_set_valign(orb_container, GTK_ALIGN_CENTER);
        gtk_widget_set_halign(orb_container, GTK_ALIGN_CENTER);
        gtk_widget_set_vexpand(orb_container, TRUE);

        GtkWidget* icon_orb = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
        gtk_widget_add_css_class(icon_orb, "powermenu-icon-orb");
        gtk_widget_set_size_request(icon_orb, 50, 50);

        GtkWidget* icon_lbl = gtk_label_new(act.icon.c_str());
        gtk_widget_add_css_class(icon_lbl, "powermenu-orb-icon");
        gtk_widget_set_halign(icon_lbl, GTK_ALIGN_CENTER);
        gtk_widget_set_valign(icon_lbl, GTK_ALIGN_CENTER);
        gtk_container_add(GTK_CONTAINER(icon_orb), icon_lbl);
        gtk_box_pack_start(GTK_BOX(orb_container), icon_orb, FALSE, FALSE, 0);

        // Bottom: Typography Labels
        GtkWidget* bottom_vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
        gtk_widget_set_halign(bottom_vbox, GTK_ALIGN_CENTER);

        GtkWidget* t_lbl = gtk_label_new(act.title.c_str());
        gtk_widget_add_css_class(t_lbl, "powermenu-tile-title");

        GtkWidget* sub_lbl = gtk_label_new(act.subtitle.c_str());
        gtk_widget_add_css_class(sub_lbl, "powermenu-tile-sub");

        gtk_box_pack_start(GTK_BOX(bottom_vbox), t_lbl, FALSE, FALSE, 0);
        gtk_box_pack_start(GTK_BOX(bottom_vbox), sub_lbl, FALSE, FALSE, 0);

        gtk_box_pack_start(GTK_BOX(tile_content), top_tile_row, FALSE, FALSE, 0);
        gtk_box_pack_start(GTK_BOX(tile_content), orb_container, TRUE, TRUE, 0);
        gtk_box_pack_end(GTK_BOX(tile_content), bottom_vbox, FALSE, FALSE, 2);

        gtk_container_add(GTK_CONTAINER(btn), tile_content);

        size_t idx = i;
        g_signal_connect(btn, "clicked", G_CALLBACK(+[](GtkButton*, gpointer data) {
            size_t a_idx = GPOINTER_TO_SIZE(data);
            PowerMenu::execute_action(a_idx);
        }), GSIZE_TO_POINTER(idx));

        action_buttons.push_back(btn);
        gtk_box_pack_start(GTK_BOX(tiles_row), btn, TRUE, TRUE, 0);
    }

    gtk_box_pack_start(GTK_BOX(card), tiles_row, TRUE, TRUE, 0);

    // ─── Footer Controls Bar ───
    GtkWidget* footer_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
    gtk_widget_add_css_class(footer_box, "powermenu-footer-box");
    gtk_widget_set_halign(footer_box, GTK_ALIGN_CENTER);

    GtkWidget* hint_kbd = gtk_label_new("󰌌 Press  [ L ] Lock   •   [ S ] Sleep   •   [ E ] Exit   •   [ R ] Restart   •   [ P ] Power Off");
    gtk_widget_add_css_class(hint_kbd, "powermenu-footer-kbd");

    GtkWidget* hint_esc = gtk_label_new("[ Esc ] Cancel");
    gtk_widget_add_css_class(hint_esc, "powermenu-footer-esc");

    gtk_box_pack_start(GTK_BOX(footer_box), hint_kbd, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(footer_box), hint_esc, FALSE, FALSE, 8);

    gtk_box_pack_start(GTK_BOX(card), footer_box, FALSE, FALSE, 0);

    gtk_box_pack_start(GTK_BOX(center_box), card, FALSE, FALSE, 0);
    gtk_container_add(GTK_CONTAINER(backdrop), center_box);
    gtk_container_add(GTK_CONTAINER(window), backdrop);

    // Keyboard Shortcuts Handler
    g_signal_connect(window, "key-press-event", G_CALLBACK(+[](GtkWidget*, GdkEventKey* event, gpointer) -> gboolean {
        guint key = event->keyval;

        if (key == GDK_KEY_Escape) {
            PowerMenu::hide();
            return TRUE;
        }

        // Hotkey mappings: L/1 (Lock), S/2 (Sleep), E/3 (Exit), R/4 (Reboot), P/5 (Power)
        if (key == GDK_KEY_l || key == GDK_KEY_L || key == GDK_KEY_1 || key == GDK_KEY_KP_1) {
            PowerMenu::execute_action(0);
            return TRUE;
        }
        if (key == GDK_KEY_s || key == GDK_KEY_S || key == GDK_KEY_2 || key == GDK_KEY_KP_2) {
            PowerMenu::execute_action(1);
            return TRUE;
        }
        if (key == GDK_KEY_e || key == GDK_KEY_E || key == GDK_KEY_3 || key == GDK_KEY_KP_3) {
            PowerMenu::execute_action(2);
            return TRUE;
        }
        if (key == GDK_KEY_r || key == GDK_KEY_R || key == GDK_KEY_4 || key == GDK_KEY_KP_4) {
            PowerMenu::execute_action(3);
            return TRUE;
        }
        if (key == GDK_KEY_p || key == GDK_KEY_P || key == GDK_KEY_5 || key == GDK_KEY_KP_5) {
            PowerMenu::execute_action(4);
            return TRUE;
        }

        return FALSE;
    }), nullptr);

    gtk_widget_show_all(window);
    gtk_widget_hide(window);
}

static int64_t last_power_hide_time_ms = 0;

void PowerMenu::toggle() {
    if (!window) return;
    int64_t now = g_get_monotonic_time() / 1000;
    if (now - last_power_hide_time_ms < 250) {
        return;
    }
    if (gtk_widget_get_visible(window)) {
        hide();
    } else {
        show();
    }
}

void PowerMenu::show() {
    if (!window) return;
    if (uptime_lbl) {
        std::string up = "󰔚  " + get_uptime_string();
        // Check battery if available
        std::ifstream bat("/sys/class/power_supply/BAT0/capacity");
        if (!bat.is_open()) bat.open("/sys/class/power_supply/BAT1/capacity");
        if (bat.is_open()) {
            std::string cap;
            bat >> cap;
            if (!cap.empty()) {
                up = "󰂂 " + cap + "%  •  " + up;
            }
        }
        gtk_label_set_text(GTK_LABEL(uptime_lbl), up.c_str());
    }
    gtk_widget_show(window);
    gtk_window_present(GTK_WINDOW(window));
}

void PowerMenu::hide() {
    if (!window) return;
    last_power_hide_time_ms = g_get_monotonic_time() / 1000;
    gtk_widget_hide(window);
}

} // namespace zenith
