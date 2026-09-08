#include "shell/clipboard/clipboard_manager.hpp"
#include "gtk3_compat.hpp"
#include <gtk-layer-shell/gtk-layer-shell.h>
#include <gdk/gdk.h>
#include <gdk/gdkkeysyms.h>
#include <cairo.h>
#include <nlohmann/json.hpp>
#include <iostream>
#include <fstream>
#include <filesystem>
#include <ctime>
#include <algorithm>

namespace fs = std::filesystem;
using json = nlohmann::json;

namespace zenith {

GtkWidget* ClipboardManager::window = nullptr;
GtkWidget* ClipboardManager::search_entry = nullptr;
GtkWidget* ClipboardManager::listbox = nullptr;
GtkWidget* ClipboardManager::badge_lbl = nullptr;
GtkWidget* ClipboardManager::empty_box = nullptr;
std::deque<ClipItem> ClipboardManager::history;
std::string ClipboardManager::last_copied = "";
std::vector<int> ClipboardManager::filtered_indices;
GtkClipboard* ClipboardManager::gtk_clip = nullptr;

namespace {

std::string get_history_file_path() {
    const char* home = g_get_home_dir();
    if (!home) return "";
    std::string dir = std::string(home) + "/.local/state/zenithshell";
    std::error_code ec;
    fs::create_directories(dir, ec);
    return dir + "/clipboard_history.json";
}

} // namespace

void ClipboardManager::load_history() {
    std::string path = get_history_file_path();
    if (path.empty() || !fs::exists(path)) return;

    try {
        std::ifstream file(path);
        if (!file.is_open()) return;

        json j;
        file >> j;
        if (j.is_array()) {
            history.clear();
            for (const auto& item : j) {
                if (item.contains("text") && item.contains("timestamp")) {
                    history.push_back({
                        item["text"].get<std::string>(),
                        item["timestamp"].get<std::string>()
                    });
                }
            }
            if (!history.empty()) {
                last_copied = history.front().text;
            }
            std::cout << "[ClipboardManager] Loaded " << history.size() << " clipboard items from disk." << std::endl;
        }
    } catch (const std::exception& e) {
        std::cerr << "[ClipboardManager] Failed to parse history file: " << e.what() << std::endl;
    }
}

void ClipboardManager::save_history() {
    std::string path = get_history_file_path();
    if (path.empty()) return;

    try {
        json j = json::array();
        for (const auto& item : history) {
            j.push_back({
                {"text", item.text},
                {"timestamp", item.timestamp}
            });
        }
        std::ofstream file(path, std::ios::trunc);
        if (file.is_open()) {
            file << j.dump(2);
        }
    } catch (const std::exception& e) {
        std::cerr << "[ClipboardManager] Failed to save history: " << e.what() << std::endl;
    }
}

void ClipboardManager::init(GtkApplication* app) {
    window = gtk_application_window_new(app);
    gtk_widget_add_css_class(window, "clipboard-window");
    gtk_window_set_default_size(GTK_WINDOW(window), 540, 500);

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
    gtk_layer_set_keyboard_mode(GTK_WINDOW(window), GTK_LAYER_SHELL_KEYBOARD_MODE_NONE);

    // Centered overlay card
    gtk_layer_set_anchor(GTK_WINDOW(window), GTK_LAYER_SHELL_EDGE_TOP, TRUE);
    gtk_layer_set_anchor(GTK_WINDOW(window), GTK_LAYER_SHELL_EDGE_BOTTOM, FALSE);
    gtk_layer_set_anchor(GTK_WINDOW(window), GTK_LAYER_SHELL_EDGE_LEFT, FALSE);
    gtk_layer_set_anchor(GTK_WINDOW(window), GTK_LAYER_SHELL_EDGE_RIGHT, FALSE);
    gtk_layer_set_margin(GTK_WINDOW(window), GTK_LAYER_SHELL_EDGE_TOP, 70);

    GtkWidget* card = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
    gtk_widget_add_css_class(card, "clipboard-card");

    // Header with Icon, Title, Count Badge, Clear Button, and Close Button
    GtkWidget* header = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_widget_add_css_class(header, "clipboard-header");

    GtkWidget* icon_lbl = gtk_label_new("󰅍");
    gtk_widget_add_css_class(icon_lbl, "clipboard-header-icon");

    GtkWidget* title = gtk_label_new("Clipboard History");
    gtk_widget_add_css_class(title, "clipboard-header-title");
    gtk_widget_set_halign(title, GTK_ALIGN_START);

    badge_lbl = gtk_label_new("0 clips");
    gtk_widget_add_css_class(badge_lbl, "clipboard-badge");

    GtkWidget* clear_btn = gtk_button_new_with_label("󰃢 Clear");
    gtk_widget_add_css_class(clear_btn, "clipboard-clear-btn");
    g_signal_connect(clear_btn, "clicked", G_CALLBACK(+[](GtkButton*, gpointer) {
        ClipboardManager::clear_history();
    }), nullptr);

    GtkWidget* close_btn = gtk_button_new_with_label("󰅖");
    gtk_widget_add_css_class(close_btn, "clipboard-close-btn");
    g_signal_connect(close_btn, "clicked", G_CALLBACK(+[](GtkButton*, gpointer) {
        ClipboardManager::hide();
    }), nullptr);

    gtk_box_pack_start(GTK_BOX(header), icon_lbl, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(header), title, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(header), badge_lbl, FALSE, FALSE, 0);
    gtk_box_pack_end(GTK_BOX(header), close_btn, FALSE, FALSE, 0);
    gtk_box_pack_end(GTK_BOX(header), clear_btn, FALSE, FALSE, 0);

    gtk_box_pack_start(GTK_BOX(card), header, FALSE, FALSE, 0);

    // Search Bar
    search_entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(search_entry), "Search copied text, links, code, commands...");
    gtk_widget_add_css_class(search_entry, "clipboard-search");
    gtk_box_pack_start(GTK_BOX(card), search_entry, FALSE, FALSE, 0);

    g_signal_connect(search_entry, "changed", G_CALLBACK(+[](GtkEditable* editable, gpointer) {
        const char* text = gtk_entry_get_text(GTK_ENTRY(editable));
        ClipboardManager::render_list(text ? text : "");
    }), nullptr);

    // Stack or Scroll Container
    GtkWidget* scroll = gtk_scrolled_window_new(nullptr, nullptr);
    gtk_widget_add_css_class(scroll, "clipboard-scroll");
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_widget_set_size_request(scroll, -1, 360);

    GtkWidget* content_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);

    listbox = gtk_list_box_new();
    gtk_widget_add_css_class(listbox, "clipboard-list");
    gtk_list_box_set_selection_mode(GTK_LIST_BOX(listbox), GTK_SELECTION_SINGLE);

    g_signal_connect(listbox, "row-activated", G_CALLBACK(+[](GtkListBox*, GtkListBoxRow* row, gpointer) {
        int row_idx = gtk_list_box_row_get_index(row);
        if (row_idx >= 0 && static_cast<size_t>(row_idx) < filtered_indices.size()) {
            ClipboardManager::paste_item(filtered_indices[row_idx]);
        }
    }), nullptr);

    // Empty state widget
    empty_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_widget_add_css_class(empty_box, "clipboard-empty-box");

    GtkWidget* empty_icon = gtk_label_new("󰅍");
    gtk_widget_add_css_class(empty_icon, "clipboard-empty-icon");

    GtkWidget* empty_title = gtk_label_new("No Clipboard History");
    gtk_widget_add_css_class(empty_title, "clipboard-empty-title");

    GtkWidget* empty_sub = gtk_label_new("Copied text and snippets will appear here automatically.");
    gtk_widget_add_css_class(empty_sub, "clipboard-empty-sub");

    gtk_box_pack_start(GTK_BOX(empty_box), empty_icon, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(empty_box), empty_title, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(empty_box), empty_sub, FALSE, FALSE, 0);

    gtk_box_pack_start(GTK_BOX(content_box), listbox, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(content_box), empty_box, TRUE, TRUE, 0);

    gtk_container_add(GTK_CONTAINER(scroll), content_box);
    gtk_box_pack_start(GTK_BOX(card), scroll, TRUE, TRUE, 0);

    gtk_container_add(GTK_CONTAINER(window), card);

    // Keyboard navigation (Escape, Up, Down, Enter, Delete)
    g_signal_connect(window, "key-press-event", G_CALLBACK(+[](GtkWidget*, GdkEventKey* event, gpointer) -> gboolean {
        if (event->keyval == GDK_KEY_Escape) {
            ClipboardManager::hide();
            return TRUE;
        }
        if (event->keyval == GDK_KEY_Down) {
            GtkListBoxRow* selected = gtk_list_box_get_selected_row(GTK_LIST_BOX(listbox));
            int next_idx = selected ? gtk_list_box_row_get_index(selected) + 1 : 0;
            GtkListBoxRow* target = gtk_list_box_get_row_at_index(GTK_LIST_BOX(listbox), next_idx);
            if (target) {
                gtk_list_box_select_row(GTK_LIST_BOX(listbox), target);
                gtk_widget_grab_focus(GTK_WIDGET(target));
            }
            return TRUE;
        }
        if (event->keyval == GDK_KEY_Up) {
            GtkListBoxRow* selected = gtk_list_box_get_selected_row(GTK_LIST_BOX(listbox));
            int prev_idx = selected ? gtk_list_box_row_get_index(selected) - 1 : 0;
            if (prev_idx >= 0) {
                GtkListBoxRow* target = gtk_list_box_get_row_at_index(GTK_LIST_BOX(listbox), prev_idx);
                if (target) {
                    gtk_list_box_select_row(GTK_LIST_BOX(listbox), target);
                    gtk_widget_grab_focus(GTK_WIDGET(target));
                }
            } else if (search_entry) {
                gtk_widget_grab_focus(search_entry);
            }
            return TRUE;
        }
        if (event->keyval == GDK_KEY_Delete && (event->state & GDK_SHIFT_MASK)) {
            GtkListBoxRow* selected = gtk_list_box_get_selected_row(GTK_LIST_BOX(listbox));
            if (selected) {
                int row_idx = gtk_list_box_row_get_index(selected);
                if (row_idx >= 0 && static_cast<size_t>(row_idx) < filtered_indices.size()) {
                    ClipboardManager::delete_item(filtered_indices[row_idx]);
                    return TRUE;
                }
            }
        }
        if (event->keyval == GDK_KEY_Return || event->keyval == GDK_KEY_KP_Enter) {
            GtkListBoxRow* selected = gtk_list_box_get_selected_row(GTK_LIST_BOX(listbox));
            if (!selected) {
                selected = gtk_list_box_get_row_at_index(GTK_LIST_BOX(listbox), 0);
            }
            if (selected) {
                int row_idx = gtk_list_box_row_get_index(selected);
                if (row_idx >= 0 && static_cast<size_t>(row_idx) < filtered_indices.size()) {
                    ClipboardManager::paste_item(filtered_indices[row_idx]);
                    return TRUE;
                }
            }
        }
        return FALSE;
    }), nullptr);

    // Native Wayland GtkClipboard Event Listener (Zero-CPU idle overhead)
    gtk_clip = gtk_clipboard_get_default(gdk_display_get_default());
    if (!gtk_clip) {
        gtk_clip = gtk_clipboard_get(GDK_SELECTION_CLIPBOARD);
    }

    if (gtk_clip) {
        g_signal_connect(gtk_clip, "owner-change", G_CALLBACK(+[](GtkClipboard* clip, GdkEventOwnerChange*, gpointer) {
            gtk_clipboard_request_text(clip, +[](GtkClipboard*, const gchar* text, gpointer) {
                if (text && *text) {
                    ClipboardManager::add_item(text);
                }
            }, nullptr);
        }), nullptr);
    }

    // Load persisted history from disk
    load_history();

    // Terminate legacy cliphist watchers and spawn native event-driven watcher
    g_spawn_command_line_async("pkill -f 'wl-paste.*cliphist'", nullptr);
    g_spawn_command_line_async("pkill -f 'wl-paste.*--clip-store'", nullptr);
    g_spawn_command_line_async("wl-paste --watch zenithshell --clip-store", nullptr);
}

void ClipboardManager::toggle() {
    if (!window) return;
    if (gtk_widget_get_visible(window)) hide();
    else show();
}

void ClipboardManager::show() {
    if (!window) return;
    gtk_layer_set_keyboard_mode(GTK_WINDOW(window), GTK_LAYER_SHELL_KEYBOARD_MODE_EXCLUSIVE);
    if (search_entry) {
        gtk_entry_set_text(GTK_ENTRY(search_entry), "");
    }

    // Sync latest text if any clipboard change occurred while hidden
    if (gtk_clip) {
        gtk_clipboard_request_text(gtk_clip, +[](GtkClipboard*, const gchar* text, gpointer) {
            if (text && *text) {
                ClipboardManager::add_item(text);
            }
        }, nullptr);
    }

    render_list("");
    gtk_widget_show_all(window);
    if (search_entry) {
        gtk_widget_grab_focus(search_entry);
    }
}

void ClipboardManager::hide() {
    if (!window) return;
    gtk_layer_set_keyboard_mode(GTK_WINDOW(window), GTK_LAYER_SHELL_KEYBOARD_MODE_NONE);
    gtk_widget_hide(window);
}

void ClipboardManager::add_item(const std::string& text, const std::string& timestamp) {
    if (text.empty() || text == last_copied) return;

    // Deduplicate identical items
    history.erase(std::remove_if(history.begin(), history.end(), [&](const ClipItem& item) {
        return item.text == text;
    }), history.end());

    std::string t = timestamp;
    if (t.empty()) {
        auto now = std::time(nullptr);
        char tbuf[16];
        std::strftime(tbuf, sizeof(tbuf), "%H:%M", std::localtime(&now));
        t = std::string(tbuf);
    }

    history.push_front({text, t});
    if (history.size() > 100) history.pop_back();
    last_copied = text;

    save_history();

    if (window && gtk_widget_get_visible(window)) {
        render_list(search_entry ? gtk_entry_get_text(GTK_ENTRY(search_entry)) : "");
    }
}

void ClipboardManager::delete_item(int index) {
    if (index < 0 || static_cast<size_t>(index) >= history.size()) return;
    history.erase(history.begin() + index);
    save_history();
    render_list(search_entry ? gtk_entry_get_text(GTK_ENTRY(search_entry)) : "");
}

void ClipboardManager::paste_item(int index) {
    if (index < 0 || static_cast<size_t>(index) >= history.size()) return;
    std::string text = history[index].text;
    hide();

    last_copied = text;

    // Set clipboard in GTK and store in memory (persists even if apps close)
    if (gtk_clip) {
        gtk_clipboard_set_text(gtk_clip, text.c_str(), static_cast<gint>(text.length()));
        gtk_clipboard_store(gtk_clip);
    }

    // Fallback direct Wayland pipe
    FILE* fp = popen("wl-copy 2>/dev/null", "w");
    if (fp) {
        fwrite(text.c_str(), 1, text.size(), fp);
        pclose(fp);
    }
}

void ClipboardManager::clear_history() {
    history.clear();
    last_copied = "";
    filtered_indices.clear();
    save_history();
    render_list("");
}

void ClipboardManager::render_list(const std::string& filter_text) {
    if (!listbox) return;

    filtered_indices.clear();

    GList* children = gtk_container_get_children(GTK_CONTAINER(listbox));
    for (GList* l = children; l != nullptr; l = l->next) {
        gtk_widget_destroy(GTK_WIDGET(l->data));
    }
    g_list_free(children);

    std::string query = filter_text;
    if (query.empty() && search_entry) {
        const char* q = gtk_entry_get_text(GTK_ENTRY(search_entry));
        if (q) query = q;
    }
    std::transform(query.begin(), query.end(), query.begin(), ::tolower);

    // Update count badge
    if (badge_lbl) {
        std::string count_str = std::to_string(history.size()) + (history.size() == 1 ? " clip" : " clips");
        gtk_label_set_text(GTK_LABEL(badge_lbl), count_str.c_str());
    }

    int match_count = 0;

    for (size_t i = 0; i < history.size(); ++i) {
        const auto& item = history[i];
        std::string item_lower = item.text;
        std::transform(item_lower.begin(), item_lower.end(), item_lower.begin(), ::tolower);

        if (!query.empty() && item_lower.find(query) == std::string::npos) continue;

        filtered_indices.push_back(static_cast<int>(i));
        match_count++;

        GtkWidget* row_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
        gtk_widget_add_css_class(row_box, "clipboard-item");

        // Icon based on content type
        const char* icon_char = "󰅍";
        if (item.text.find("http://") == 0 || item.text.find("https://") == 0) {
            icon_char = "";
        } else if (item.text.find('\n') != std::string::npos || item.text.find('{') != std::string::npos || item.text.find("def ") != std::string::npos) {
            icon_char = "";
        }

        GtkWidget* icon = gtk_label_new(icon_char);
        gtk_widget_add_css_class(icon, "clip-icon");

        // Single line preview
        std::string clean_preview = item.text;
        std::replace(clean_preview.begin(), clean_preview.end(), '\n', ' ');
        std::replace(clean_preview.begin(), clean_preview.end(), '\t', ' ');

        GtkWidget* text_lbl = gtk_label_new(clean_preview.c_str());
        gtk_widget_add_css_class(text_lbl, "clip-text");
        gtk_label_set_ellipsize(GTK_LABEL(text_lbl), PANGO_ELLIPSIZE_END);
        gtk_label_set_max_width_chars(GTK_LABEL(text_lbl), 46);
        gtk_widget_set_halign(text_lbl, GTK_ALIGN_START);

        GtkWidget* time_lbl = gtk_label_new(item.timestamp.c_str());
        gtk_widget_add_css_class(time_lbl, "clip-time");

        // Per-item delete button
        GtkWidget* del_btn = gtk_button_new_with_label("󰅖");
        gtk_widget_add_css_class(del_btn, "clip-item-del-btn");
        gtk_widget_set_tooltip_text(del_btn, "Delete clip");
        g_signal_connect(del_btn, "clicked", G_CALLBACK(+[](GtkButton*, gpointer data) {
            int clip_idx = GPOINTER_TO_INT(data);
            ClipboardManager::delete_item(clip_idx);
        }), GINT_TO_POINTER(static_cast<int>(i)));

        gtk_box_pack_start(GTK_BOX(row_box), icon, FALSE, FALSE, 0);
        gtk_box_pack_start(GTK_BOX(row_box), text_lbl, TRUE, TRUE, 0);
        gtk_box_pack_end(GTK_BOX(row_box), del_btn, FALSE, FALSE, 0);
        gtk_box_pack_end(GTK_BOX(row_box), time_lbl, FALSE, FALSE, 0);

        gtk_container_add(GTK_CONTAINER(listbox), row_box);
    }

    if (empty_box) {
        if (match_count == 0) {
            gtk_widget_show(empty_box);
            gtk_widget_hide(listbox);
        } else {
            gtk_widget_hide(empty_box);
            gtk_widget_show(listbox);
        }
    }

    gtk_widget_show_all(listbox);
}

} // namespace zenith
