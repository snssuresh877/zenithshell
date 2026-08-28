#include "shell/keybinds/keybinds_overlay.hpp"
#include "gtk3_compat.hpp"
#include <gtk-layer-shell/gtk-layer-shell.h>
#include <cairo.h>
#include <iostream>
#include <algorithm>

namespace zenith {

GtkWidget* KeybindsOverlay::window = nullptr;
GtkWidget* KeybindsOverlay::search_entry = nullptr;
GtkWidget* KeybindsOverlay::list_box = nullptr;
std::vector<KeybindEntry> KeybindsOverlay::keybinds;

void KeybindsOverlay::init(GtkApplication* app) {
    load_default_keybinds();
    create_window(app);
}

void KeybindsOverlay::load_default_keybinds() {
    keybinds = {
        // Navigation & Launchers
        {"SUPER + SPACE", "Open Spotlight Application Launcher", "Launchers"},
        {"SUPER + ENTER", "Open Foot Terminal", "Launchers"},
        {"SUPER + E", "Open Dolphin File Manager", "Launchers"},
        {"SUPER + B", "Open Default Web Browser", "Launchers"},
        {"SUPER + K", "Toggle Keybindings Cheatsheet", "Launchers"},

        // Windows & Workspaces
        {"SUPER + Q", "Close Active Window", "Window Management"},
        {"SUPER + F", "Toggle Fullscreen Mode", "Window Management"},
        {"SUPER + SHIFT + F", "Toggle Monocle Layout", "Window Management"},
        {"SUPER + D", "Dwindle Layout (Toggle Split)", "Window Management"},
        {"SUPER + M", "Toggle Floating Window Mode", "Window Management"},
        {"SUPER + 1..9", "Switch to Workspace 1 through 9", "Window Management"},
        {"SUPER + SHIFT + 1..9", "Move Window to Workspace 1 through 9", "Window Management"},

        // System & Quick Panels
        {"SUPER + W", "Toggle Top Bar Visibility (Full Space)", "System Panels"},
        {"SUPER + N", "Toggle Notification History Center", "System Panels"},
        {"SUPER + I", "Toggle Network & Wi-Fi Management", "System Panels"},
        {"SUPER + A", "Toggle Interactive Control Center", "System Panels"},
        {"SUPER + V", "Open Clipboard History Manager", "System Panels"},
        {"SUPER + R", "Toggle Reminder Notes Overlay", "System Panels"},
        {"SUPER + ALT + U", "Cycle UI Engine (ZenithShell ↔ Waybar)", "System Panels"},

        // System Controls (TUI & Quick Controls)
        {"SUPER + CTRL + A", "Audio Settings & Routing (Control Center)", "System Controls"},
        {"SUPER + CTRL + B", "Bluetooth Controls (bluetui)", "System Controls"},
        {"SUPER + CTRL + W", "Network & Wi-Fi Manager (nmcli)", "System Controls"},
        {"SUPER + CTRL + S", "Share Menu (LocalSend)", "System Controls"},
        {"SUPER + CTRL + T", "Activity Monitor (btop)", "System Controls"},
        {"SUPER + CTRL + C", "Capture Controls Menu (Screenshot/OCR)", "System Controls"},
        {"SUPER + CTRL + O", "System Toggles Menu", "System Controls"},
        {"SUPER + CTRL + H", "Hardware Reload & Diagnostics", "System Controls"},
        {"SUPER + CTRL + .", "Transcoding Menu (Media Convert)", "System Controls"},

        // AI & Utilities
        {"SUPER + O", "Launch Local AI Assistant (Ollama)", "AI & Utilities"},
        {"SUPER + SHIFT + A", "Explain Selected Code/Text with AI", "AI & Utilities"},
        {"Print", "Capture Active Window Screenshot", "Utilities"},
        {"SHIFT + Print", "Capture Interactive Area Screenshot", "Utilities"},
        {"SUPER + SHIFT + C", "Open Color Picker (Hyprpicker)", "Utilities"},
        {"SUPER + SHIFT + P", "Open Power Menu (Lock/Shutdown)", "Utilities"},
    };
}

void KeybindsOverlay::create_window(GtkApplication* app) {
    if (window) return;

    window = gtk_application_window_new(app);
    gtk_widget_add_css_class(window, "keybinds-window");
    gtk_window_set_default_size(GTK_WINDOW(window), 540, 520);

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

    gtk_layer_init_for_window(GTK_WINDOW(window));
    gtk_layer_set_layer(GTK_WINDOW(window), GTK_LAYER_SHELL_LAYER_TOP);
    gtk_layer_set_keyboard_mode(GTK_WINDOW(window), GTK_LAYER_SHELL_KEYBOARD_MODE_EXCLUSIVE);

    // Center on screen
    gtk_layer_set_anchor(GTK_WINDOW(window), GTK_LAYER_SHELL_EDGE_TOP, FALSE);
    gtk_layer_set_anchor(GTK_WINDOW(window), GTK_LAYER_SHELL_EDGE_BOTTOM, FALSE);
    gtk_layer_set_anchor(GTK_WINDOW(window), GTK_LAYER_SHELL_EDGE_LEFT, FALSE);
    gtk_layer_set_anchor(GTK_WINDOW(window), GTK_LAYER_SHELL_EDGE_RIGHT, FALSE);

    GtkWidget* card = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
    gtk_widget_add_css_class(card, "keybinds-card");

    // Header
    GtkWidget* header_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    GtkWidget* icon_lbl = gtk_label_new("󰌌");
    gtk_widget_add_css_class(icon_lbl, "keybinds-header-icon");

    GtkWidget* title_lbl = gtk_label_new("Keyboard Shortcuts");
    gtk_widget_add_css_class(title_lbl, "keybinds-header-title");
    gtk_widget_set_halign(title_lbl, GTK_ALIGN_START);

    GtkWidget* close_btn = gtk_button_new_with_label("󰅖");
    gtk_widget_add_css_class(close_btn, "keybinds-close-btn");
    g_signal_connect(close_btn, "clicked", G_CALLBACK(+[](GtkButton*, gpointer) {
        KeybindsOverlay::hide();
    }), nullptr);

    gtk_box_pack_start(GTK_BOX(header_box), icon_lbl, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(header_box), title_lbl, TRUE, TRUE, 0);
    gtk_box_pack_end(GTK_BOX(header_box), close_btn, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(card), header_box, FALSE, FALSE, 0);

    // Search bar
    search_entry = gtk_entry_new();
    gtk_widget_add_css_class(search_entry, "keybinds-search-entry");
    gtk_entry_set_placeholder_text(GTK_ENTRY(search_entry), "Search shortcuts (e.g. window, terminal, screenshot)...");
    gtk_box_pack_start(GTK_BOX(card), search_entry, FALSE, FALSE, 0);

    g_signal_connect(search_entry, "changed", G_CALLBACK(+[](GtkEditable* editable, gpointer) {
        const char* text = gtk_entry_get_text(GTK_ENTRY(editable));
        KeybindsOverlay::filter_keybinds(text ? text : "");
    }), nullptr);

    // Scroll container
    GtkWidget* scroll = gtk_scrolled_window_new(nullptr, nullptr);
    gtk_widget_add_css_class(scroll, "keybinds-scroll");
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_widget_set_size_request(scroll, -1, 380);

    list_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    gtk_container_add(GTK_CONTAINER(scroll), list_box);
    gtk_box_pack_start(GTK_BOX(card), scroll, TRUE, TRUE, 0);

    gtk_container_add(GTK_CONTAINER(window), card);

    // Escape listener
    g_signal_connect(window, "key-press-event", G_CALLBACK(+[](GtkWidget*, GdkEventKey* event, gpointer) -> gboolean {
        if (event->keyval == GDK_KEY_Escape) {
            KeybindsOverlay::hide();
            return TRUE;
        }
        return FALSE;
    }), nullptr);
}

void KeybindsOverlay::toggle() {
    if (!window) return;
    if (gtk_widget_get_visible(window)) {
        hide();
    } else {
        show();
    }
}

void KeybindsOverlay::show() {
    if (!window) return;
    if (search_entry) {
        gtk_entry_set_text(GTK_ENTRY(search_entry), "");
        gtk_widget_grab_focus(search_entry);
    }
    filter_keybinds("");
    gtk_widget_show_all(window);
}

void KeybindsOverlay::hide() {
    if (!window) return;
    gtk_widget_hide(window);
}

void KeybindsOverlay::filter_keybinds(const std::string& query) {
    if (!list_box) return;

    GList* children = gtk_container_get_children(GTK_CONTAINER(list_box));
    for (GList* l = children; l != nullptr; l = l->next) {
        gtk_widget_destroy(GTK_WIDGET(l->data));
    }
    g_list_free(children);

    std::string q_lower = query;
    std::transform(q_lower.begin(), q_lower.end(), q_lower.begin(), ::tolower);

    std::string current_cat = "";

    for (const auto& entry : keybinds) {
        std::string k_lower = entry.keys;
        std::string d_lower = entry.description;
        std::string c_lower = entry.category;
        std::transform(k_lower.begin(), k_lower.end(), k_lower.begin(), ::tolower);
        std::transform(d_lower.begin(), d_lower.end(), d_lower.begin(), ::tolower);
        std::transform(c_lower.begin(), c_lower.end(), c_lower.begin(), ::tolower);

        if (!q_lower.empty() &&
            k_lower.find(q_lower) == std::string::npos &&
            d_lower.find(q_lower) == std::string::npos &&
            c_lower.find(q_lower) == std::string::npos) {
            continue;
        }

        // Category header if changed
        if (entry.category != current_cat && q_lower.empty()) {
            current_cat = entry.category;
            GtkWidget* cat_lbl = gtk_label_new(current_cat.c_str());
            gtk_widget_add_css_class(cat_lbl, "keybinds-cat-header");
            gtk_widget_set_halign(cat_lbl, GTK_ALIGN_START);
            gtk_box_pack_start(GTK_BOX(list_box), cat_lbl, FALSE, FALSE, 4);
        }

        build_entry(entry);
    }

    gtk_widget_show_all(list_box);
}

void KeybindsOverlay::build_entry(const KeybindEntry& entry) {
    GtkWidget* row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
    gtk_widget_add_css_class(row, "keybinds-row-card");

    // Description text (Left)
    GtkWidget* desc = gtk_label_new(entry.description.c_str());
    gtk_widget_add_css_class(desc, "keybinds-row-desc");
    gtk_widget_set_halign(desc, GTK_ALIGN_START);
    gtk_box_pack_start(GTK_BOX(row), desc, TRUE, TRUE, 0);

    // Key badge pill (Right)
    GtkWidget* key_badge = gtk_label_new(entry.keys.c_str());
    gtk_widget_add_css_class(key_badge, "keybinds-row-keys");
    gtk_box_pack_end(GTK_BOX(row), key_badge, FALSE, FALSE, 0);

    gtk_box_pack_start(GTK_BOX(list_box), row, FALSE, FALSE, 0);
}

} // namespace zenith
