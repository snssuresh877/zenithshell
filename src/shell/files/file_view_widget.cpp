#include "file_view_widget.hpp"
#include "file_item.hpp"
#include "file_operations.hpp"
#include "../../gtk3_compat.hpp"

#include <glib/gstdio.h>
#include <gdk/gdkkeysyms.h>
#include <algorithm>
#include <iostream>
#include <cstring>
#include <sys/stat.h>

namespace zenith {

enum {
    COL_PIXBUF_LARGE = 0,
    COL_PIXBUF_SMALL,
    COL_NAME,
    COL_SIZE_STR,
    COL_TYPE_STR,
    COL_DATE_STR,
    COL_PATH,
    COL_IS_DIR,
    COL_RAW_SIZE,
    COL_RAW_TIME,
    COL_ITEM_PTR,
    NUM_VIEW_COLS
};

struct FileViewData {
    GtkWidget* root_box{nullptr};
    GtkWidget* stack{nullptr};
    GtkWidget* grid_scrolled{nullptr};
    GtkWidget* icon_view{nullptr};
    GtkWidget* list_scrolled{nullptr};
    GtkWidget* tree_view{nullptr};
    GtkListStore* store{nullptr};

    std::string current_path;
    std::vector<std::shared_ptr<FileItem>> all_items;
    std::vector<std::shared_ptr<FileItem>> filtered_items;

    ViewMode view_mode{ViewMode::GRID};
    bool show_hidden{false};
    std::string search_query;
    SortField sort_field{SortField::NAME};
    bool sort_ascending{true};

    GFileMonitor* file_monitor{nullptr};
    gulong monitor_handler_id{0};
    guint debounce_refresh_timer{0};

    FileViewWidget::NavigateCallback on_navigate;
    FileViewWidget::StatusCallback on_status;
};

static void file_view_data_free(gpointer user_data) {
    auto* data = static_cast<FileViewData*>(user_data);
    if (!data) return;

    if (data->debounce_refresh_timer > 0) {
        g_source_remove(data->debounce_refresh_timer);
        data->debounce_refresh_timer = 0;
    }

    if (data->file_monitor) {
        if (data->monitor_handler_id > 0) {
            g_signal_handler_disconnect(data->file_monitor, data->monitor_handler_id);
            data->monitor_handler_id = 0;
        }
        g_file_monitor_cancel(data->file_monitor);
        g_object_unref(data->file_monitor);
        data->file_monitor = nullptr;
    }

    delete data;
}

static FileViewData* get_data(GtkWidget* widget) {
    return static_cast<FileViewData*>(g_object_get_data(G_OBJECT(widget), "file_view_data"));
}

static void update_status_bar(FileViewData* data) {
    if (!data || !data->on_status) return;

    std::vector<std::string> sel = FileViewWidget::get_selected_paths(data->root_box);
    int total = static_cast<int>(data->filtered_items.size());
    int sel_count = static_cast<int>(sel.size());
    uint64_t sel_bytes = 0;

    for (const auto& p : sel) {
        for (const auto& item : data->filtered_items) {
            if (item->path == p) {
                if (!item->is_directory) {
                    sel_bytes += item->size;
                }
                break;
            }
        }
    }

    data->on_status(total, sel_count, sel_bytes);
}

static void apply_sort(FileViewData* data) {
    if (!data) return;

    std::sort(data->filtered_items.begin(), data->filtered_items.end(),
              [data](const std::shared_ptr<FileItem>& a, const std::shared_ptr<FileItem>& b) {
        // Folders always first
        if (a->is_directory != b->is_directory) {
            return a->is_directory > b->is_directory;
        }

        int cmp = 0;
        switch (data->sort_field) {
            case SortField::NAME:
                cmp = g_utf8_collate(a->name.c_str(), b->name.c_str());
                break;
            case SortField::SIZE:
                if (a->size < b->size) cmp = -1;
                else if (a->size > b->size) cmp = 1;
                else cmp = g_utf8_collate(a->name.c_str(), b->name.c_str());
                break;
            case SortField::TYPE:
                cmp = g_utf8_collate(a->mime_type.c_str(), b->mime_type.c_str());
                if (cmp == 0) cmp = g_utf8_collate(a->name.c_str(), b->name.c_str());
                break;
            case SortField::DATE:
                if (a->mtime < b->mtime) cmp = -1;
                else if (a->mtime > b->mtime) cmp = 1;
                else cmp = g_utf8_collate(a->name.c_str(), b->name.c_str());
                break;
        }

        return data->sort_ascending ? (cmp < 0) : (cmp > 0);
    });
}

static void repopulate_store(FileViewData* data) {
    if (!data || !data->store) return;

    gtk_list_store_clear(data->store);

    // Apply filtering
    data->filtered_items.clear();
    for (const auto& item : data->all_items) {
        if (!data->show_hidden && item->is_hidden) {
            continue;
        }

        if (!data->search_query.empty()) {
            std::string q = data->search_query;
            std::string n = item->name;
            std::transform(q.begin(), q.end(), q.begin(), ::tolower);
            std::transform(n.begin(), n.end(), n.begin(), ::tolower);
            if (n.find(q) == std::string::npos) {
                continue;
            }
        }

        data->filtered_items.push_back(item);
    }

    apply_sort(data);

    // Populate GtkListStore
    GtkTreeIter iter;
    for (const auto& item : data->filtered_items) {
        gtk_list_store_append(data->store, &iter);
        gtk_list_store_set(data->store, &iter,
            COL_PIXBUF_LARGE, item->pixbuf_large,
            COL_PIXBUF_SMALL, item->pixbuf_small,
            COL_NAME, item->display_name.c_str(),
            COL_SIZE_STR, item->formatted_size.c_str(),
            COL_TYPE_STR, item->mime_type.c_str(),
            COL_DATE_STR, item->formatted_date.c_str(),
            COL_PATH, item->path.c_str(),
            COL_IS_DIR, item->is_directory,
            COL_RAW_SIZE, static_cast<guint64>(item->size),
            COL_RAW_TIME, static_cast<gint64>(item->mtime),
            COL_ITEM_PTR, item.get(),
            -1
        );
    }

    update_status_bar(data);
}

static gboolean on_debounce_refresh(gpointer user_data) {
    auto* data = static_cast<FileViewData*>(user_data);
    if (!data) return G_SOURCE_REMOVE;

    data->debounce_refresh_timer = 0;
    FileViewWidget::refresh(data->root_box);
    return G_SOURCE_REMOVE;
}

static void on_monitor_changed(GFileMonitor*, GFile*, GFile*, GFileMonitorEvent event_type, gpointer user_data) {
    auto* data = static_cast<FileViewData*>(user_data);
    if (!data) return;

    // Filter uninteresting events
    if (event_type == G_FILE_MONITOR_EVENT_CHANGED ||
        event_type == G_FILE_MONITOR_EVENT_CHANGES_DONE_HINT ||
        event_type == G_FILE_MONITOR_EVENT_DELETED ||
        event_type == G_FILE_MONITOR_EVENT_CREATED ||
        event_type == G_FILE_MONITOR_EVENT_MOVED_IN ||
        event_type == G_FILE_MONITOR_EVENT_MOVED_OUT) {

        if (data->debounce_refresh_timer == 0) {
            data->debounce_refresh_timer = g_timeout_add(150, on_debounce_refresh, data);
        }
    }
}

static void setup_file_monitor(FileViewData* data, const std::string& path) {
    if (data->debounce_refresh_timer > 0) {
        g_source_remove(data->debounce_refresh_timer);
        data->debounce_refresh_timer = 0;
    }

    if (data->file_monitor) {
        if (data->monitor_handler_id > 0) {
            g_signal_handler_disconnect(data->file_monitor, data->monitor_handler_id);
            data->monitor_handler_id = 0;
        }
        g_file_monitor_cancel(data->file_monitor);
        g_object_unref(data->file_monitor);
        data->file_monitor = nullptr;
    }

    GFile* gfile = g_file_new_for_path(path.c_str());
    data->file_monitor = g_file_monitor_directory(gfile, G_FILE_MONITOR_WATCH_MOUNTS, nullptr, nullptr);
    g_object_unref(gfile);

    if (data->file_monitor) {
        data->monitor_handler_id = g_signal_connect(data->file_monitor, "changed",
                                                    G_CALLBACK(on_monitor_changed), data);
    }
}

static void on_icon_activated(GtkIconView*, GtkTreePath* path, gpointer user_data) {
    auto* data = static_cast<FileViewData*>(user_data);
    if (!data || !data->store) return;

    GtkTreeIter iter;
    if (gtk_tree_model_get_iter(GTK_TREE_MODEL(data->store), &iter, path)) {
        gchar* item_path = nullptr;
        gboolean is_dir = FALSE;
        gtk_tree_model_get(GTK_TREE_MODEL(data->store), &iter,
                           COL_PATH, &item_path,
                           COL_IS_DIR, &is_dir,
                           -1);

        if (item_path) {
            std::string p(item_path);
            g_free(item_path);

            if (is_dir && data->on_navigate) {
                data->on_navigate(p);
            } else {
                FileOperations::launch_file(p);
            }
        }
    }
}

static void on_row_activated(GtkTreeView*, GtkTreePath* path, GtkTreeViewColumn*, gpointer user_data) {
    auto* data = static_cast<FileViewData*>(user_data);
    if (!data || !data->store) return;

    GtkTreeIter iter;
    if (gtk_tree_model_get_iter(GTK_TREE_MODEL(data->store), &iter, path)) {
        gchar* item_path = nullptr;
        gboolean is_dir = FALSE;
        gtk_tree_model_get(GTK_TREE_MODEL(data->store), &iter,
                           COL_PATH, &item_path,
                           COL_IS_DIR, &is_dir,
                           -1);

        if (item_path) {
            std::string p(item_path);
            g_free(item_path);

            if (is_dir && data->on_navigate) {
                data->on_navigate(p);
            } else {
                FileOperations::launch_file(p);
            }
        }
    }
}

static void on_selection_changed(gpointer, gpointer user_data) {
    auto* data = static_cast<FileViewData*>(user_data);
    update_status_bar(data);
}

// Show right click context menu
static void show_context_menu(FileViewData* data, GdkEventButton* event) {
    GtkWidget* menu = gtk_menu_new();
    gtk_widget_add_css_class(menu, "files-context-menu");

    std::vector<std::string> selected = FileViewWidget::get_selected_paths(data->root_box);

    if (!selected.empty()) {
        // ── Open ──────────────────────────────────────────────────────────────
        GtkWidget* item_open = gtk_menu_item_new_with_label(selected.size() == 1 ? "Open" : "Open Selected");
        g_signal_connect_swapped(item_open, "activate", G_CALLBACK(FileViewWidget::action_open_selected), data->root_box);
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), item_open);

        // "Open With…" only for single file selection
        if (selected.size() == 1) {
            struct stat st;
            if (!(stat(selected[0].c_str(), &st) == 0 && S_ISDIR(st.st_mode))) {
                GtkWidget* item_open_with = gtk_menu_item_new_with_label("Open With…");
                std::string* sel_path = new std::string(selected[0]);
                g_object_set_data_full(G_OBJECT(item_open_with), "item_path", sel_path, [](gpointer p) { delete static_cast<std::string*>(p); });
                g_signal_connect(item_open_with, "activate", G_CALLBACK(+[](GtkMenuItem* mi, gpointer) {
                    auto* p = static_cast<std::string*>(g_object_get_data(G_OBJECT(mi), "item_path"));
                    if (p) FileOperations::open_with_dialog(*p, nullptr);
                }), nullptr);
                gtk_menu_shell_append(GTK_MENU_SHELL(menu), item_open_with);
            }
        }

        GtkWidget* item_term = gtk_menu_item_new_with_label("Open in Terminal");
        g_signal_connect_swapped(item_term, "activate", G_CALLBACK(FileViewWidget::action_open_terminal), data->root_box);
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), item_term);

        gtk_menu_shell_append(GTK_MENU_SHELL(menu), gtk_separator_menu_item_new());

        // ── Archive: Extract Here (if archive selected) ───────────────────────
        bool any_archive = false;
        for (const auto& sp : selected) {
            GFile* gf = g_file_new_for_path(sp.c_str());
            GFileInfo* fi = g_file_query_info(gf, "standard::content-type",
                                               G_FILE_QUERY_INFO_NONE, nullptr, nullptr);
            if (fi) {
                const char* ct = g_file_info_get_content_type(fi);
                if (ct && FileOperations::is_archive(ct)) any_archive = true;
                g_object_unref(fi);
            }
            g_object_unref(gf);
            if (any_archive) break;
        }

        if (any_archive && selected.size() == 1) {
            GtkWidget* item_extract = gtk_menu_item_new_with_label("Extract Here");
            std::string* arc_path = new std::string(selected[0]);
            std::string* tgt_dir  = new std::string(data->current_path);
            g_object_set_data_full(G_OBJECT(item_extract), "arc_path", arc_path, [](gpointer p) { delete static_cast<std::string*>(p); });
            g_object_set_data_full(G_OBJECT(item_extract), "tgt_dir",  tgt_dir,  [](gpointer p) { delete static_cast<std::string*>(p); });
            g_signal_connect(item_extract, "activate", G_CALLBACK(+[](GtkMenuItem* mi, gpointer) {
                auto* ap = static_cast<std::string*>(g_object_get_data(G_OBJECT(mi), "arc_path"));
                auto* td = static_cast<std::string*>(g_object_get_data(G_OBJECT(mi), "tgt_dir"));
                if (ap && td) FileOperations::extract_archive(*ap, *td);
            }), nullptr);
            gtk_menu_shell_append(GTK_MENU_SHELL(menu), item_extract);
            gtk_menu_shell_append(GTK_MENU_SHELL(menu), gtk_separator_menu_item_new());
        }

        // ── Compress ──────────────────────────────────────────────────────────
        GtkWidget* item_compress = gtk_menu_item_new_with_label("Compress…");
        std::vector<std::string>* sel_copy = new std::vector<std::string>(selected);
        g_object_set_data_full(G_OBJECT(item_compress), "sel_paths", sel_copy, [](gpointer p) { delete static_cast<std::vector<std::string>*>(p); });
        g_signal_connect(item_compress, "activate", G_CALLBACK(+[](GtkMenuItem* mi, gpointer) {
            auto* paths = static_cast<std::vector<std::string>*>(g_object_get_data(G_OBJECT(mi), "sel_paths"));
            if (paths) FileOperations::compress_files(*paths);
        }), nullptr);
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), item_compress);

        gtk_menu_shell_append(GTK_MENU_SHELL(menu), gtk_separator_menu_item_new());

        // ── Clipboard ─────────────────────────────────────────────────────────
        GtkWidget* item_cut = gtk_menu_item_new_with_label("Cut (Ctrl+X)");
        g_signal_connect_swapped(item_cut, "activate", G_CALLBACK(FileViewWidget::action_cut), data->root_box);
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), item_cut);

        GtkWidget* item_copy = gtk_menu_item_new_with_label("Copy (Ctrl+C)");
        g_signal_connect_swapped(item_copy, "activate", G_CALLBACK(FileViewWidget::action_copy), data->root_box);
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), item_copy);

        gtk_menu_shell_append(GTK_MENU_SHELL(menu), gtk_separator_menu_item_new());

        // ── Rename / Delete ───────────────────────────────────────────────────
        if (selected.size() == 1) {
            GtkWidget* item_rename = gtk_menu_item_new_with_label("Rename (F2)");
            g_signal_connect_swapped(item_rename, "activate", G_CALLBACK(FileViewWidget::action_rename_selected), data->root_box);
            gtk_menu_shell_append(GTK_MENU_SHELL(menu), item_rename);
        }

        GtkWidget* item_trash = gtk_menu_item_new_with_label("Move to Trash (Del)");
        g_signal_connect_swapped(item_trash, "activate", G_CALLBACK(FileViewWidget::action_trash_selected), data->root_box);
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), item_trash);

        GtkWidget* item_delete = gtk_menu_item_new_with_label("Delete Permanently (Shift+Del)");
        g_signal_connect_swapped(item_delete, "activate", G_CALLBACK(FileViewWidget::action_delete_selected), data->root_box);
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), item_delete);

        gtk_menu_shell_append(GTK_MENU_SHELL(menu), gtk_separator_menu_item_new());

        GtkWidget* item_prop = gtk_menu_item_new_with_label("Properties");
        g_signal_connect_swapped(item_prop, "activate", G_CALLBACK(FileViewWidget::action_properties), data->root_box);
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), item_prop);
    } else {
        // Empty space menu
        GtkWidget* item_folder = gtk_menu_item_new_with_label("New Folder (Ctrl+Shift+N)");
        g_signal_connect_swapped(item_folder, "activate", G_CALLBACK(FileViewWidget::action_new_folder), data->root_box);
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), item_folder);

        GtkWidget* item_file = gtk_menu_item_new_with_label("New Document");
        g_signal_connect_swapped(item_file, "activate", G_CALLBACK(FileViewWidget::action_new_file), data->root_box);
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), item_file);

        GtkWidget* item_term = gtk_menu_item_new_with_label("Open Terminal Here");
        g_signal_connect_swapped(item_term, "activate", G_CALLBACK(FileViewWidget::action_open_terminal), data->root_box);
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), item_term);

        gtk_menu_shell_append(GTK_MENU_SHELL(menu), gtk_separator_menu_item_new());

        GtkWidget* item_paste = gtk_menu_item_new_with_label("Paste (Ctrl+V)");
        gtk_widget_set_sensitive(item_paste, FileOperations::has_clipboard_files());
        g_signal_connect_swapped(item_paste, "activate", G_CALLBACK(FileViewWidget::action_paste), data->root_box);
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), item_paste);

        gtk_menu_shell_append(GTK_MENU_SHELL(menu), gtk_separator_menu_item_new());

        GtkWidget* item_select_all = gtk_menu_item_new_with_label("Select All (Ctrl+A)");
        g_signal_connect_swapped(item_select_all, "activate", G_CALLBACK(FileViewWidget::select_all), data->root_box);
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), item_select_all);

        GtkWidget* item_hidden = gtk_check_menu_item_new_with_label("Show Hidden Files (Ctrl+H)");
        gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(item_hidden), data->show_hidden);
        g_signal_connect_swapped(item_hidden, "toggled", G_CALLBACK(+[](FileViewData* d) {
            FileViewWidget::set_show_hidden(d->root_box, !d->show_hidden);
        }), data);
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), item_hidden);

        GtkWidget* item_refresh = gtk_menu_item_new_with_label("Refresh (F5)");
        g_signal_connect_swapped(item_refresh, "activate", G_CALLBACK(FileViewWidget::refresh), data->root_box);
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), item_refresh);
    }

    gtk_widget_show_all(menu);
    gtk_menu_popup_at_pointer(GTK_MENU(menu), reinterpret_cast<GdkEvent*>(event));
}

static gboolean on_icon_button_press(GtkWidget* widget, GdkEventButton* event, gpointer user_data) {
    auto* data = static_cast<FileViewData*>(user_data);
    if (!data) return FALSE;

    if (event->type == GDK_BUTTON_PRESS && event->button == 3) {
        GtkTreePath* path = gtk_icon_view_get_path_at_pos(GTK_ICON_VIEW(widget), static_cast<gint>(event->x), static_cast<gint>(event->y));
        if (path) {
            if (!gtk_icon_view_path_is_selected(GTK_ICON_VIEW(widget), path)) {
                gtk_icon_view_unselect_all(GTK_ICON_VIEW(widget));
                gtk_icon_view_select_path(GTK_ICON_VIEW(widget), path);
            }
            gtk_tree_path_free(path);
        } else {
            gtk_icon_view_unselect_all(GTK_ICON_VIEW(widget));
        }
        show_context_menu(data, event);
        return TRUE;
    }
    return FALSE;
}

static gboolean on_tree_button_press(GtkWidget* widget, GdkEventButton* event, gpointer user_data) {
    auto* data = static_cast<FileViewData*>(user_data);
    if (!data) return FALSE;

    if (event->type == GDK_BUTTON_PRESS && event->button == 3) {
        GtkTreePath* path = nullptr;
        if (gtk_tree_view_get_path_at_pos(GTK_TREE_VIEW(widget), static_cast<gint>(event->x), static_cast<gint>(event->y), &path, nullptr, nullptr, nullptr)) {
            GtkTreeSelection* sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(widget));
            if (!gtk_tree_selection_path_is_selected(sel, path)) {
                gtk_tree_selection_unselect_all(sel);
                gtk_tree_selection_select_path(sel, path);
            }
            gtk_tree_path_free(path);
        } else {
            GtkTreeSelection* sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(widget));
            gtk_tree_selection_unselect_all(sel);
        }
        show_context_menu(data, event);
        return TRUE;
    }
    return FALSE;
}

static gboolean on_key_press(GtkWidget*, GdkEventKey* event, gpointer user_data) {
    auto* data = static_cast<FileViewData*>(user_data);
    if (!data) return FALSE;

    guint key = event->keyval;
    guint state = event->state & gtk_accelerator_get_default_mod_mask();

    if (state == 0) {
        if (key == GDK_KEY_Return || key == GDK_KEY_KP_Enter) {
            FileViewWidget::action_open_selected(data->root_box);
            return TRUE;
        } else if (key == GDK_KEY_BackSpace) {
            if (data->current_path != "/" && data->on_navigate) {
                GFile* current = g_file_new_for_path(data->current_path.c_str());
                GFile* parent = g_file_get_parent(current);
                if (parent) {
                    char* parent_path = g_file_get_path(parent);
                    if (parent_path) {
                        data->on_navigate(parent_path);
                        g_free(parent_path);
                    }
                    g_object_unref(parent);
                }
                g_object_unref(current);
            }
            return TRUE;
        } else if (key == GDK_KEY_Delete) {
            FileViewWidget::action_trash_selected(data->root_box);
            return TRUE;
        } else if (key == GDK_KEY_F2) {
            FileViewWidget::action_rename_selected(data->root_box);
            return TRUE;
        } else if (key == GDK_KEY_F5) {
            FileViewWidget::refresh(data->root_box);
            return TRUE;
        }
    } else if (state == GDK_SHIFT_MASK) {
        if (key == GDK_KEY_Delete) {
            FileViewWidget::action_delete_selected(data->root_box);
            return TRUE;
        }
    } else if (state == GDK_CONTROL_MASK) {
        if (key == GDK_KEY_c || key == GDK_KEY_C) {
            FileViewWidget::action_copy(data->root_box);
            return TRUE;
        } else if (key == GDK_KEY_x || key == GDK_KEY_X) {
            FileViewWidget::action_cut(data->root_box);
            return TRUE;
        } else if (key == GDK_KEY_v || key == GDK_KEY_V) {
            FileViewWidget::action_paste(data->root_box);
            return TRUE;
        } else if (key == GDK_KEY_a || key == GDK_KEY_A) {
            FileViewWidget::select_all(data->root_box);
            return TRUE;
        } else if (key == GDK_KEY_h || key == GDK_KEY_H) {
            FileViewWidget::set_show_hidden(data->root_box, !data->show_hidden);
            return TRUE;
        } else if (key == GDK_KEY_r || key == GDK_KEY_R) {
            FileViewWidget::refresh(data->root_box);
            return TRUE;
        }
    } else if (state == (GDK_CONTROL_MASK | GDK_SHIFT_MASK)) {
        if (key == GDK_KEY_n || key == GDK_KEY_N) {
            FileViewWidget::action_new_folder(data->root_box);
            return TRUE;
        }
    }

    return FALSE;
}

// Column header clicked in TreeView
static void on_column_clicked(GtkTreeViewColumn* col, gpointer user_data) {
    auto* data = static_cast<FileViewData*>(user_data);
    if (!data) return;

    gint sort_id = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(col), "sort_id"));
    SortField target_field = static_cast<SortField>(sort_id);

    if (data->sort_field == target_field) {
        data->sort_ascending = !data->sort_ascending;
    } else {
        data->sort_field = target_field;
        data->sort_ascending = true;
    }

    gtk_tree_view_column_set_sort_indicator(col, TRUE);
    gtk_tree_view_column_set_sort_order(col, data->sort_ascending ? GTK_SORT_ASCENDING : GTK_SORT_DESCENDING);

    apply_sort(data);
    repopulate_store(data);
}

GtkWidget* FileViewWidget::create(NavigateCallback on_navigate, StatusCallback on_status) {
    auto* data = new FileViewData();
    data->on_navigate = std::move(on_navigate);
    data->on_status = std::move(on_status);

    data->root_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_add_css_class(data->root_box, "files-view-container");

    data->stack = gtk_stack_new();
    gtk_stack_set_transition_type(GTK_STACK(data->stack), GTK_STACK_TRANSITION_TYPE_CROSSFADE);
    gtk_box_pack_start(GTK_BOX(data->root_box), data->stack, TRUE, TRUE, 0);

    // ListStore definition
    data->store = gtk_list_store_new(
        NUM_VIEW_COLS,
        GDK_TYPE_PIXBUF,    // COL_PIXBUF_LARGE
        GDK_TYPE_PIXBUF,    // COL_PIXBUF_SMALL
        G_TYPE_STRING,      // COL_NAME
        G_TYPE_STRING,      // COL_SIZE_STR
        G_TYPE_STRING,      // COL_TYPE_STR
        G_TYPE_STRING,      // COL_DATE_STR
        G_TYPE_STRING,      // COL_PATH
        G_TYPE_BOOLEAN,     // COL_IS_DIR
        G_TYPE_UINT64,      // COL_RAW_SIZE
        G_TYPE_INT64,       // COL_RAW_TIME
        G_TYPE_POINTER      // COL_ITEM_PTR
    );

    // 1. Grid View (GtkIconView)
    data->grid_scrolled = gtk_scrolled_window_new(nullptr, nullptr);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(data->grid_scrolled),
                                   GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

    data->icon_view = gtk_icon_view_new_with_model(GTK_TREE_MODEL(data->store));
    gtk_icon_view_set_pixbuf_column(GTK_ICON_VIEW(data->icon_view), COL_PIXBUF_LARGE);
    gtk_icon_view_set_text_column(GTK_ICON_VIEW(data->icon_view), COL_NAME);
    gtk_icon_view_set_selection_mode(GTK_ICON_VIEW(data->icon_view), GTK_SELECTION_MULTIPLE);
    gtk_icon_view_set_item_width(GTK_ICON_VIEW(data->icon_view), 96);
    gtk_icon_view_set_row_spacing(GTK_ICON_VIEW(data->icon_view), 12);
    gtk_icon_view_set_column_spacing(GTK_ICON_VIEW(data->icon_view), 12);
    gtk_icon_view_set_margin(GTK_ICON_VIEW(data->icon_view), 16);
    gtk_widget_add_css_class(data->icon_view, "files-icon-view");

    g_signal_connect(data->icon_view, "item-activated", G_CALLBACK(on_icon_activated), data);
    g_signal_connect(data->icon_view, "selection-changed", G_CALLBACK(on_selection_changed), data);
    g_signal_connect(data->icon_view, "button-press-event", G_CALLBACK(on_icon_button_press), data);
    g_signal_connect(data->icon_view, "key-press-event", G_CALLBACK(on_key_press), data);

    gtk_container_add(GTK_CONTAINER(data->grid_scrolled), data->icon_view);
    gtk_stack_add_named(GTK_STACK(data->stack), data->grid_scrolled, "grid");

    // 2. List View (GtkTreeView)
    data->list_scrolled = gtk_scrolled_window_new(nullptr, nullptr);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(data->list_scrolled),
                                   GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

    data->tree_view = gtk_tree_view_new_with_model(GTK_TREE_MODEL(data->store));
    gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(data->tree_view), TRUE);
    gtk_widget_add_css_class(data->tree_view, "files-tree-view");

    GtkTreeSelection* sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(data->tree_view));
    gtk_tree_selection_set_mode(sel, GTK_SELECTION_MULTIPLE);
    g_signal_connect(sel, "changed", G_CALLBACK(on_selection_changed), data);

    // Column 1: Name (Icon + Text)
    {
        GtkTreeViewColumn* col = gtk_tree_view_column_new();
        gtk_tree_view_column_set_title(col, "Name");
        gtk_tree_view_column_set_resizable(col, TRUE);
        gtk_tree_view_column_set_min_width(col, 220);
        gtk_tree_view_column_set_expand(col, TRUE);
        gtk_tree_view_column_set_clickable(col, TRUE);
        g_object_set_data(G_OBJECT(col), "sort_id", GINT_TO_POINTER(SortField::NAME));
        g_signal_connect(col, "clicked", G_CALLBACK(on_column_clicked), data);

        GtkCellRenderer* pix_rend = gtk_cell_renderer_pixbuf_new();
        gtk_tree_view_column_pack_start(col, pix_rend, FALSE);
        gtk_tree_view_column_add_attribute(col, pix_rend, "pixbuf", COL_PIXBUF_SMALL);

        GtkCellRenderer* txt_rend = gtk_cell_renderer_text_new();
        g_object_set(txt_rend, "ellipsize", PANGO_ELLIPSIZE_END, nullptr);
        gtk_tree_view_column_pack_start(col, txt_rend, TRUE);
        gtk_tree_view_column_add_attribute(col, txt_rend, "text", COL_NAME);

        gtk_tree_view_append_column(GTK_TREE_VIEW(data->tree_view), col);
    }

    // Column 2: Size
    {
        GtkTreeViewColumn* col = gtk_tree_view_column_new();
        gtk_tree_view_column_set_title(col, "Size");
        gtk_tree_view_column_set_resizable(col, TRUE);
        gtk_tree_view_column_set_min_width(col, 90);
        gtk_tree_view_column_set_clickable(col, TRUE);
        g_object_set_data(G_OBJECT(col), "sort_id", GINT_TO_POINTER(SortField::SIZE));
        g_signal_connect(col, "clicked", G_CALLBACK(on_column_clicked), data);

        GtkCellRenderer* txt_rend = gtk_cell_renderer_text_new();
        g_object_set(txt_rend, "xalign", 1.0f, nullptr);
        gtk_tree_view_column_pack_start(col, txt_rend, TRUE);
        gtk_tree_view_column_add_attribute(col, txt_rend, "text", COL_SIZE_STR);

        gtk_tree_view_append_column(GTK_TREE_VIEW(data->tree_view), col);
    }

    // Column 3: Type
    {
        GtkTreeViewColumn* col = gtk_tree_view_column_new();
        gtk_tree_view_column_set_title(col, "Type");
        gtk_tree_view_column_set_resizable(col, TRUE);
        gtk_tree_view_column_set_min_width(col, 130);
        gtk_tree_view_column_set_clickable(col, TRUE);
        g_object_set_data(G_OBJECT(col), "sort_id", GINT_TO_POINTER(SortField::TYPE));
        g_signal_connect(col, "clicked", G_CALLBACK(on_column_clicked), data);

        GtkCellRenderer* txt_rend = gtk_cell_renderer_text_new();
        gtk_tree_view_column_pack_start(col, txt_rend, TRUE);
        gtk_tree_view_column_add_attribute(col, txt_rend, "text", COL_TYPE_STR);

        gtk_tree_view_append_column(GTK_TREE_VIEW(data->tree_view), col);
    }

    // Column 4: Date Modified
    {
        GtkTreeViewColumn* col = gtk_tree_view_column_new();
        gtk_tree_view_column_set_title(col, "Date Modified");
        gtk_tree_view_column_set_resizable(col, TRUE);
        gtk_tree_view_column_set_min_width(col, 150);
        gtk_tree_view_column_set_clickable(col, TRUE);
        g_object_set_data(G_OBJECT(col), "sort_id", GINT_TO_POINTER(SortField::DATE));
        g_signal_connect(col, "clicked", G_CALLBACK(on_column_clicked), data);

        GtkCellRenderer* txt_rend = gtk_cell_renderer_text_new();
        gtk_tree_view_column_pack_start(col, txt_rend, TRUE);
        gtk_tree_view_column_add_attribute(col, txt_rend, "text", COL_DATE_STR);

        gtk_tree_view_append_column(GTK_TREE_VIEW(data->tree_view), col);
    }

    g_signal_connect(data->tree_view, "row-activated", G_CALLBACK(on_row_activated), data);
    g_signal_connect(data->tree_view, "button-press-event", G_CALLBACK(on_tree_button_press), data);
    g_signal_connect(data->tree_view, "key-press-event", G_CALLBACK(on_key_press), data);

    gtk_container_add(GTK_CONTAINER(data->list_scrolled), data->tree_view);
    gtk_stack_add_named(GTK_STACK(data->stack), data->list_scrolled, "list");

    gtk_stack_set_visible_child_name(GTK_STACK(data->stack), "grid");

    g_object_set_data_full(G_OBJECT(data->root_box), "file_view_data", data, file_view_data_free);
    return data->root_box;
}

void FileViewWidget::load_directory(GtkWidget* widget, const std::string& path) {
    auto* data = get_data(widget);
    if (!data) return;

    data->current_path = path;
    data->all_items.clear();

    GFile* dir = g_file_new_for_path(path.c_str());
    GError* error = nullptr;
    GFileEnumerator* enumerator = g_file_enumerate_children(
        dir,
        "standard::*,time::*",
        G_FILE_QUERY_INFO_NONE,
        nullptr,
        &error
    );

    if (error) {
        std::cerr << "[FileViewWidget] Failed to enumerate directory: " << error->message << "\n";
        g_error_free(error);
        g_object_unref(dir);
        repopulate_store(data);
        return;
    }

    if (enumerator) {
        while (true) {
            GFileInfo* info = g_file_enumerator_next_file(enumerator, nullptr, nullptr);
            if (!info) break;

            const char* name = g_file_info_get_name(info);
            GFile* child_file = g_file_get_child(dir, name);

            auto item = FileItem::from_file_info(child_file, info, 48, 20);
            if (item) {
                data->all_items.push_back(item);
            }

            g_object_unref(child_file);
            g_object_unref(info);
        }
        g_object_unref(enumerator);
    }
    g_object_unref(dir);

    setup_file_monitor(data, path);
    repopulate_store(data);
}

std::string FileViewWidget::get_current_directory(GtkWidget* widget) {
    auto* data = get_data(widget);
    return data ? data->current_path : "";
}

void FileViewWidget::refresh(GtkWidget* widget) {
    auto* data = get_data(widget);
    if (!data || data->current_path.empty()) return;
    load_directory(widget, data->current_path);
}

void FileViewWidget::set_view_mode(GtkWidget* widget, ViewMode mode) {
    auto* data = get_data(widget);
    if (!data) return;

    data->view_mode = mode;
    gtk_stack_set_visible_child_name(GTK_STACK(data->stack), (mode == ViewMode::GRID) ? "grid" : "list");
}

ViewMode FileViewWidget::get_view_mode(GtkWidget* widget) {
    auto* data = get_data(widget);
    return data ? data->view_mode : ViewMode::GRID;
}

void FileViewWidget::set_show_hidden(GtkWidget* widget, bool show) {
    auto* data = get_data(widget);
    if (!data) return;

    data->show_hidden = show;
    repopulate_store(data);
}

bool FileViewWidget::get_show_hidden(GtkWidget* widget) {
    auto* data = get_data(widget);
    return data ? data->show_hidden : false;
}

void FileViewWidget::set_search_query(GtkWidget* widget, const std::string& query) {
    auto* data = get_data(widget);
    if (!data) return;

    data->search_query = query;
    repopulate_store(data);
}

void FileViewWidget::set_sort(GtkWidget* widget, SortField field, bool ascending) {
    auto* data = get_data(widget);
    if (!data) return;

    data->sort_field = field;
    data->sort_ascending = ascending;
    apply_sort(data);
    repopulate_store(data);
}

std::vector<std::string> FileViewWidget::get_selected_paths(GtkWidget* widget) {
    auto* data = get_data(widget);
    std::vector<std::string> results;
    if (!data || !data->store) return results;

    if (data->view_mode == ViewMode::GRID) {
        GList* list = gtk_icon_view_get_selected_items(GTK_ICON_VIEW(data->icon_view));
        for (GList* l = list; l != nullptr; l = l->next) {
            auto* path = static_cast<GtkTreePath*>(l->data);
            GtkTreeIter iter;
            if (gtk_tree_model_get_iter(GTK_TREE_MODEL(data->store), &iter, path)) {
                gchar* p = nullptr;
                gtk_tree_model_get(GTK_TREE_MODEL(data->store), &iter, COL_PATH, &p, -1);
                if (p) {
                    results.emplace_back(p);
                    g_free(p);
                }
            }
        }
        g_list_free_full(list, reinterpret_cast<GDestroyNotify>(gtk_tree_path_free));
    } else {
        GtkTreeSelection* sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(data->tree_view));
        GList* list = gtk_tree_selection_get_selected_rows(sel, nullptr);
        for (GList* l = list; l != nullptr; l = l->next) {
            auto* path = static_cast<GtkTreePath*>(l->data);
            GtkTreeIter iter;
            if (gtk_tree_model_get_iter(GTK_TREE_MODEL(data->store), &iter, path)) {
                gchar* p = nullptr;
                gtk_tree_model_get(GTK_TREE_MODEL(data->store), &iter, COL_PATH, &p, -1);
                if (p) {
                    results.emplace_back(p);
                    g_free(p);
                }
            }
        }
        g_list_free_full(list, reinterpret_cast<GDestroyNotify>(gtk_tree_path_free));
    }

    return results;
}

void FileViewWidget::select_all(GtkWidget* widget) {
    auto* data = get_data(widget);
    if (!data) return;

    if (data->view_mode == ViewMode::GRID) {
        gtk_icon_view_select_all(GTK_ICON_VIEW(data->icon_view));
    } else {
        GtkTreeSelection* sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(data->tree_view));
        gtk_tree_selection_select_all(sel);
    }
}

void FileViewWidget::clear_selection(GtkWidget* widget) {
    auto* data = get_data(widget);
    if (!data) return;

    if (data->view_mode == ViewMode::GRID) {
        gtk_icon_view_unselect_all(GTK_ICON_VIEW(data->icon_view));
    } else {
        GtkTreeSelection* sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(data->tree_view));
        gtk_tree_selection_unselect_all(sel);
    }
}

void FileViewWidget::action_open_selected(GtkWidget* widget) {
    auto* data = get_data(widget);
    if (!data) return;

    auto paths = get_selected_paths(widget);
    if (paths.empty()) return;

    if (paths.size() == 1) {
        // Single item: if folder, navigate; else launch
        struct stat st;
        if (stat(paths[0].c_str(), &st) == 0 && S_ISDIR(st.st_mode)) {
            if (data->on_navigate) {
                data->on_navigate(paths[0]);
            }
            return;
        }
    }

    for (const auto& p : paths) {
        FileOperations::launch_file(p);
    }
}

void FileViewWidget::action_cut(GtkWidget* widget) {
    auto paths = get_selected_paths(widget);
    if (!paths.empty()) {
        FileOperations::copy_to_clipboard(paths, true);
    }
}

void FileViewWidget::action_copy(GtkWidget* widget) {
    auto paths = get_selected_paths(widget);
    if (!paths.empty()) {
        FileOperations::copy_to_clipboard(paths, false);
    }
}

void FileViewWidget::action_paste(GtkWidget* widget) {
    auto* data = get_data(widget);
    if (!data || data->current_path.empty()) return;

    // Use progress-aware paste — shows dialog for files > 1 MB, silent for small files
    GtkWindow* parent_win = GTK_WINDOW(gtk_widget_get_toplevel(widget));
    FileOperations::paste_from_clipboard_with_progress(
        data->current_path,
        GTK_IS_WINDOW(parent_win) ? parent_win : nullptr
    );
    refresh(widget);
}

void FileViewWidget::action_rename_selected(GtkWidget* widget) {
    auto* data = get_data(widget);
    if (!data) return;

    auto paths = get_selected_paths(widget);
    if (paths.size() != 1) return;

    const std::string& old_path = paths[0];
    size_t last_slash = old_path.find_last_of('/');
    std::string filename = (last_slash != std::string::npos) ? old_path.substr(last_slash + 1) : old_path;

    GtkWidget* toplevel = gtk_widget_get_toplevel(widget);
    GtkWidget* dialog = gtk_dialog_new_with_buttons(
        "Rename Item",
        GTK_IS_WINDOW(toplevel) ? GTK_WINDOW(toplevel) : nullptr,
        static_cast<GtkDialogFlags>(GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT),
        "_Cancel", GTK_RESPONSE_CANCEL,
        "_Rename", GTK_RESPONSE_ACCEPT,
        nullptr
    );
    gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_ACCEPT);
    gtk_widget_add_css_class(dialog, "zenith-files-dialog");

    GtkWidget* content = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
    gtk_container_set_border_width(GTK_CONTAINER(content), 16);
    gtk_box_set_spacing(GTK_BOX(content), 12);

    GtkWidget* lbl = gtk_label_new("Enter new name:");
    gtk_label_set_xalign(GTK_LABEL(lbl), 0.0f);
    gtk_box_pack_start(GTK_BOX(content), lbl, FALSE, FALSE, 0);

    GtkWidget* entry = gtk_entry_new();
    gtk_entry_set_text(GTK_ENTRY(entry), filename.c_str());
    gtk_entry_set_activates_default(GTK_ENTRY(entry), TRUE);
    gtk_box_pack_start(GTK_BOX(content), entry, FALSE, FALSE, 0);

    // Pre-select text before extension if file
    size_t dot = filename.find_last_of('.');
    if (dot != std::string::npos && dot > 0) {
        gtk_editable_select_region(GTK_EDITABLE(entry), 0, static_cast<gint>(dot));
    } else {
        gtk_editable_select_region(GTK_EDITABLE(entry), 0, -1);
    }

    gtk_widget_show_all(dialog);
    gint res = gtk_dialog_run(GTK_DIALOG(dialog));

    if (res == GTK_RESPONSE_ACCEPT) {
        const char* text = gtk_entry_get_text(GTK_ENTRY(entry));
        if (text && strlen(text) > 0 && filename != text) {
            FileOperations::rename_item(old_path, text);
            refresh(widget);
        }
    }

    gtk_widget_destroy(dialog);
}

void FileViewWidget::action_trash_selected(GtkWidget* widget) {
    auto paths = get_selected_paths(widget);
    if (!paths.empty()) {
        FileOperations::move_to_trash(paths);
        refresh(widget);
    }
}

void FileViewWidget::action_delete_selected(GtkWidget* widget) {
    auto paths = get_selected_paths(widget);
    if (paths.empty()) return;

    GtkWidget* toplevel = gtk_widget_get_toplevel(widget);
    std::string prompt = (paths.size() == 1)
        ? "Are you sure you want to permanently delete this item? This action cannot be undone."
        : "Are you sure you want to permanently delete " + std::to_string(paths.size()) + " items? This action cannot be undone.";

    GtkWidget* dialog = gtk_message_dialog_new(
        GTK_IS_WINDOW(toplevel) ? GTK_WINDOW(toplevel) : nullptr,
        static_cast<GtkDialogFlags>(GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT),
        GTK_MESSAGE_WARNING,
        GTK_BUTTONS_OK_CANCEL,
        "%s", prompt.c_str()
    );
    gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_CANCEL);
    gtk_widget_add_css_class(dialog, "zenith-files-dialog");

    gint res = gtk_dialog_run(GTK_DIALOG(dialog));
    if (res == GTK_RESPONSE_OK) {
        FileOperations::delete_permanently(paths);
        refresh(widget);
    }
    gtk_widget_destroy(dialog);
}

void FileViewWidget::action_new_folder(GtkWidget* widget) {
    auto* data = get_data(widget);
    if (!data || data->current_path.empty()) return;

    GtkWidget* toplevel = gtk_widget_get_toplevel(widget);
    GtkWidget* dialog = gtk_dialog_new_with_buttons(
        "Create New Folder",
        GTK_IS_WINDOW(toplevel) ? GTK_WINDOW(toplevel) : nullptr,
        static_cast<GtkDialogFlags>(GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT),
        "_Cancel", GTK_RESPONSE_CANCEL,
        "_Create", GTK_RESPONSE_ACCEPT,
        nullptr
    );
    gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_ACCEPT);
    gtk_widget_add_css_class(dialog, "zenith-files-dialog");

    GtkWidget* content = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
    gtk_container_set_border_width(GTK_CONTAINER(content), 16);
    gtk_box_set_spacing(GTK_BOX(content), 12);

    GtkWidget* lbl = gtk_label_new("Folder name:");
    gtk_label_set_xalign(GTK_LABEL(lbl), 0.0f);
    gtk_box_pack_start(GTK_BOX(content), lbl, FALSE, FALSE, 0);

    GtkWidget* entry = gtk_entry_new();
    gtk_entry_set_text(GTK_ENTRY(entry), "New Folder");
    gtk_entry_set_activates_default(GTK_ENTRY(entry), TRUE);
    gtk_editable_select_region(GTK_EDITABLE(entry), 0, -1);
    gtk_box_pack_start(GTK_BOX(content), entry, FALSE, FALSE, 0);

    gtk_widget_show_all(dialog);
    gint res = gtk_dialog_run(GTK_DIALOG(dialog));

    if (res == GTK_RESPONSE_ACCEPT) {
        const char* text = gtk_entry_get_text(GTK_ENTRY(entry));
        if (text && strlen(text) > 0) {
            FileOperations::create_folder(data->current_path, text);
            refresh(widget);
        }
    }

    gtk_widget_destroy(dialog);
}

void FileViewWidget::action_new_file(GtkWidget* widget) {
    auto* data = get_data(widget);
    if (!data || data->current_path.empty()) return;

    GtkWidget* toplevel = gtk_widget_get_toplevel(widget);
    GtkWidget* dialog = gtk_dialog_new_with_buttons(
        "Create New File",
        GTK_IS_WINDOW(toplevel) ? GTK_WINDOW(toplevel) : nullptr,
        static_cast<GtkDialogFlags>(GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT),
        "_Cancel", GTK_RESPONSE_CANCEL,
        "_Create", GTK_RESPONSE_ACCEPT,
        nullptr
    );
    gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_ACCEPT);
    gtk_widget_add_css_class(dialog, "zenith-files-dialog");

    GtkWidget* content = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
    gtk_container_set_border_width(GTK_CONTAINER(content), 16);
    gtk_box_set_spacing(GTK_BOX(content), 12);

    GtkWidget* lbl = gtk_label_new("File name:");
    gtk_label_set_xalign(GTK_LABEL(lbl), 0.0f);
    gtk_box_pack_start(GTK_BOX(content), lbl, FALSE, FALSE, 0);

    GtkWidget* entry = gtk_entry_new();
    gtk_entry_set_text(GTK_ENTRY(entry), "New Document.txt");
    gtk_entry_set_activates_default(GTK_ENTRY(entry), TRUE);
    gtk_editable_select_region(GTK_EDITABLE(entry), 0, 12);
    gtk_box_pack_start(GTK_BOX(content), entry, FALSE, FALSE, 0);

    gtk_widget_show_all(dialog);
    gint res = gtk_dialog_run(GTK_DIALOG(dialog));

    if (res == GTK_RESPONSE_ACCEPT) {
        const char* text = gtk_entry_get_text(GTK_ENTRY(entry));
        if (text && strlen(text) > 0) {
            FileOperations::create_file(data->current_path, text);
            refresh(widget);
        }
    }

    gtk_widget_destroy(dialog);
}

void FileViewWidget::action_open_terminal(GtkWidget* widget) {
    auto* data = get_data(widget);
    if (!data) return;

    auto paths = get_selected_paths(widget);
    if (!paths.empty()) {
        struct stat st;
        if (stat(paths[0].c_str(), &st) == 0 && S_ISDIR(st.st_mode)) {
            FileOperations::open_terminal(paths[0]);
            return;
        }
    }

    if (!data->current_path.empty()) {
        FileOperations::open_terminal(data->current_path);
    }
}

void FileViewWidget::action_properties(GtkWidget* widget) {
    auto* data = get_data(widget);
    if (!data) return;

    auto paths = get_selected_paths(widget);
    std::string target_path = paths.empty() ? data->current_path : paths[0];
    if (target_path.empty()) return;

    GFile* file = g_file_new_for_path(target_path.c_str());
    GFileInfo* info = g_file_query_info(
        file,
        "standard::*,time::*,access::*,unix::*",
        G_FILE_QUERY_INFO_NONE,
        nullptr,
        nullptr
    );

    if (!info) {
        g_object_unref(file);
        return;
    }

    auto item = FileItem::from_file_info(file, info, 64, 24);
    g_object_unref(info);
    g_object_unref(file);

    if (!item) return;

    GtkWidget* toplevel = gtk_widget_get_toplevel(widget);
    GtkWidget* dialog = gtk_dialog_new_with_buttons(
        ("Properties: " + item->name).c_str(),
        GTK_IS_WINDOW(toplevel) ? GTK_WINDOW(toplevel) : nullptr,
        static_cast<GtkDialogFlags>(GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT),
        "_Close", GTK_RESPONSE_CLOSE,
        nullptr
    );
    gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_CLOSE);
    gtk_widget_add_css_class(dialog, "zenith-files-dialog");

    GtkWidget* content = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
    gtk_container_set_border_width(GTK_CONTAINER(content), 20);
    gtk_box_set_spacing(GTK_BOX(content), 16);

    // Header with Large Icon + Name
    GtkWidget* header_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 16);
    if (item->pixbuf_large) {
        GtkWidget* icon_img = gtk_image_new_from_pixbuf(item->pixbuf_large);
        gtk_box_pack_start(GTK_BOX(header_box), icon_img, FALSE, FALSE, 0);
    }

    GtkWidget* name_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    GtkWidget* name_label = gtk_label_new(item->name.c_str());
    gtk_label_set_xalign(GTK_LABEL(name_label), 0.0f);
    gtk_widget_add_css_class(name_label, "files-prop-title");
    gtk_box_pack_start(GTK_BOX(name_box), name_label, FALSE, FALSE, 0);

    GtkWidget* type_label = gtk_label_new(item->mime_type.c_str());
    gtk_label_set_xalign(GTK_LABEL(type_label), 0.0f);
    gtk_widget_add_css_class(type_label, "files-prop-subtitle");
    gtk_box_pack_start(GTK_BOX(name_box), type_label, FALSE, FALSE, 0);

    gtk_box_pack_start(GTK_BOX(header_box), name_box, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(content), header_box, FALSE, FALSE, 0);

    gtk_box_pack_start(GTK_BOX(content), gtk_separator_new(GTK_ORIENTATION_HORIZONTAL), FALSE, FALSE, 0);

    // Details Grid
    GtkWidget* grid = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(grid), 8);
    gtk_grid_set_column_spacing(GTK_GRID(grid), 16);

    auto add_row = [grid](int row, const char* label_str, const std::string& val_str) {
        GtkWidget* l = gtk_label_new(label_str);
        gtk_label_set_xalign(GTK_LABEL(l), 0.0f);
        gtk_widget_add_css_class(l, "files-prop-key");
        gtk_grid_attach(GTK_GRID(grid), l, 0, row, 1, 1);

        GtkWidget* v = gtk_label_new(val_str.c_str());
        gtk_label_set_xalign(GTK_LABEL(v), 0.0f);
        gtk_label_set_selectable(GTK_LABEL(v), TRUE);
        gtk_widget_add_css_class(v, "files-prop-val");
        gtk_grid_attach(GTK_GRID(grid), v, 1, row, 1, 1);
    };

    int row = 0;
    add_row(row++, "Location:", item->path);
    add_row(row++, "Size:", item->formatted_size + " (" + std::to_string(item->size) + " bytes)");
    add_row(row++, "Modified:", item->formatted_date);

    gtk_box_pack_start(GTK_BOX(content), grid, TRUE, TRUE, 0);

    gtk_widget_show_all(dialog);
    gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);
}

} // namespace zenith
