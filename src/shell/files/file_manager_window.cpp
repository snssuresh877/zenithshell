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

struct TabState {
    GtkWidget* file_view{nullptr};
    GtkWidget* tab_box{nullptr};
    GtkWidget* tab_label{nullptr};
    std::string current_path;
    std::vector<std::string> back_history;
    std::vector<std::string> forward_history;
    int last_total_items{0};
    int last_selected_items{0};
    uint64_t last_selected_bytes{0};
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
    GtkWidget* btn_term{nullptr};
    GtkWidget* btn_hidden{nullptr};
    GtkWidget* btn_grid_view{nullptr};
    GtkWidget* btn_list_view{nullptr};

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
    gtk_widget_set_sensitive(state->btn_back, !state->active_tab->back_history.empty());
    gtk_widget_set_sensitive(state->btn_forward, !state->active_tab->forward_history.empty());
    gtk_widget_set_sensitive(state->btn_up, state->active_tab->current_path != "/");
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
    if (state->active_tab->last_selected_items > 0) {
        text = std::to_string(state->active_tab->last_selected_items) + " of " +
               std::to_string(state->active_tab->last_total_items) + " selected";
        if (state->active_tab->last_selected_bytes > 0) {
            text += " (" + FileItem::format_size(state->active_tab->last_selected_bytes) + ")";
        }
    } else {
        text = std::to_string(state->active_tab->last_total_items) + (state->active_tab->last_total_items == 1 ? " item" : " items");
    }

    gtk_label_set_text(GTK_LABEL(state->status_label), text.c_str());
}

static void navigate_to(FileManagerState* state, const std::string& path, bool record_history = true) {
    if (!state || !state->active_tab || path.empty()) return;

    if (record_history && !state->active_tab->current_path.empty() && state->active_tab->current_path != path) {
        state->active_tab->back_history.push_back(state->active_tab->current_path);
        state->active_tab->forward_history.clear();
    }

    state->active_tab->current_path = path;

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
    PlacesSidebar::set_active_path(state->sidebar, path);
    FileViewWidget::load_directory(state->active_tab->file_view, path);
    update_disk_space(state, path);
}

static void go_back(FileManagerState* state) {
    if (!state || !state->active_tab || state->active_tab->back_history.empty()) return;

    std::string prev = state->active_tab->back_history.back();
    state->active_tab->back_history.pop_back();

    if (!state->active_tab->current_path.empty()) {
        state->active_tab->forward_history.push_back(state->active_tab->current_path);
    }

    navigate_to(state, prev, false);
}

static void go_forward(FileManagerState* state) {
    if (!state || !state->active_tab || state->active_tab->forward_history.empty()) return;

    std::string next = state->active_tab->forward_history.back();
    state->active_tab->forward_history.pop_back();

    if (!state->active_tab->current_path.empty()) {
        state->active_tab->back_history.push_back(state->active_tab->current_path);
    }

    navigate_to(state, next, false);
}

static void go_up(FileManagerState* state) {
    if (!state || !state->active_tab || state->active_tab->current_path.empty() || state->active_tab->current_path == "/") return;

    GFile* cur = g_file_new_for_path(state->active_tab->current_path.c_str());
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

    int page_num = gtk_notebook_page_num(GTK_NOTEBOOK(state->notebook), tab->file_view);
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

static void create_new_tab(FileManagerState* state, const std::string& initial_path) {
    auto tab = std::make_unique<TabState>();
    TabState* tab_ptr = tab.get();

    tab_ptr->file_view = FileViewWidget::create(
        [state, tab_ptr](const std::string& target) {
            if (state->active_tab == tab_ptr) {
                navigate_to(state, target, true);
            } else {
                if (!tab_ptr->current_path.empty() && tab_ptr->current_path != target) {
                    tab_ptr->back_history.push_back(tab_ptr->current_path);
                    tab_ptr->forward_history.clear();
                }
                tab_ptr->current_path = target;
                
                std::string base_name = target;
                size_t last_slash = target.find_last_of('/');
                if (last_slash != std::string::npos && last_slash != target.length() - 1) {
                    base_name = target.substr(last_slash + 1);
                }
                if (base_name.empty()) base_name = "/";
                if (tab_ptr->tab_label) gtk_label_set_text(GTK_LABEL(tab_ptr->tab_label), base_name.c_str());

                FileViewWidget::load_directory(tab_ptr->file_view, target);
            }
        },
        [state, tab_ptr](int total, int sel_count, uint64_t sel_bytes) {
            tab_ptr->last_total_items = total;
            tab_ptr->last_selected_items = sel_count;
            tab_ptr->last_selected_bytes = sel_bytes;
            if (state->active_tab == tab_ptr) {
                update_status_text(state);
            }
        }
    );

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
    
    int index = gtk_notebook_append_page(GTK_NOTEBOOK(state->notebook), tab_ptr->file_view, tab_box);
    gtk_notebook_set_tab_reorderable(GTK_NOTEBOOK(state->notebook), tab_ptr->file_view, TRUE);
    gtk_widget_show_all(tab_ptr->file_view);
    
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
                FileViewWidget::set_search_query(state->active_tab->file_view, "");
            }
            return TRUE;
        } else if (key == GDK_KEY_t || key == GDK_KEY_T) {
            if (state->active_tab) {
                create_new_tab(state, state->active_tab->current_path);
            } else {
                create_new_tab(state, g_get_home_dir() ? g_get_home_dir() : "/");
            }
            return TRUE;
        } else if (key == GDK_KEY_w || key == GDK_KEY_W) {
            if (state->active_tab) close_tab(state, state->active_tab);
            return TRUE;
        } else if (key == GDK_KEY_1) {
            if (state->active_tab) FileViewWidget::set_view_mode(state->active_tab->file_view, ViewMode::GRID);
            gtk_widget_add_css_class(state->btn_grid_view, "files-btn-view-active");
            gtk_widget_remove_css_class(state->btn_list_view, "files-btn-view-active");
            return TRUE;
        } else if (key == GDK_KEY_2) {
            if (state->active_tab) FileViewWidget::set_view_mode(state->active_tab->file_view, ViewMode::LIST);
            gtk_widget_add_css_class(state->btn_list_view, "files-btn-view-active");
            gtk_widget_remove_css_class(state->btn_grid_view, "files-btn-view-active");
            return TRUE;
        }
    } else if (state_mask == 0) {
        if (key == GDK_KEY_Escape) {
            if (gtk_revealer_get_reveal_child(GTK_REVEALER(state->search_revealer))) {
                gtk_entry_set_text(GTK_ENTRY(state->search_entry), "");
                gtk_revealer_set_reveal_child(GTK_REVEALER(state->search_revealer), FALSE);
                if (state->active_tab) {
                    FileViewWidget::set_search_query(state->active_tab->file_view, "");
                    gtk_widget_grab_focus(state->active_tab->file_view);
                }
                return TRUE;
            }
        } else if (key == GDK_KEY_F4) {
            if (state->active_tab) FileOperations::open_terminal(state->active_tab->current_path);
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
        create_new_tab(s, s->active_tab ? s->active_tab->current_path : (g_get_home_dir() ? g_get_home_dir() : "/"));
    }), state);
    gtk_box_pack_start(GTK_BOX(act_box), state->btn_new_tab, FALSE, FALSE, 0);

    state->btn_search = gtk_button_new_from_icon_name("edit-find-symbolic", GTK_ICON_SIZE_BUTTON);
    gtk_widget_set_tooltip_text(state->btn_search, "Search Files (Ctrl+F)");
    gtk_widget_add_css_class(state->btn_search, "files-btn-tool");
    g_signal_connect_swapped(state->btn_search, "clicked", G_CALLBACK(+[](FileManagerState* s) {
        gboolean active = gtk_revealer_get_reveal_child(GTK_REVEALER(s->search_revealer));
        gtk_revealer_set_reveal_child(GTK_REVEALER(s->search_revealer), !active);
        if (!active) {
            gtk_widget_grab_focus(s->search_entry);
        } else {
            if (s->active_tab) FileViewWidget::set_search_query(s->active_tab->file_view, "");
        }
    }), state);
    gtk_box_pack_start(GTK_BOX(act_box), state->btn_search, FALSE, FALSE, 0);

    state->btn_new_folder = gtk_button_new_from_icon_name("folder-new-symbolic", GTK_ICON_SIZE_BUTTON);
    gtk_widget_set_tooltip_text(state->btn_new_folder, "New Folder (Ctrl+Shift+N)");
    gtk_widget_add_css_class(state->btn_new_folder, "files-btn-tool");
    g_signal_connect_swapped(state->btn_new_folder, "clicked", G_CALLBACK(+[](FileManagerState* s) {
        if (s->active_tab) FileViewWidget::action_new_folder(s->active_tab->file_view);
    }), state);
    gtk_box_pack_start(GTK_BOX(act_box), state->btn_new_folder, FALSE, FALSE, 0);

    state->btn_term = gtk_button_new_from_icon_name("utilities-terminal-symbolic", GTK_ICON_SIZE_BUTTON);
    gtk_widget_set_tooltip_text(state->btn_term, "Open Terminal (F4)");
    gtk_widget_add_css_class(state->btn_term, "files-btn-tool");
    g_signal_connect_swapped(state->btn_term, "clicked", G_CALLBACK(+[](FileManagerState* s) {
        if (s->active_tab) FileOperations::open_terminal(s->active_tab->current_path);
    }), state);
    gtk_box_pack_start(GTK_BOX(act_box), state->btn_term, FALSE, FALSE, 0);

    state->btn_hidden = gtk_toggle_button_new();
    gtk_button_set_image(GTK_BUTTON(state->btn_hidden), gtk_image_new_from_icon_name("view-conceal-symbolic", GTK_ICON_SIZE_BUTTON));
    gtk_widget_set_tooltip_text(state->btn_hidden, "Toggle Hidden Files (Ctrl+H)");
    gtk_widget_add_css_class(state->btn_hidden, "files-btn-tool");
    g_signal_connect_swapped(state->btn_hidden, "toggled", G_CALLBACK(+[](FileManagerState* s) {
        gboolean act = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(s->btn_hidden));
        for (auto& tab : s->tabs) FileViewWidget::set_show_hidden(tab->file_view, act);
    }), state);
    gtk_box_pack_start(GTK_BOX(act_box), state->btn_hidden, FALSE, FALSE, 0);

    // View Switcher (Grid / List)
    GtkWidget* view_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_widget_add_css_class(view_box, "files-view-switcher");

    state->btn_grid_view = gtk_button_new_from_icon_name("view-grid-symbolic", GTK_ICON_SIZE_BUTTON);
    gtk_widget_set_tooltip_text(state->btn_grid_view, "Grid View (Ctrl+1)");
    gtk_widget_add_css_class(state->btn_grid_view, "files-btn-view-active");
    g_signal_connect_swapped(state->btn_grid_view, "clicked", G_CALLBACK(+[](FileManagerState* s) {
        for (auto& tab : s->tabs) FileViewWidget::set_view_mode(tab->file_view, ViewMode::GRID);
        gtk_widget_add_css_class(s->btn_grid_view, "files-btn-view-active");
        gtk_widget_remove_css_class(s->btn_list_view, "files-btn-view-active");
    }), state);
    gtk_box_pack_start(GTK_BOX(view_box), state->btn_grid_view, FALSE, FALSE, 0);

    state->btn_list_view = gtk_button_new_from_icon_name("view-list-symbolic", GTK_ICON_SIZE_BUTTON);
    gtk_widget_set_tooltip_text(state->btn_list_view, "List View (Ctrl+2)");
    g_signal_connect_swapped(state->btn_list_view, "clicked", G_CALLBACK(+[](FileManagerState* s) {
        for (auto& tab : s->tabs) FileViewWidget::set_view_mode(tab->file_view, ViewMode::LIST);
        gtk_widget_add_css_class(s->btn_list_view, "files-btn-view-active");
        gtk_widget_remove_css_class(s->btn_grid_view, "files-btn-view-active");
    }), state);
    gtk_box_pack_start(GTK_BOX(view_box), state->btn_list_view, FALSE, FALSE, 0);

    gtk_box_pack_start(GTK_BOX(act_box), view_box, FALSE, FALSE, 0);
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
        if (s->active_tab) FileViewWidget::set_search_query(s->active_tab->file_view, q ? q : "");
    }), state);
    gtk_box_pack_start(GTK_BOX(search_box), state->search_entry, TRUE, TRUE, 0);

    GtkWidget* btn_close_search = gtk_button_new_from_icon_name("window-close-symbolic", GTK_ICON_SIZE_BUTTON);
    gtk_widget_add_css_class(btn_close_search, "files-btn-close-search");
    g_signal_connect_swapped(btn_close_search, "clicked", G_CALLBACK(+[](FileManagerState* s) {
        gtk_entry_set_text(GTK_ENTRY(s->search_entry), "");
        gtk_revealer_set_reveal_child(GTK_REVEALER(s->search_revealer), FALSE);
        if (s->active_tab) FileViewWidget::set_search_query(s->active_tab->file_view, "");
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
            if (tab->file_view == child) {
                s->active_tab = tab.get();
                PathBarWidget::set_path(s->path_bar, tab->current_path);
                PlacesSidebar::set_active_path(s->sidebar, tab->current_path);
                update_nav_buttons(s);
                update_status_text(s);
                update_disk_space(s, tab->current_path);
                
                std::string title = tab->current_path;
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
