#include "file_manager_window.hpp"
#include "path_bar_widget.hpp"
#include "places_sidebar.hpp"
#include "file_view_widget.hpp"
#include "file_item.hpp"
#include "file_operations.hpp"
#include "../../gtk3_compat.hpp"

#include <sys/statvfs.h>
#include <gdk/gdkkeysyms.h>
#include <vector>
#include <iostream>
#include <cstring>
#include <memory>
#include <algorithm>

namespace zenith {

struct PaneState {
    GtkWidget* file_view{nullptr};
    std::string current_path;
    std::vector<std::string> back_history;
    std::vector<std::string> forward_history;
    int last_total_items{0};
    int last_selected_items{0};
    uint64_t last_selected_bytes{0};
};

struct TabState {
    GtkWidget* paned{nullptr};
    GtkWidget* tab_box{nullptr};
    GtkWidget* tab_label{nullptr};
    PaneState left_pane;
    PaneState right_pane;
    bool is_dual{false};
    PaneState* active_pane{nullptr};
};

struct FileManagerState {
    GtkWidget* window{nullptr};
    GtkWidget* btn_back{nullptr};
    GtkWidget* btn_forward{nullptr};
    GtkWidget* btn_up{nullptr};
    GtkWidget* btn_home{nullptr};
    GtkWidget* path_bar{nullptr};
    GtkWidget* btn_search{nullptr};
    GtkWidget* search_revealer{nullptr};
    GtkWidget* search_entry{nullptr};
    GtkWidget* btn_new_folder{nullptr};
    GtkWidget* btn_new_tab{nullptr};
    GtkWidget* btn_dual_pane{nullptr};
    GtkWidget* btn_term{nullptr};
    GtkWidget* btn_hidden{nullptr};
    GtkWidget* btn_grid_view{nullptr};
    GtkWidget* btn_list_view{nullptr};
    GtkWidget* btn_settings{nullptr};

    GtkWidget* paned{nullptr};
    GtkWidget* sidebar{nullptr};
    
    GtkWidget* notebook{nullptr};
    std::vector<std::unique_ptr<TabState>> tabs;
    TabState* active_tab{nullptr};

    GtkWidget* status_label{nullptr};
    GtkWidget* disk_label{nullptr};
};

static void update_nav_buttons(FileManagerState* state) {
    if (!state || !state->active_tab) return;
    gtk_widget_set_sensitive(state->btn_back, !state->active_tab->active_pane->back_history.empty());
    gtk_widget_set_sensitive(state->btn_forward, !state->active_tab->active_pane->forward_history.empty());
    gtk_widget_set_sensitive(state->btn_up, state->active_tab->active_pane->current_path != "/");
}

static void update_disk_space(FileManagerState* state, const std::string& path) {
    if (!state || !state->disk_label) return;

    struct statvfs vfs;
    if (statvfs(path.c_str(), &vfs) == 0) {
        uint64_t free_bytes = static_cast<uint64_t>(vfs.f_bavail) * vfs.f_frsize;
        std::string s = FileItem::format_size(free_bytes) + " available";
        gtk_label_set_text(GTK_LABEL(state->disk_label), s.c_str());
    } else {
        gtk_label_set_text(GTK_LABEL(state->disk_label), "");
    }
}

static void update_status_text(FileManagerState* state) {
    if (!state || !state->status_label || !state->active_tab) return;

    std::string text;
    if (state->active_tab->active_pane->last_selected_items > 0) {
        text = std::to_string(state->active_tab->active_pane->last_selected_items) + " of " +
               std::to_string(state->active_tab->active_pane->last_total_items) + " selected";
        if (state->active_tab->active_pane->last_selected_bytes > 0) {
            text += " (" + FileItem::format_size(state->active_tab->active_pane->last_selected_bytes) + ")";
        }
    } else {
        text = std::to_string(state->active_tab->active_pane->last_total_items) + (state->active_tab->active_pane->last_total_items == 1 ? " item" : " items");
    }

    gtk_label_set_text(GTK_LABEL(state->status_label), text.c_str());
}

static void navigate_to(FileManagerState* state, const std::string& path, bool record_history = true) {
    if (!state || !state->active_tab || path.empty()) return;

    if (record_history && !state->active_tab->active_pane->current_path.empty() && state->active_tab->active_pane->current_path != path) {
        state->active_tab->active_pane->back_history.push_back(state->active_tab->active_pane->current_path);
        state->active_tab->active_pane->forward_history.clear();
    }

    state->active_tab->active_pane->current_path = path;

    // Update tab title
    std::string title = path;
    const char* home = g_get_home_dir();
    if (home && title.rfind(home, 0) == 0) {
        title = "~" + title.substr(strlen(home));
    }
    std::string base_name = title;
    size_t last_slash = title.find_last_of('/');
    if (last_slash != std::string::npos && last_slash != title.length() - 1) {
        base_name = title.substr(last_slash + 1);
    }
    if (base_name.empty()) base_name = "/";
    if (state->active_tab->tab_label) gtk_label_set_text(GTK_LABEL(state->active_tab->tab_label), base_name.c_str());

    // Update window title
    gtk_window_set_title(GTK_WINDOW(state->window), (title + " — Zenith Files").c_str());

    update_nav_buttons(state);
    PathBarWidget::set_path(state->path_bar, path);
    if (state->active_tab->active_pane == &state->active_tab->left_pane) PlacesSidebar::set_active_path(state->sidebar, path);
    FileViewWidget::load_directory(state->active_tab->active_pane->file_view, path);
    update_disk_space(state, path);
}

static void go_back(FileManagerState* state) {
    if (!state || !state->active_tab || state->active_tab->active_pane->back_history.empty()) return;

    std::string prev = state->active_tab->active_pane->back_history.back();
    state->active_tab->active_pane->back_history.pop_back();

    if (!state->active_tab->active_pane->current_path.empty()) {
        state->active_tab->active_pane->forward_history.push_back(state->active_tab->active_pane->current_path);
    }

    navigate_to(state, prev, false);
}

static void go_forward(FileManagerState* state) {
    if (!state || !state->active_tab || state->active_tab->active_pane->forward_history.empty()) return;

    std::string next = state->active_tab->active_pane->forward_history.back();
    state->active_tab->active_pane->forward_history.pop_back();

    if (!state->active_tab->active_pane->current_path.empty()) {
        state->active_tab->active_pane->back_history.push_back(state->active_tab->active_pane->current_path);
    }

    navigate_to(state, next, false);
}

static void go_up(FileManagerState* state) {
    if (!state || !state->active_tab || state->active_tab->active_pane->current_path.empty() || state->active_tab->active_pane->current_path == "/") return;

    GFile* cur = g_file_new_for_path(state->active_tab->active_pane->current_path.c_str());
    GFile* parent = g_file_get_parent(cur);
    if (parent) {
        char* parent_path = g_file_get_path(parent);
        if (parent_path) {
            navigate_to(state, parent_path, true);
            g_free(parent_path);
        }
        g_object_unref(parent);
    }
    g_object_unref(cur);
}

static void go_home(FileManagerState* state) {
    const char* home = g_get_home_dir();
    if (home) {
        navigate_to(state, home, true);
    }
}

static void close_tab(FileManagerState* state, TabState* tab) {
    if (!state || !tab || state->tabs.size() <= 1) return; // Don't close the last tab

    int page_num = gtk_notebook_page_num(GTK_NOTEBOOK(state->notebook), tab->paned);
    if (page_num >= 0) {
        gtk_notebook_remove_page(GTK_NOTEBOOK(state->notebook), page_num);
    }
    
    auto it = std::find_if(state->tabs.begin(), state->tabs.end(), [tab](const std::unique_ptr<TabState>& t) {
        return t.get() == tab;
    });
    
    if (it != state->tabs.end()) {
        state->tabs.erase(it);
    }
}

static void create_pane_view(FileManagerState* state, TabState* tab_ptr, PaneState* pane) {
    pane->file_view = FileViewWidget::create(
        [state, tab_ptr, pane](const std::string& target) {
            if (state->active_tab == tab_ptr && tab_ptr->active_pane == pane) {
                navigate_to(state, target, true);
            } else {
                if (!pane->current_path.empty() && pane->current_path != target) {
                    pane->back_history.push_back(pane->current_path);
                    pane->forward_history.clear();
                }
                pane->current_path = target;
                if (tab_ptr->active_pane == pane && state->active_tab == tab_ptr) {
                    std::string base_name = target;
                    size_t last_slash = target.find_last_of('/');
                    if (last_slash != std::string::npos && last_slash != target.length() - 1) {
                        base_name = target.substr(last_slash + 1);
                    }
                    if (base_name.empty()) base_name = "/";
                    if (tab_ptr->tab_label) gtk_label_set_text(GTK_LABEL(tab_ptr->tab_label), base_name.c_str());
                }
                FileViewWidget::load_directory(pane->file_view, target);
            }
        },
        [state, tab_ptr, pane](int total, int sel_count, uint64_t sel_bytes) {
            pane->last_total_items = total;
            pane->last_selected_items = sel_count;
            pane->last_selected_bytes = sel_bytes;
            if (state->active_tab == tab_ptr && tab_ptr->active_pane == pane) {
                update_status_text(state);
            }
        }
    );
    auto focus_cb = +[](GtkWidget*, GdkEventFocus*, gpointer user_data) -> gboolean {
        auto* d = static_cast<std::pair<FileManagerState*, std::pair<TabState*, PaneState*>>*>(user_data);
        d->second.first->active_pane = d->second.second;
        if (d->first->active_tab == d->second.first) {
            PathBarWidget::set_path(d->first->path_bar, d->second.second->current_path);
            update_nav_buttons(d->first);
            update_status_text(d->first);
            update_disk_space(d->first, d->second.second->current_path);
        }
        return FALSE;
    };
    g_signal_connect_data(pane->file_view, "focus-in-event", G_CALLBACK(focus_cb), new std::pair<FileManagerState*, std::pair<TabState*, PaneState*>>(state, {tab_ptr, pane}), [](gpointer d, GClosure*) {
        delete static_cast<std::pair<FileManagerState*, std::pair<TabState*, PaneState*>>*>(d);
    }, static_cast<GConnectFlags>(0));
}

static void toggle_dual_pane(FileManagerState* state) {
    if (!state || !state->active_tab) return;
    auto* tab = state->active_tab;
    tab->is_dual = !tab->is_dual;
    if (tab->is_dual) {
        create_pane_view(state, tab, &tab->right_pane);
        gtk_widget_show_all(tab->right_pane.file_view);
        gtk_paned_pack2(GTK_PANED(tab->paned), tab->right_pane.file_view, TRUE, FALSE);
        std::string cur = tab->left_pane.current_path;
        tab->active_pane = &tab->right_pane;
        navigate_to(state, cur, false);
        gtk_widget_grab_focus(tab->right_pane.file_view);
    } else {
        gtk_widget_destroy(tab->right_pane.file_view);
        tab->right_pane.file_view = nullptr;
        tab->active_pane = &tab->left_pane;
        PathBarWidget::set_path(state->path_bar, tab->active_pane->current_path);
        update_nav_buttons(state);
        update_status_text(state);
        gtk_widget_grab_focus(tab->left_pane.file_view);
    }
}

static void create_new_tab(FileManagerState* state, const std::string& initial_path) {
    auto tab = std::make_unique<TabState>();
    TabState* tab_ptr = tab.get();
    
    tab_ptr->paned = gtk_paned_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_widget_show(tab_ptr->paned);

    create_pane_view(state, tab_ptr, &tab_ptr->left_pane);
    tab_ptr->active_pane = &tab_ptr->left_pane;
    gtk_paned_pack1(GTK_PANED(tab_ptr->paned), tab_ptr->left_pane.file_view, TRUE, FALSE);
    
    GtkWidget* tab_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
    tab_ptr->tab_box = tab_box;
    
    GtkWidget* icon = gtk_image_new_from_icon_name("folder-symbolic", GTK_ICON_SIZE_MENU);
    gtk_box_pack_start(GTK_BOX(tab_box), icon, FALSE, FALSE, 0);

    tab_ptr->tab_label = gtk_label_new("New Tab");
    gtk_box_pack_start(GTK_BOX(tab_box), tab_ptr->tab_label, TRUE, TRUE, 0);

    GtkWidget* close_btn = gtk_button_new_from_icon_name("window-close-symbolic", GTK_ICON_SIZE_MENU);
    gtk_button_set_relief(GTK_BUTTON(close_btn), GTK_RELIEF_NONE);
    gtk_box_pack_start(GTK_BOX(tab_box), close_btn, FALSE, FALSE, 0);

    gtk_widget_show_all(tab_box);
    
    int index = gtk_notebook_append_page(GTK_NOTEBOOK(state->notebook), tab_ptr->paned, tab_box);
    gtk_notebook_set_tab_reorderable(GTK_NOTEBOOK(state->notebook), tab_ptr->paned, TRUE);
    
    state->tabs.push_back(std::move(tab));
    
    auto close_cb = +[](GtkWidget*, gpointer user_data) {
        auto* data = static_cast<std::pair<FileManagerState*, TabState*>*>(user_data);
        close_tab(data->first, data->second);
    };
    g_signal_connect_data(close_btn, "clicked", G_CALLBACK(close_cb), new std::pair<FileManagerState*, TabState*>(state, tab_ptr), [](gpointer data, GClosure*) {
        delete static_cast<std::pair<FileManagerState*, TabState*>*>(data);
    }, static_cast<GConnectFlags>(0));

    if (state->tabs.size() == 1) {
        state->active_tab = tab_ptr;
        navigate_to(state, initial_path, false);
    } else {
        gtk_notebook_set_current_page(GTK_NOTEBOOK(state->notebook), index);
        state->active_tab = tab_ptr;
        navigate_to(state, initial_path, false);
    }
}

static void open_settings_dialog(FileManagerState* state) {
    if (!state || !state->window) return;

    GtkWidget* dialog = gtk_dialog_new_with_buttons(
        "Zenith Files — Preferences & Shortcuts",
        GTK_WINDOW(state->window),
        static_cast<GtkDialogFlags>(GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT),
        "_Close", GTK_RESPONSE_CLOSE,
        nullptr
    );
    gtk_window_set_default_size(GTK_WINDOW(dialog), 620, 520);
    gtk_widget_add_css_class(dialog, "zenith-files-dialog");

    GtkWidget* content = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
    gtk_container_set_border_width(GTK_CONTAINER(content), 16);
    gtk_box_set_spacing(GTK_BOX(content), 12);

    GtkWidget* notebook = gtk_notebook_new();
    gtk_widget_add_css_class(notebook, "files-settings-notebook");

    // ── Tab 1: Preferences & View Sizing ──────────────────────────────────────
    GtkWidget* tab1_vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 16);
    gtk_container_set_border_width(GTK_CONTAINER(tab1_vbox), 16);

    // Section 1: Icon / Folder Size
    GtkWidget* size_header = gtk_label_new("<b>Folder & Icon Size</b>");
    gtk_label_set_use_markup(GTK_LABEL(size_header), TRUE);
    gtk_label_set_xalign(GTK_LABEL(size_header), 0.0f);
    gtk_widget_add_css_class(size_header, "files-prop-title");
    gtk_box_pack_start(GTK_BOX(tab1_vbox), size_header, FALSE, FALSE, 0);

    GtkWidget* size_desc = gtk_label_new("Adjust the size of folders and file icons in Grid View (also Ctrl+Scroll or Ctrl++/Ctrl+-):");
    gtk_label_set_xalign(GTK_LABEL(size_desc), 0.0f);
    gtk_label_set_line_wrap(GTK_LABEL(size_desc), TRUE);
    gtk_widget_add_css_class(size_desc, "files-prop-subtitle");
    gtk_box_pack_start(GTK_BOX(tab1_vbox), size_desc, FALSE, FALSE, 0);

    // Get current size from active tab
    int cur_size = 96;
    if (state->active_tab && state->active_tab->active_pane) {
        cur_size = FileViewWidget::get_icon_size(state->active_tab->active_pane->file_view);
    }

    GtkWidget* scale = gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL, 48, 192, 16);
    gtk_scale_set_value_pos(GTK_SCALE(scale), GTK_POS_RIGHT);
    gtk_scale_set_digits(GTK_SCALE(scale), 0);
    gtk_range_set_value(GTK_RANGE(scale), cur_size);
    gtk_scale_add_mark(GTK_SCALE(scale), 48, GTK_POS_BOTTOM, "Small (48px)");
    gtk_scale_add_mark(GTK_SCALE(scale), 64, GTK_POS_BOTTOM, "Medium (64px)");
    gtk_scale_add_mark(GTK_SCALE(scale), 96, GTK_POS_BOTTOM, "Normal (96px)");
    gtk_scale_add_mark(GTK_SCALE(scale), 128, GTK_POS_BOTTOM, "Large (128px)");
    gtk_scale_add_mark(GTK_SCALE(scale), 160, GTK_POS_BOTTOM, "Huge (160px)");

    g_signal_connect(scale, "value-changed", G_CALLBACK(+[](GtkRange* r, gpointer user_data) {
        auto* s = static_cast<FileManagerState*>(user_data);
        int sz = static_cast<int>(gtk_range_get_value(r));
        for (auto& tab : s->tabs) {
            FileViewWidget::set_icon_size(tab->left_pane.file_view, sz);
            if (tab->is_dual) FileViewWidget::set_icon_size(tab->right_pane.file_view, sz);
        }
    }), state);

    gtk_box_pack_start(GTK_BOX(tab1_vbox), scale, FALSE, FALSE, 8);

    // Zoom buttons row
    GtkWidget* zoom_btn_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);

    GtkWidget* btn_smaller = gtk_button_new_with_label("➖ Smaller (Ctrl+-)");
    gtk_widget_add_css_class(btn_smaller, "files-btn-tool");
    g_signal_connect_swapped(btn_smaller, "clicked", G_CALLBACK(+[](FileManagerState* s) {
        for (auto& tab : s->tabs) {
            FileViewWidget::zoom_out(tab->left_pane.file_view);
            if (tab->is_dual) FileViewWidget::zoom_out(tab->right_pane.file_view);
        }
    }), state);
    gtk_box_pack_start(GTK_BOX(zoom_btn_box), btn_smaller, FALSE, FALSE, 0);

    GtkWidget* btn_reset = gtk_button_new_with_label("↺ Reset (Ctrl+0)");
    gtk_widget_add_css_class(btn_reset, "files-btn-tool");
    g_signal_connect_swapped(btn_reset, "clicked", G_CALLBACK(+[](FileManagerState* s) {
        for (auto& tab : s->tabs) {
            FileViewWidget::zoom_reset(tab->left_pane.file_view);
            if (tab->is_dual) FileViewWidget::zoom_reset(tab->right_pane.file_view);
        }
    }), state);
    gtk_box_pack_start(GTK_BOX(zoom_btn_box), btn_reset, FALSE, FALSE, 0);

    GtkWidget* btn_larger = gtk_button_new_with_label("➕ Larger (Ctrl++)");
    gtk_widget_add_css_class(btn_larger, "files-btn-tool");
    g_signal_connect_swapped(btn_larger, "clicked", G_CALLBACK(+[](FileManagerState* s) {
        for (auto& tab : s->tabs) {
            FileViewWidget::zoom_in(tab->left_pane.file_view);
            if (tab->is_dual) FileViewWidget::zoom_in(tab->right_pane.file_view);
        }
    }), state);
    gtk_box_pack_start(GTK_BOX(zoom_btn_box), btn_larger, FALSE, FALSE, 0);

    gtk_box_pack_start(GTK_BOX(tab1_vbox), zoom_btn_box, FALSE, FALSE, 0);

    gtk_box_pack_start(GTK_BOX(tab1_vbox), gtk_separator_new(GTK_ORIENTATION_HORIZONTAL), FALSE, FALSE, 8);

    // Section 2: General Options
    GtkWidget* gen_header = gtk_label_new("<b>General Options</b>");
    gtk_label_set_use_markup(GTK_LABEL(gen_header), TRUE);
    gtk_label_set_xalign(GTK_LABEL(gen_header), 0.0f);
    gtk_widget_add_css_class(gen_header, "files-prop-title");
    gtk_box_pack_start(GTK_BOX(tab1_vbox), gen_header, FALSE, FALSE, 0);

    GtkWidget* chk_hidden = gtk_check_button_new_with_label("Show hidden files and folders by default (Ctrl+H)");
    if (state->active_tab && state->active_tab->active_pane) {
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(chk_hidden),
            FileViewWidget::get_show_hidden(state->active_tab->active_pane->file_view));
    }
    g_signal_connect(chk_hidden, "toggled", G_CALLBACK(+[](GtkToggleButton* btn, gpointer user_data) {
        auto* s = static_cast<FileManagerState*>(user_data);
        gboolean act = gtk_toggle_button_get_active(btn);
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(s->btn_hidden), act);
        for (auto& tab : s->tabs) {
            FileViewWidget::set_show_hidden(tab->left_pane.file_view, act);
            if (tab->is_dual) FileViewWidget::set_show_hidden(tab->right_pane.file_view, act);
        }
    }), state);
    gtk_box_pack_start(GTK_BOX(tab1_vbox), chk_hidden, FALSE, FALSE, 0);

    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), tab1_vbox, gtk_label_new("⚙ Preferences"));

    // ── Tab 2: Keyboard Shortcuts Reference ──────────────────────────────────
    GtkWidget* scroll = gtk_scrolled_window_new(nullptr, nullptr);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);

    GtkWidget* tab2_vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
    gtk_container_set_border_width(GTK_CONTAINER(tab2_vbox), 16);

    auto add_shortcut_section = [](GtkWidget* vbox, const char* section_title, const std::vector<std::pair<std::string, std::string>>& items) {
        GtkWidget* hdr = gtk_label_new(section_title);
        gtk_label_set_use_markup(GTK_LABEL(hdr), TRUE);
        gtk_label_set_xalign(GTK_LABEL(hdr), 0.0f);
        gtk_widget_add_css_class(hdr, "files-prop-title");
        gtk_box_pack_start(GTK_BOX(vbox), hdr, FALSE, FALSE, 4);

        GtkWidget* grid = gtk_grid_new();
        gtk_grid_set_row_spacing(GTK_GRID(grid), 6);
        gtk_grid_set_column_spacing(GTK_GRID(grid), 16);

        int row = 0;
        for (const auto& item : items) {
            GtkWidget* key_lbl = gtk_label_new(item.first.c_str());
            gtk_label_set_use_markup(GTK_LABEL(key_lbl), TRUE);
            gtk_label_set_xalign(GTK_LABEL(key_lbl), 0.0f);
            gtk_widget_add_css_class(key_lbl, "files-prop-key");
            gtk_grid_attach(GTK_GRID(grid), key_lbl, 0, row, 1, 1);

            GtkWidget* desc_lbl = gtk_label_new(item.second.c_str());
            gtk_label_set_xalign(GTK_LABEL(desc_lbl), 0.0f);
            gtk_widget_add_css_class(desc_lbl, "files-prop-val");
            gtk_grid_attach(GTK_GRID(grid), desc_lbl, 1, row, 1, 1);

            row++;
        }
        gtk_box_pack_start(GTK_BOX(vbox), grid, FALSE, FALSE, 0);
        gtk_box_pack_start(GTK_BOX(vbox), gtk_separator_new(GTK_ORIENTATION_HORIZONTAL), FALSE, FALSE, 6);
    };

    add_shortcut_section(tab2_vbox, "<b>🧭 Navigation & Tabs</b>", {
        {"<tt>Alt + Left</tt>", "Go back in history"},
        {"<tt>Alt + Right</tt>", "Go forward in history"},
        {"<tt>Alt + Up / Backspace</tt>", "Go to parent directory"},
        {"<tt>Alt + Home</tt>", "Go to Home folder"},
        {"<tt>Ctrl + L</tt>", "Enter editable path bar directly"},
        {"<tt>Ctrl + F</tt>", "Open recursive background search"},
        {"<tt>Ctrl + T</tt>", "Open a new tab"},
        {"<tt>Ctrl + W</tt>", "Close active tab"},
        {"<tt>F3</tt>", "Toggle split view (Dual Pane)"},
        {"<tt>F4</tt>", "Open terminal in current directory"}
    });

    add_shortcut_section(tab2_vbox, "<b>📁 File Operations</b>", {
        {"<tt>Enter / Double Click</tt>", "Open selected file or enter folder"},
        {"<tt>F2</tt>", "Rename single item or Bulk Rename multiple items"},
        {"<tt>Ctrl + C</tt>", "Copy selected items to clipboard"},
        {"<tt>Ctrl + X</tt>", "Cut selected items to clipboard"},
        {"<tt>Ctrl + V</tt>", "Paste items from clipboard (with progress)"},
        {"<tt>Ctrl + Shift + C</tt>", "Copy full path(s) to clipboard"},
        {"<tt>Ctrl + Shift + N</tt>", "Create a new folder"},
        {"<tt>Delete</tt>", "Move selected items to Trash"},
        {"<tt>Shift + Delete</tt>", "Permanently delete (with confirmation)"},
        {"<tt>Ctrl + A</tt>", "Select all items in current folder"},
        {"<tt>F5 / Ctrl + R</tt>", "Refresh folder contents"}
    });

    add_shortcut_section(tab2_vbox, "<b>🔍 Views & Zoom</b>", {
        {"<tt>Ctrl + 1</tt>", "Switch to Grid (Icon) View"},
        {"<tt>Ctrl + 2</tt>", "Switch to List (Detail) View"},
        {"<tt>Ctrl + Plus (+)</tt>", "Zoom in (increase icon/folder size)"},
        {"<tt>Ctrl + Minus (-)</tt>", "Zoom out (decrease icon/folder size)"},
        {"<tt>Ctrl + 0</tt>", "Reset zoom to normal size (96px)"},
        {"<tt>Ctrl + ScrollWheel</tt>", "Smoothly zoom icon size in view"},
        {"<tt>Ctrl + H</tt>", "Toggle hidden files & folders"},
        {"<tt>Ctrl + ,</tt>", "Open Preferences & Shortcuts"}
    });

    gtk_container_add(GTK_CONTAINER(scroll), tab2_vbox);
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), scroll, gtk_label_new("⌨ Shortcuts"));

    gtk_box_pack_start(GTK_BOX(content), notebook, TRUE, TRUE, 0);

    gtk_widget_show_all(dialog);
    gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);
}

static gboolean on_window_key_press(GtkWidget*, GdkEventKey* event, gpointer user_data) {
    auto* state = static_cast<FileManagerState*>(user_data);
    if (!state) return FALSE;

    guint key = event->keyval;
    guint state_mask = event->state & gtk_accelerator_get_default_mod_mask();

    if (state_mask == GDK_MOD1_MASK) { // Alt key
        if (key == GDK_KEY_Left) {
            go_back(state);
            return TRUE;
        } else if (key == GDK_KEY_Right) {
            go_forward(state);
            return TRUE;
        } else if (key == GDK_KEY_Up) {
            go_up(state);
            return TRUE;
        } else if (key == GDK_KEY_Home) {
            go_home(state);
            return TRUE;
        }
    } else if (state_mask == GDK_CONTROL_MASK) {
        if (key == GDK_KEY_l || key == GDK_KEY_L) {
            PathBarWidget::enter_edit_mode(state->path_bar);
            return TRUE;
        } else if (key == GDK_KEY_f || key == GDK_KEY_F) {
            gboolean active = gtk_revealer_get_reveal_child(GTK_REVEALER(state->search_revealer));
            gtk_revealer_set_reveal_child(GTK_REVEALER(state->search_revealer), !active);
            if (!active) {
                gtk_widget_grab_focus(state->search_entry);
            } else if (state->active_tab) {
                FileViewWidget::set_search_query(state->active_tab->active_pane->file_view, "");
            }
            return TRUE;
        } else if (key == GDK_KEY_t || key == GDK_KEY_T) {
            if (state->active_tab) {
                create_new_tab(state, state->active_tab->active_pane->current_path);
            } else {
                create_new_tab(state, g_get_home_dir() ? g_get_home_dir() : "/");
            }
            return TRUE;
        } else if (key == GDK_KEY_w || key == GDK_KEY_W) {
            if (state->active_tab) close_tab(state, state->active_tab);
            return TRUE;
        } else if (key == GDK_KEY_1) {
            if (state->active_tab) FileViewWidget::set_view_mode(state->active_tab->active_pane->file_view, ViewMode::GRID);
            gtk_widget_add_css_class(state->btn_grid_view, "files-btn-view-active");
            gtk_widget_remove_css_class(state->btn_list_view, "files-btn-view-active");
            return TRUE;
        } else if (key == GDK_KEY_2) {
            if (state->active_tab) FileViewWidget::set_view_mode(state->active_tab->active_pane->file_view, ViewMode::LIST);
            gtk_widget_add_css_class(state->btn_list_view, "files-btn-view-active");
            gtk_widget_remove_css_class(state->btn_grid_view, "files-btn-view-active");
            return TRUE;
        } else if (key == GDK_KEY_plus || key == GDK_KEY_equal || key == GDK_KEY_KP_Add) {
            for (auto& tab : state->tabs) {
                FileViewWidget::zoom_in(tab->left_pane.file_view);
                if (tab->is_dual) FileViewWidget::zoom_in(tab->right_pane.file_view);
            }
            return TRUE;
        } else if (key == GDK_KEY_minus || key == GDK_KEY_KP_Subtract) {
            for (auto& tab : state->tabs) {
                FileViewWidget::zoom_out(tab->left_pane.file_view);
                if (tab->is_dual) FileViewWidget::zoom_out(tab->right_pane.file_view);
            }
            return TRUE;
        } else if (key == GDK_KEY_0 || key == GDK_KEY_KP_0) {
            for (auto& tab : state->tabs) {
                FileViewWidget::zoom_reset(tab->left_pane.file_view);
                if (tab->is_dual) FileViewWidget::zoom_reset(tab->right_pane.file_view);
            }
            return TRUE;
        } else if (key == GDK_KEY_comma) {
            open_settings_dialog(state);
            return TRUE;
        }
    } else if (state_mask == 0) {
        if (key == GDK_KEY_Escape) {
            if (gtk_revealer_get_reveal_child(GTK_REVEALER(state->search_revealer))) {
                gtk_entry_set_text(GTK_ENTRY(state->search_entry), "");
                gtk_revealer_set_reveal_child(GTK_REVEALER(state->search_revealer), FALSE);
                if (state->active_tab) {
                    FileViewWidget::set_search_query(state->active_tab->active_pane->file_view, "");
                    gtk_widget_grab_focus(state->active_tab->active_pane->file_view);
                }
                return TRUE;
            }
        } else if (key == GDK_KEY_F3) {
            toggle_dual_pane(state);
            return TRUE;
        } else if (key == GDK_KEY_F4) {
            if (state->active_tab) FileOperations::open_terminal(state->active_tab->active_pane->current_path);
            return TRUE;
        }
    }

    return FALSE;
}

GtkWidget* FileManagerWindow::create(const std::string& initial_path) {
    auto* state = new FileManagerState();

    state->window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_default_size(GTK_WINDOW(state->window), 980, 640);
    gtk_window_set_icon_name(GTK_WINDOW(state->window), "system-file-manager");
    gtk_widget_add_css_class(state->window, "zenith-files-window");

    GtkWidget* main_vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_container_add(GTK_CONTAINER(state->window), main_vbox);

    // 1. Toolbar / HeaderBar
    GtkWidget* toolbar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_widget_add_css_class(toolbar, "files-toolbar");
    gtk_container_set_border_width(GTK_CONTAINER(toolbar), 8);
    gtk_box_pack_start(GTK_BOX(main_vbox), toolbar, FALSE, FALSE, 0);

    // Nav buttons: Back, Forward, Up, Home
    GtkWidget* nav_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 2);
    gtk_widget_add_css_class(nav_box, "files-nav-group");

    state->btn_back = gtk_button_new_from_icon_name("go-previous-symbolic", GTK_ICON_SIZE_BUTTON);
    gtk_widget_set_tooltip_text(state->btn_back, "Back (Alt+Left)");
    gtk_widget_add_css_class(state->btn_back, "files-btn-nav");
    g_signal_connect_swapped(state->btn_back, "clicked", G_CALLBACK(go_back), state);
    gtk_box_pack_start(GTK_BOX(nav_box), state->btn_back, FALSE, FALSE, 0);

    state->btn_forward = gtk_button_new_from_icon_name("go-next-symbolic", GTK_ICON_SIZE_BUTTON);
    gtk_widget_set_tooltip_text(state->btn_forward, "Forward (Alt+Right)");
    gtk_widget_add_css_class(state->btn_forward, "files-btn-nav");
    g_signal_connect_swapped(state->btn_forward, "clicked", G_CALLBACK(go_forward), state);
    gtk_box_pack_start(GTK_BOX(nav_box), state->btn_forward, FALSE, FALSE, 0);

    state->btn_up = gtk_button_new_from_icon_name("go-up-symbolic", GTK_ICON_SIZE_BUTTON);
    gtk_widget_set_tooltip_text(state->btn_up, "Parent Directory (Alt+Up)");
    gtk_widget_add_css_class(state->btn_up, "files-btn-nav");
    g_signal_connect_swapped(state->btn_up, "clicked", G_CALLBACK(go_up), state);
    gtk_box_pack_start(GTK_BOX(nav_box), state->btn_up, FALSE, FALSE, 0);

    state->btn_home = gtk_button_new_from_icon_name("user-home-symbolic", GTK_ICON_SIZE_BUTTON);
    gtk_widget_set_tooltip_text(state->btn_home, "Home Folder (Alt+Home)");
    gtk_widget_add_css_class(state->btn_home, "files-btn-nav");
    g_signal_connect_swapped(state->btn_home, "clicked", G_CALLBACK(go_home), state);
    gtk_box_pack_start(GTK_BOX(nav_box), state->btn_home, FALSE, FALSE, 0);

    gtk_box_pack_start(GTK_BOX(toolbar), nav_box, FALSE, FALSE, 0);

    // PathBar (Breadcrumbs)
    state->path_bar = PathBarWidget::create([state](const std::string& target) {
        navigate_to(state, target, true);
    });
    gtk_box_pack_start(GTK_BOX(toolbar), state->path_bar, TRUE, TRUE, 0);

    // Actions Box: New Tab, Search, New Folder, Terminal, Hidden Toggle, View Switcher
    GtkWidget* act_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
    gtk_widget_add_css_class(act_box, "files-act-group");

    state->btn_new_tab = gtk_button_new_from_icon_name("tab-new-symbolic", GTK_ICON_SIZE_BUTTON);
    gtk_widget_set_tooltip_text(state->btn_new_tab, "New Tab (Ctrl+T)");
    gtk_widget_add_css_class(state->btn_new_tab, "files-btn-tool");
    g_signal_connect_swapped(state->btn_new_tab, "clicked", G_CALLBACK(+[](FileManagerState* s) {
        create_new_tab(s, s->active_tab ? s->active_tab->active_pane->current_path : (g_get_home_dir() ? g_get_home_dir() : "/"));
    }), state);
    gtk_box_pack_start(GTK_BOX(act_box), state->btn_new_tab, FALSE, FALSE, 0);

    state->btn_dual_pane = gtk_button_new_from_icon_name("view-restore-symbolic", GTK_ICON_SIZE_BUTTON);
    gtk_widget_set_tooltip_text(state->btn_dual_pane, "Split View (F3)");
    gtk_widget_add_css_class(state->btn_dual_pane, "files-btn-tool");
    g_signal_connect_swapped(state->btn_dual_pane, "clicked", G_CALLBACK(toggle_dual_pane), state);
    gtk_box_pack_start(GTK_BOX(act_box), state->btn_dual_pane, FALSE, FALSE, 0);

    state->btn_search = gtk_button_new_from_icon_name("edit-find-symbolic", GTK_ICON_SIZE_BUTTON);
    gtk_widget_set_tooltip_text(state->btn_search, "Search Files (Ctrl+F)");
    gtk_widget_add_css_class(state->btn_search, "files-btn-tool");
    g_signal_connect_swapped(state->btn_search, "clicked", G_CALLBACK(+[](FileManagerState* s) {
        gboolean active = gtk_revealer_get_reveal_child(GTK_REVEALER(s->search_revealer));
        gtk_revealer_set_reveal_child(GTK_REVEALER(s->search_revealer), !active);
        if (!active) {
            gtk_widget_grab_focus(s->search_entry);
        } else {
            if (s->active_tab) FileViewWidget::set_search_query(s->active_tab->active_pane->file_view, "");
        }
    }), state);
    gtk_box_pack_start(GTK_BOX(act_box), state->btn_search, FALSE, FALSE, 0);

    state->btn_new_folder = gtk_button_new_from_icon_name("folder-new-symbolic", GTK_ICON_SIZE_BUTTON);
    gtk_widget_set_tooltip_text(state->btn_new_folder, "New Folder (Ctrl+Shift+N)");
    gtk_widget_add_css_class(state->btn_new_folder, "files-btn-tool");
    g_signal_connect_swapped(state->btn_new_folder, "clicked", G_CALLBACK(+[](FileManagerState* s) {
        if (s->active_tab) FileViewWidget::action_new_folder(s->active_tab->active_pane->file_view);
    }), state);
    gtk_box_pack_start(GTK_BOX(act_box), state->btn_new_folder, FALSE, FALSE, 0);

    state->btn_term = gtk_button_new_from_icon_name("utilities-terminal-symbolic", GTK_ICON_SIZE_BUTTON);
    gtk_widget_set_tooltip_text(state->btn_term, "Open Terminal (F4)");
    gtk_widget_add_css_class(state->btn_term, "files-btn-tool");
    g_signal_connect_swapped(state->btn_term, "clicked", G_CALLBACK(+[](FileManagerState* s) {
        if (s->active_tab) FileOperations::open_terminal(s->active_tab->active_pane->current_path);
    }), state);
    gtk_box_pack_start(GTK_BOX(act_box), state->btn_term, FALSE, FALSE, 0);

    state->btn_hidden = gtk_toggle_button_new();
    gtk_button_set_image(GTK_BUTTON(state->btn_hidden), gtk_image_new_from_icon_name("view-conceal-symbolic", GTK_ICON_SIZE_BUTTON));
    gtk_widget_set_tooltip_text(state->btn_hidden, "Toggle Hidden Files (Ctrl+H)");
    gtk_widget_add_css_class(state->btn_hidden, "files-btn-tool");
    g_signal_connect_swapped(state->btn_hidden, "toggled", G_CALLBACK(+[](FileManagerState* s) {
        gboolean act = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(s->btn_hidden));
        for (auto& tab : s->tabs) { FileViewWidget::set_show_hidden(tab->left_pane.file_view, act); if (tab->is_dual) FileViewWidget::set_show_hidden(tab->right_pane.file_view, act); }
    }), state);
    gtk_box_pack_start(GTK_BOX(act_box), state->btn_hidden, FALSE, FALSE, 0);

    // View Switcher (Grid / List)
    GtkWidget* view_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_widget_add_css_class(view_box, "files-view-switcher");

    state->btn_grid_view = gtk_button_new_from_icon_name("view-grid-symbolic", GTK_ICON_SIZE_BUTTON);
    gtk_widget_set_tooltip_text(state->btn_grid_view, "Grid View (Ctrl+1)");
    gtk_widget_add_css_class(state->btn_grid_view, "files-btn-view-active");
    g_signal_connect_swapped(state->btn_grid_view, "clicked", G_CALLBACK(+[](FileManagerState* s) {
        for (auto& tab : s->tabs) { FileViewWidget::set_view_mode(tab->left_pane.file_view, ViewMode::GRID); if (tab->is_dual) FileViewWidget::set_view_mode(tab->right_pane.file_view, ViewMode::GRID); }
        gtk_widget_add_css_class(s->btn_grid_view, "files-btn-view-active");
        gtk_widget_remove_css_class(s->btn_list_view, "files-btn-view-active");
    }), state);
    gtk_box_pack_start(GTK_BOX(view_box), state->btn_grid_view, FALSE, FALSE, 0);

    state->btn_list_view = gtk_button_new_from_icon_name("view-list-symbolic", GTK_ICON_SIZE_BUTTON);
    gtk_widget_set_tooltip_text(state->btn_list_view, "List View (Ctrl+2)");
    g_signal_connect_swapped(state->btn_list_view, "clicked", G_CALLBACK(+[](FileManagerState* s) {
        for (auto& tab : s->tabs) { FileViewWidget::set_view_mode(tab->left_pane.file_view, ViewMode::LIST); if (tab->is_dual) FileViewWidget::set_view_mode(tab->right_pane.file_view, ViewMode::LIST); }
        gtk_widget_add_css_class(s->btn_list_view, "files-btn-view-active");
        gtk_widget_remove_css_class(s->btn_grid_view, "files-btn-view-active");
    }), state);
    gtk_box_pack_start(GTK_BOX(act_box), view_box, FALSE, FALSE, 0);

    state->btn_settings = gtk_button_new_from_icon_name("emblem-system-symbolic", GTK_ICON_SIZE_BUTTON);
    gtk_widget_set_tooltip_text(state->btn_settings, "Preferences & Shortcuts (Ctrl+,)");
    gtk_widget_add_css_class(state->btn_settings, "files-btn-tool");
    g_signal_connect_swapped(state->btn_settings, "clicked", G_CALLBACK(open_settings_dialog), state);
    gtk_box_pack_start(GTK_BOX(act_box), state->btn_settings, FALSE, FALSE, 0);

    gtk_box_pack_start(GTK_BOX(toolbar), act_box, FALSE, FALSE, 0);

    // 2. Filter / Search Bar (Revealer)
    state->search_revealer = gtk_revealer_new();
    gtk_revealer_set_transition_type(GTK_REVEALER(state->search_revealer), GTK_REVEALER_TRANSITION_TYPE_SLIDE_DOWN);

    GtkWidget* search_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_widget_add_css_class(search_box, "files-search-bar");
    gtk_container_set_border_width(GTK_CONTAINER(search_box), 6);

    state->search_entry = gtk_search_entry_new();
    gtk_widget_add_css_class(state->search_entry, "files-search-entry");
    gtk_entry_set_placeholder_text(GTK_ENTRY(state->search_entry), "Search in current folder...");
    g_signal_connect_swapped(state->search_entry, "search-changed", G_CALLBACK(+[](FileManagerState* s) {
        const char* q = gtk_entry_get_text(GTK_ENTRY(s->search_entry));
        if (s->active_tab) FileViewWidget::set_search_query(s->active_tab->active_pane->file_view, q ? q : "");
    }), state);
    gtk_box_pack_start(GTK_BOX(search_box), state->search_entry, TRUE, TRUE, 0);

    GtkWidget* btn_close_search = gtk_button_new_from_icon_name("window-close-symbolic", GTK_ICON_SIZE_BUTTON);
    gtk_widget_add_css_class(btn_close_search, "files-btn-close-search");
    g_signal_connect_swapped(btn_close_search, "clicked", G_CALLBACK(+[](FileManagerState* s) {
        gtk_entry_set_text(GTK_ENTRY(s->search_entry), "");
        gtk_revealer_set_reveal_child(GTK_REVEALER(s->search_revealer), FALSE);
        if (s->active_tab) FileViewWidget::set_search_query(s->active_tab->active_pane->file_view, "");
    }), state);
    gtk_box_pack_start(GTK_BOX(search_box), btn_close_search, FALSE, FALSE, 0);

    gtk_container_add(GTK_CONTAINER(state->search_revealer), search_box);
    gtk_box_pack_start(GTK_BOX(main_vbox), state->search_revealer, FALSE, FALSE, 0);

    // 3. Middle Paned: Sidebar + FileView
    state->paned = gtk_paned_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_paned_set_position(GTK_PANED(state->paned), 210);
    gtk_box_pack_start(GTK_BOX(main_vbox), state->paned, TRUE, TRUE, 0);

    state->sidebar = PlacesSidebar::create([state](const std::string& target) {
        navigate_to(state, target, true);
    });
    gtk_paned_pack1(GTK_PANED(state->paned), state->sidebar, FALSE, FALSE);

    state->notebook = gtk_notebook_new();
    gtk_notebook_set_scrollable(GTK_NOTEBOOK(state->notebook), TRUE);
    gtk_notebook_set_show_border(GTK_NOTEBOOK(state->notebook), FALSE);
    gtk_widget_add_css_class(state->notebook, "files-notebook");
    
    auto switch_page_cb = +[](GtkNotebook* notebook, GtkWidget* page, guint page_num, gpointer user_data) {
        auto* s = static_cast<FileManagerState*>(user_data);
        GtkWidget* child = gtk_notebook_get_nth_page(notebook, page_num);
        for (auto& tab : s->tabs) {
            if (tab->paned == child) {
                s->active_tab = tab.get();
                PathBarWidget::set_path(s->path_bar, tab->active_pane->current_path);
                if (tab->active_pane == &tab->left_pane) PlacesSidebar::set_active_path(s->sidebar, tab->active_pane->current_path);
                update_nav_buttons(s);
                update_status_text(s);
                update_disk_space(s, tab->active_pane->current_path);
                
                std::string title = tab->active_pane->current_path;
                const char* home = g_get_home_dir();
                if (home && title.rfind(home, 0) == 0) title = "~" + title.substr(strlen(home));
                gtk_window_set_title(GTK_WINDOW(s->window), (title + " — Zenith Files").c_str());
                break;
            }
        }
    };
    g_signal_connect(state->notebook, "switch-page", G_CALLBACK(switch_page_cb), state);

    gtk_paned_pack2(GTK_PANED(state->paned), state->notebook, TRUE, FALSE);

    // 4. Status Bar
    GtkWidget* statusbar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
    gtk_widget_add_css_class(statusbar, "files-statusbar");
    gtk_container_set_border_width(GTK_CONTAINER(statusbar), 6);

    state->status_label = gtk_label_new("0 items");
    gtk_label_set_xalign(GTK_LABEL(state->status_label), 0.0f);
    gtk_widget_add_css_class(state->status_label, "files-status-text");
    gtk_box_pack_start(GTK_BOX(statusbar), state->status_label, FALSE, FALSE, 0);

    state->disk_label = gtk_label_new("");
    gtk_label_set_xalign(GTK_LABEL(state->disk_label), 1.0f);
    gtk_widget_add_css_class(state->disk_label, "files-disk-text");
    gtk_box_pack_end(GTK_BOX(statusbar), state->disk_label, FALSE, FALSE, 0);

    gtk_box_pack_end(GTK_BOX(main_vbox), statusbar, FALSE, FALSE, 0);

    // Connect window key press
    g_signal_connect(state->window, "key-press-event", G_CALLBACK(on_window_key_press), state);

    // Save state pointer on window
    g_object_set_data(G_OBJECT(state->window), "file_manager_state", state);

    // Clean up state on window destroy
    g_signal_connect(state->window, "destroy", G_CALLBACK(+[](GtkWidget*, gpointer u) {
        delete static_cast<FileManagerState*>(u);
    }), state);

    // Determine target initial path
    std::string start_path = initial_path;
    if (start_path.empty()) {
        const char* h = g_get_home_dir();
        start_path = h ? h : "/";
    }

    create_new_tab(state, start_path);

    gtk_widget_show_all(state->window);
    return state->window;
}

void FileManagerWindow::open_path(GtkWidget* window, const std::string& path) {
    if (!window) return;
    auto* state = static_cast<FileManagerState*>(g_object_get_data(G_OBJECT(window), "file_manager_state"));
    if (state) {
        navigate_to(state, path, true);
        gtk_window_present(GTK_WINDOW(window));
    }
}

} // namespace zenith
