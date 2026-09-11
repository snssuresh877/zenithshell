#include "file_manager_window.hpp"
#include "path_bar_widget.hpp"
#include "places_sidebar.hpp"
#include "file_view_widget.hpp"
#include "file_item.hpp"
#include "file_operations.hpp"
#include "command_palette.hpp"
#include "inspector_panel.hpp"
#include "file_shortcuts.hpp"
#include "file_preferences_dialog.hpp"
#include "quick_preview.hpp"
#include "theme/theme_engine.hpp"
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
    PaneState left_pane;
    PaneState right_pane;
    PaneState* active_pane{&left_pane};
    bool is_dual{false};
    GtkWidget* tab_box{nullptr};
    GtkWidget* tab_label{nullptr};
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
    GtkWidget* btn_inspector{nullptr};
    GtkWidget* btn_palette{nullptr};

    GtkWidget* main_paned{nullptr};
    GtkWidget* sidebar{nullptr};
    GtkWidget* center_paned{nullptr};
    GtkWidget* inspector_panel{nullptr};
    bool inspector_visible{false};
    
    GtkWidget* notebook{nullptr};
    std::vector<std::unique_ptr<TabState>> tabs;
    TabState* active_tab{nullptr};

    GtkWidget* status_label{nullptr};
    GtkWidget* disk_label{nullptr};
};

// Forward declarations
static void navigate_to(FileManagerState* state, const std::string& path, bool record_history);
static void go_back(FileManagerState* state);
static void go_forward(FileManagerState* state);
static void go_up(FileManagerState* state);
static void go_home(FileManagerState* state);
static void close_tab(FileManagerState* state, TabState* tab);
static void create_new_tab(FileManagerState* state, const std::string& initial_path);
static void toggle_dual_pane(FileManagerState* state);
static void toggle_inspector(FileManagerState* state);
static void execute_action(FileManagerState* state, const std::string& action_id);

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

static void toggle_inspector(FileManagerState* state) {
    if (!state || !state->inspector_panel || !state->center_paned) return;

    state->inspector_visible = !state->inspector_visible;
    if (state->inspector_visible) {
        gtk_widget_show_all(state->inspector_panel);
        gint width = gtk_widget_get_allocated_width(state->window);
        if (width > 500) {
            gtk_paned_set_position(GTK_PANED(state->center_paned), width - 300);
        }
        gtk_widget_add_css_class(state->btn_inspector, "files-btn-view-active");

        if (state->active_tab && state->active_tab->active_pane) {
            auto sel = FileViewWidget::get_selected_paths(state->active_tab->active_pane->file_view);
            InspectorPanel::set_current_directory(state->inspector_panel, state->active_tab->active_pane->current_path);
            InspectorPanel::update_selection(state->inspector_panel, sel);
        }
    } else {
        gtk_widget_hide(state->inspector_panel);
        gtk_widget_remove_css_class(state->btn_inspector, "files-btn-view-active");
    }
}

static void navigate_to(FileManagerState* state, const std::string& path, bool record_history) {
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

    if (state->inspector_visible && state->inspector_panel) {
        InspectorPanel::set_current_directory(state->inspector_panel, path);
        InspectorPanel::update_selection(state->inspector_panel, {});
    }
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
        },
        [state, tab_ptr, pane](const std::vector<std::string>& sel_paths) {
            if (state->active_tab == tab_ptr && tab_ptr->active_pane == pane) {
                if (state->inspector_visible && state->inspector_panel) {
                    InspectorPanel::update_selection(state->inspector_panel, sel_paths);
                }
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
            if (d->first->inspector_visible && d->first->inspector_panel) {
                auto sel = FileViewWidget::get_selected_paths(d->second.second->file_view);
                InspectorPanel::set_current_directory(d->first->inspector_panel, d->second.second->current_path);
                InspectorPanel::update_selection(d->first->inspector_panel, sel);
            }
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

static void execute_action(FileManagerState* state, const std::string& action_id) {
    if (!state) return;

    if (action_id == "nav_back") {
        go_back(state);
    } else if (action_id == "nav_forward") {
        go_forward(state);
    } else if (action_id == "nav_up") {
        go_up(state);
    } else if (action_id == "nav_home") {
        go_home(state);
    } else if (action_id == "edit_path") {
        PathBarWidget::enter_edit_mode(state->path_bar);
    } else if (action_id == "new_tab") {
        create_new_tab(state, state->active_tab ? state->active_tab->active_pane->current_path : (g_get_home_dir() ? g_get_home_dir() : "/"));
    } else if (action_id == "close_tab") {
        if (state->active_tab) close_tab(state, state->active_tab);
    } else if (action_id == "split_view") {
        toggle_dual_pane(state);
    } else if (action_id == "open_terminal") {
        if (state->active_tab) FileOperations::open_terminal(state->active_tab->active_pane->current_path);
    } else if (action_id == "new_folder") {
        if (state->active_tab) FileViewWidget::action_new_folder(state->active_tab->active_pane->file_view);
    } else if (action_id == "new_file") {
        if (state->active_tab) FileViewWidget::action_new_file(state->active_tab->active_pane->file_view);
    } else if (action_id == "rename_item") {
        if (state->active_tab) FileViewWidget::action_rename_selected(state->active_tab->active_pane->file_view);
    } else if (action_id == "copy_items") {
        if (state->active_tab) FileViewWidget::action_copy(state->active_tab->active_pane->file_view);
    } else if (action_id == "cut_items") {
        if (state->active_tab) FileViewWidget::action_cut(state->active_tab->active_pane->file_view);
    } else if (action_id == "paste_items") {
        if (state->active_tab) FileViewWidget::action_paste(state->active_tab->active_pane->file_view);
    } else if (action_id == "copy_path") {
        if (state->active_tab) FileViewWidget::action_copy_path(state->active_tab->active_pane->file_view);
    } else if (action_id == "trash_items") {
        if (state->active_tab) FileViewWidget::action_trash_selected(state->active_tab->active_pane->file_view);
    } else if (action_id == "delete_permanent") {
        if (state->active_tab) FileViewWidget::action_delete_selected(state->active_tab->active_pane->file_view);
    } else if (action_id == "select_all") {
        if (state->active_tab) FileViewWidget::select_all(state->active_tab->active_pane->file_view);
    } else if (action_id == "refresh_view") {
        if (state->active_tab) FileViewWidget::refresh(state->active_tab->active_pane->file_view);
    } else if (action_id == "search_files") {
        gboolean active = gtk_revealer_get_reveal_child(GTK_REVEALER(state->search_revealer));
        gtk_revealer_set_reveal_child(GTK_REVEALER(state->search_revealer), !active);
        if (!active) {
            gtk_widget_grab_focus(state->search_entry);
        } else if (state->active_tab) {
            FileViewWidget::set_search_query(state->active_tab->active_pane->file_view, "");
        }
    } else if (action_id == "inspector") {
        toggle_inspector(state);
    } else if (action_id == "quick_preview") {
        if (state->active_tab && state->active_tab->active_pane) {
            auto paths = FileViewWidget::get_selected_paths(state->active_tab->active_pane->file_view);
            if (!paths.empty()) {
                QuickPreview::toggle(GTK_WINDOW(state->window), paths[0]);
            }
        }
    } else if (action_id == "toggle_hidden") {
        gboolean act = !gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(state->btn_hidden));
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(state->btn_hidden), act);
    } else if (action_id == "view_grid") {
        for (auto& tab : state->tabs) {
            FileViewWidget::set_view_mode(tab->left_pane.file_view, ViewMode::GRID);
            if (tab->is_dual) FileViewWidget::set_view_mode(tab->right_pane.file_view, ViewMode::GRID);
        }
        gtk_widget_add_css_class(state->btn_grid_view, "files-btn-view-active");
        gtk_widget_remove_css_class(state->btn_list_view, "files-btn-view-active");
    } else if (action_id == "view_list") {
        for (auto& tab : state->tabs) {
            FileViewWidget::set_view_mode(tab->left_pane.file_view, ViewMode::LIST);
            if (tab->is_dual) FileViewWidget::set_view_mode(tab->right_pane.file_view, ViewMode::LIST);
        }
        gtk_widget_add_css_class(state->btn_list_view, "files-btn-view-active");
        gtk_widget_remove_css_class(state->btn_grid_view, "files-btn-view-active");
    } else if (action_id == "zoom_in") {
        for (auto& tab : state->tabs) {
            FileViewWidget::zoom_in(tab->left_pane.file_view);
            if (tab->is_dual) FileViewWidget::zoom_in(tab->right_pane.file_view);
        }
    } else if (action_id == "zoom_out") {
        for (auto& tab : state->tabs) {
            FileViewWidget::zoom_out(tab->left_pane.file_view);
            if (tab->is_dual) FileViewWidget::zoom_out(tab->right_pane.file_view);
        }
    } else if (action_id == "zoom_reset") {
        for (auto& tab : state->tabs) {
            FileViewWidget::zoom_reset(tab->left_pane.file_view);
            if (tab->is_dual) FileViewWidget::zoom_reset(tab->right_pane.file_view);
        }
    } else if (action_id == "command_palette") {
        CommandPalette::show(GTK_WINDOW(state->window), [state](const std::string& id) {
            execute_action(state, id);
        });
    } else if (action_id == "preferences") {
        FilePreferencesDialog::show(state);
    } else if (action_id == "properties") {
        if (state->active_tab) FileViewWidget::action_properties(state->active_tab->active_pane->file_view);
    } else if (action_id == "set_wallpaper") {
        if (state->active_tab) {
            auto sel = FileViewWidget::get_selected_paths(state->active_tab->active_pane->file_view);
            if (!sel.empty()) ThemeEngine::set_wallpaper(sel[0]);
        }
    }
}

static gboolean on_window_key_press(GtkWidget*, GdkEventKey* event, gpointer user_data) {
    auto* state = static_cast<FileManagerState*>(user_data);
    if (!state) return FALSE;

    guint key = event->keyval;
    guint state_mask = event->state & gtk_accelerator_get_default_mod_mask();

    // 1. Spotlight Command Palette
    if (FileShortcuts::matches("command_palette", key, state_mask)) {
        execute_action(state, "command_palette");
        return TRUE;
    }

    // 2. Inspector Panel
    if (FileShortcuts::matches("inspector", key, state_mask)) {
        execute_action(state, "inspector");
        return TRUE;
    }

    // 3. Quick Preview
    if (FileShortcuts::matches("quick_preview", key, state_mask)) {
        execute_action(state, "quick_preview");
        return TRUE;
    }

    // 4. Preferences Center
    if (FileShortcuts::matches("preferences", key, state_mask)) {
        execute_action(state, "preferences");
        return TRUE;
    }

    // 5. Navigation shortcuts
    if (FileShortcuts::matches("nav_back", key, state_mask)) {
        execute_action(state, "nav_back");
        return TRUE;
    }
    if (FileShortcuts::matches("nav_forward", key, state_mask)) {
        execute_action(state, "nav_forward");
        return TRUE;
    }
    if (FileShortcuts::matches("nav_up", key, state_mask)) {
        execute_action(state, "nav_up");
        return TRUE;
    }
    if (FileShortcuts::matches("nav_home", key, state_mask)) {
        execute_action(state, "nav_home");
        return TRUE;
    }
    if (FileShortcuts::matches("edit_path", key, state_mask)) {
        execute_action(state, "edit_path");
        return TRUE;
    }
    if (FileShortcuts::matches("search_files", key, state_mask)) {
        execute_action(state, "search_files");
        return TRUE;
    }
    if (FileShortcuts::matches("new_tab", key, state_mask)) {
        execute_action(state, "new_tab");
        return TRUE;
    }
    if (FileShortcuts::matches("close_tab", key, state_mask)) {
        execute_action(state, "close_tab");
        return TRUE;
    }
    if (FileShortcuts::matches("split_view", key, state_mask)) {
        execute_action(state, "split_view");
        return TRUE;
    }
    if (FileShortcuts::matches("open_terminal", key, state_mask)) {
        execute_action(state, "open_terminal");
        return TRUE;
    }

    // 6. View Switcher & Zoom shortcuts
    if (FileShortcuts::matches("view_grid", key, state_mask)) {
        execute_action(state, "view_grid");
        return TRUE;
    }
    if (FileShortcuts::matches("view_list", key, state_mask)) {
        execute_action(state, "view_list");
        return TRUE;
    }
    if (FileShortcuts::matches("zoom_in", key, state_mask)) {
        execute_action(state, "zoom_in");
        return TRUE;
    }
    if (FileShortcuts::matches("zoom_out", key, state_mask)) {
        execute_action(state, "zoom_out");
        return TRUE;
    }
    if (FileShortcuts::matches("zoom_reset", key, state_mask)) {
        execute_action(state, "zoom_reset");
        return TRUE;
    }
    if (FileShortcuts::matches("toggle_hidden", key, state_mask)) {
        execute_action(state, "toggle_hidden");
        return TRUE;
    }

    // Escape handling for search bar
    if (state_mask == 0 && key == GDK_KEY_Escape) {
        if (gtk_revealer_get_reveal_child(GTK_REVEALER(state->search_revealer))) {
            gtk_entry_set_text(GTK_ENTRY(state->search_entry), "");
            gtk_revealer_set_reveal_child(GTK_REVEALER(state->search_revealer), FALSE);
            if (state->active_tab && state->active_tab->active_pane) {
                FileViewWidget::set_search_query(state->active_tab->active_pane->file_view, "");
                gtk_widget_grab_focus(state->active_tab->active_pane->file_view);
            }
            return TRUE;
        }
    }

    return FALSE;
}

GtkWidget* FileManagerWindow::create(const std::string& initial_path) {
    FileShortcuts::init();

    auto* state = new FileManagerState();

    state->window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_default_size(GTK_WINDOW(state->window), 1080, 680);
    gtk_window_set_icon_name(GTK_WINDOW(state->window), "system-file-manager");
    gtk_widget_add_css_class(state->window, "zenith-files-window");

    GtkWidget* main_vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_container_add(GTK_CONTAINER(state->window), main_vbox);

    // ── 1. Toolbar / HeaderBar ───────────────────────────────────────────────
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

    // Actions Box: Palette, Search, New Folder/Item, Split View, Switcher, Hidden, Inspector, Settings
    GtkWidget* act_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
    gtk_widget_add_css_class(act_box, "files-act-group");

    // Command Palette Button (Ctrl+K)
    state->btn_palette = gtk_button_new_from_icon_name("system-search-symbolic", GTK_ICON_SIZE_BUTTON);
    gtk_widget_set_tooltip_text(state->btn_palette, "Command Palette (Ctrl+K)");
    gtk_widget_add_css_class(state->btn_palette, "files-btn-tool");
    g_signal_connect_swapped(state->btn_palette, "clicked", G_CALLBACK(+[](FileManagerState* s) {
        execute_action(s, "command_palette");
    }), state);
    gtk_box_pack_start(GTK_BOX(act_box), state->btn_palette, FALSE, FALSE, 0);

    // Search Toggle Button (Ctrl+F)
    state->btn_search = gtk_button_new_from_icon_name("edit-find-symbolic", GTK_ICON_SIZE_BUTTON);
    gtk_widget_set_tooltip_text(state->btn_search, "Search Files (Ctrl+F)");
    gtk_widget_add_css_class(state->btn_search, "files-btn-tool");
    g_signal_connect_swapped(state->btn_search, "clicked", G_CALLBACK(+[](FileManagerState* s) {
        execute_action(s, "search_files");
    }), state);
    gtk_box_pack_start(GTK_BOX(act_box), state->btn_search, FALSE, FALSE, 0);

    // New Tab (Ctrl+T)
    state->btn_new_tab = gtk_button_new_from_icon_name("tab-new-symbolic", GTK_ICON_SIZE_BUTTON);
    gtk_widget_set_tooltip_text(state->btn_new_tab, "New Tab (Ctrl+T)");
    gtk_widget_add_css_class(state->btn_new_tab, "files-btn-tool");
    g_signal_connect_swapped(state->btn_new_tab, "clicked", G_CALLBACK(+[](FileManagerState* s) {
        execute_action(s, "new_tab");
    }), state);
    gtk_box_pack_start(GTK_BOX(act_box), state->btn_new_tab, FALSE, FALSE, 0);

    // New Folder (Ctrl+Shift+N)
    state->btn_new_folder = gtk_button_new_from_icon_name("folder-new-symbolic", GTK_ICON_SIZE_BUTTON);
    gtk_widget_set_tooltip_text(state->btn_new_folder, "New Folder (Ctrl+Shift+N)");
    gtk_widget_add_css_class(state->btn_new_folder, "files-btn-tool");
    g_signal_connect_swapped(state->btn_new_folder, "clicked", G_CALLBACK(+[](FileManagerState* s) {
        execute_action(s, "new_folder");
    }), state);
    gtk_box_pack_start(GTK_BOX(act_box), state->btn_new_folder, FALSE, FALSE, 0);

    // Split View / Dual Pane (F3)
    state->btn_dual_pane = gtk_button_new_from_icon_name("view-restore-symbolic", GTK_ICON_SIZE_BUTTON);
    gtk_widget_set_tooltip_text(state->btn_dual_pane, "Split View (F3)");
    gtk_widget_add_css_class(state->btn_dual_pane, "files-btn-tool");
    g_signal_connect_swapped(state->btn_dual_pane, "clicked", G_CALLBACK(toggle_dual_pane), state);
    gtk_box_pack_start(GTK_BOX(act_box), state->btn_dual_pane, FALSE, FALSE, 0);

    // Open Terminal (F4)
    state->btn_term = gtk_button_new_from_icon_name("utilities-terminal-symbolic", GTK_ICON_SIZE_BUTTON);
    gtk_widget_set_tooltip_text(state->btn_term, "Open Terminal (F4)");
    gtk_widget_add_css_class(state->btn_term, "files-btn-tool");
    g_signal_connect_swapped(state->btn_term, "clicked", G_CALLBACK(+[](FileManagerState* s) {
        execute_action(s, "open_terminal");
    }), state);
    gtk_box_pack_start(GTK_BOX(act_box), state->btn_term, FALSE, FALSE, 0);

    // Toggle Hidden Files (Ctrl+H)
    state->btn_hidden = gtk_toggle_button_new();
    gtk_button_set_image(GTK_BUTTON(state->btn_hidden), gtk_image_new_from_icon_name("view-conceal-symbolic", GTK_ICON_SIZE_BUTTON));
    gtk_widget_set_tooltip_text(state->btn_hidden, "Toggle Hidden Files (Ctrl+H)");
    gtk_widget_add_css_class(state->btn_hidden, "files-btn-tool");
    g_signal_connect_swapped(state->btn_hidden, "toggled", G_CALLBACK(+[](FileManagerState* s) {
        gboolean act = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(s->btn_hidden));
        for (auto& tab : s->tabs) {
            FileViewWidget::set_show_hidden(tab->left_pane.file_view, act);
            if (tab->is_dual) FileViewWidget::set_show_hidden(tab->right_pane.file_view, act);
        }
    }), state);
    gtk_box_pack_start(GTK_BOX(act_box), state->btn_hidden, FALSE, FALSE, 0);

    // View Switcher (Grid / List)
    GtkWidget* view_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_widget_add_css_class(view_box, "files-view-switcher");

    state->btn_grid_view = gtk_button_new_from_icon_name("view-grid-symbolic", GTK_ICON_SIZE_BUTTON);
    gtk_widget_set_tooltip_text(state->btn_grid_view, "Grid View (Ctrl+1)");
    gtk_widget_add_css_class(state->btn_grid_view, "files-btn-view-active");
    g_signal_connect_swapped(state->btn_grid_view, "clicked", G_CALLBACK(+[](FileManagerState* s) {
        execute_action(s, "view_grid");
    }), state);
    gtk_box_pack_start(GTK_BOX(view_box), state->btn_grid_view, FALSE, FALSE, 0);

    state->btn_list_view = gtk_button_new_from_icon_name("view-list-symbolic", GTK_ICON_SIZE_BUTTON);
    gtk_widget_set_tooltip_text(state->btn_list_view, "List View (Ctrl+2)");
    g_signal_connect_swapped(state->btn_list_view, "clicked", G_CALLBACK(+[](FileManagerState* s) {
        execute_action(s, "view_list");
    }), state);
    gtk_box_pack_start(GTK_BOX(view_box), state->btn_list_view, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(act_box), view_box, FALSE, FALSE, 0);

    // Inspector Panel Toggle (Ctrl+I)
    state->btn_inspector = gtk_button_new_from_icon_name("dialog-information-symbolic", GTK_ICON_SIZE_BUTTON);
    gtk_widget_set_tooltip_text(state->btn_inspector, "Inspector Panel (Ctrl+I)");
    gtk_widget_add_css_class(state->btn_inspector, "files-btn-tool");
    g_signal_connect_swapped(state->btn_inspector, "clicked", G_CALLBACK(toggle_inspector), state);
    gtk_box_pack_start(GTK_BOX(act_box), state->btn_inspector, FALSE, FALSE, 0);

    // Preferences & Settings (Ctrl+,)
    state->btn_settings = gtk_button_new_from_icon_name("emblem-system-symbolic", GTK_ICON_SIZE_BUTTON);
    gtk_widget_set_tooltip_text(state->btn_settings, "Zenith Preferences & Shortcuts (Ctrl+,)");
    gtk_widget_add_css_class(state->btn_settings, "files-btn-tool");
    g_signal_connect_swapped(state->btn_settings, "clicked", G_CALLBACK(+[](FileManagerState* s) {
        FilePreferencesDialog::show(s);
    }), state);
    gtk_box_pack_start(GTK_BOX(act_box), state->btn_settings, FALSE, FALSE, 0);

    gtk_box_pack_start(GTK_BOX(toolbar), act_box, FALSE, FALSE, 0);

    // ── 2. Filter / Search Bar (Revealer) ────────────────────────────────────
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

    // ── 3. Layout Panes: Sidebar | (Tabs / Panes | Inspector) ─────────────────
    state->main_paned = gtk_paned_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_paned_set_position(GTK_PANED(state->main_paned), 210);
    gtk_box_pack_start(GTK_BOX(main_vbox), state->main_paned, TRUE, TRUE, 0);

    state->sidebar = PlacesSidebar::create([state](const std::string& target) {
        navigate_to(state, target, true);
    });
    gtk_paned_pack1(GTK_PANED(state->main_paned), state->sidebar, FALSE, FALSE);

    // Center Paned: Notebook (pack1) and Inspector (pack2)
    state->center_paned = gtk_paned_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_paned_pack2(GTK_PANED(state->main_paned), state->center_paned, TRUE, FALSE);

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

                if (s->inspector_visible && s->inspector_panel) {
                    auto sel = FileViewWidget::get_selected_paths(tab->active_pane->file_view);
                    InspectorPanel::set_current_directory(s->inspector_panel, tab->active_pane->current_path);
                    InspectorPanel::update_selection(s->inspector_panel, sel);
                }
                break;
            }
        }
    };
    g_signal_connect(state->notebook, "switch-page", G_CALLBACK(switch_page_cb), state);
    gtk_paned_pack1(GTK_PANED(state->center_paned), state->notebook, TRUE, FALSE);

    // Inspector Panel (Pack 2 of center paned, hidden initially)
    state->inspector_panel = InspectorPanel::create();
    gtk_widget_set_no_show_all(state->inspector_panel, TRUE);
    gtk_paned_pack2(GTK_PANED(state->center_paned), state->inspector_panel, FALSE, FALSE);

    // ── 4. Status Bar ────────────────────────────────────────────────────────
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
