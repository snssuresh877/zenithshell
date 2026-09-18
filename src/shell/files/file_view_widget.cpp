#include "shell/files/file_view_widget.hpp"
#include "shell/files/file_item.hpp"
#include "shell/files/file_operations.hpp"
#include "shell/files/bulk_rename_dialog.hpp"
#include "shell/files/checksum_dialog.hpp"
#include "shell/files/duplicate_finder_dialog.hpp"
#include "shell/files/disk_analyzer_dialog.hpp"
#include "shell/files/quick_preview.hpp"
#include "shell/files/places_sidebar.hpp"

#include "gtk3_compat.hpp"

#include <glib/gstdio.h>
#include <gdk/gdkkeysyms.h>
#include <algorithm>
#include <iostream>
#include <cstring>
#include <sys/stat.h>
#include <mutex>
#include <thread>
#include <filesystem>
#include <map>

namespace fs = std::filesystem;

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
    COL_MARKUP,
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
    int icon_size{96};
    bool show_hidden{false};
    std::string search_query;
    SortField sort_field{SortField::NAME};
    bool sort_ascending{true};

    GFileMonitor* file_monitor{nullptr};
    gulong monitor_handler_id{0};
    guint debounce_refresh_timer{0};

    FileViewWidget::NavigateCallback on_navigate;
    FileViewWidget::StatusCallback on_status;
    FileViewWidget::SelectionCallback on_selection;

    // ── Async thumbnail generation ────────────────────────────────────────────
    // Each call to load_directory() bumps this counter. Thumbnail idle callbacks
    // carry the generation at dispatch time and silently discard results from
    // stale generations (user navigated away before thumb finished).
    uint64_t thumb_generation{0};
    GThreadPool* thumb_pool{nullptr};  // created lazily, shared across navigations
    
    bool is_searching{false};
    uint64_t search_generation{0};

    GThreadPool* search_pool{nullptr};
    
    // Type-Ahead Search
    GtkWidget* typeahead_popover{nullptr};
    GtkWidget* typeahead_entry{nullptr};

};

static void file_view_data_free(gpointer user_data) {
    auto* data = static_cast<FileViewData*>(user_data);
    if (!data) return;

    // Bump generation so any in-flight thumbnail g_idle_add callbacks become no-ops
    data->thumb_generation++;

    if (data->thumb_pool) {
        // Don't wait — just mark exclusive so no new items are pushed after this.
        // Already-running jobs will complete but their idle callbacks will abort
        // because thumb_generation won't match.
        g_thread_pool_free(data->thumb_pool, TRUE, FALSE);
        data->thumb_pool = nullptr;
    }

    data->search_generation++;
    if (data->search_pool) {
        g_thread_pool_free(data->search_pool, TRUE, FALSE);
        data->search_pool = nullptr;
    }

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
        std::string markup = g_markup_escape_text(item->display_name.c_str(), -1);
        std::cout << "Markup: " << markup << "\n";
        
        if (!item->git_branch.empty()) {
            markup += " <span foreground='gray' size='small'>[" + item->git_branch + "]</span>";
        }
        
        if (!item->git_status.empty()) {
            std::string color = "gray";
            if (item->git_status == "M " || item->git_status == " M") color = "#E5A50A";
            else if (item->git_status == "A " || item->git_status == "??") color = "#2ea043";
            else if (item->git_status == "D " || item->git_status == " D") color = "#da3633";
            
            markup = "<span foreground='" + color + "'>" + markup + "</span>";
        }

        gtk_list_store_append(data->store, &iter);
        gtk_list_store_set(data->store, &iter,
            COL_PIXBUF_LARGE, item->pixbuf_large,
            COL_PIXBUF_SMALL, item->pixbuf_small,
            COL_NAME, item->display_name.c_str(),
            COL_MARKUP, markup.c_str(),
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

// ── Async thumbnail loading ───────────────────────────────────────────────
struct ThumbJob {
    FileViewData* view_data;
    GtkListStore* store;        // ref held during job lifetime
    std::string   path;
    std::string   uri;
    std::string   mime_type;
    std::string   item_path;    // used to find the row in the store
    int           size;
    uint64_t      generation;
};

static void dispatch_thumbnails(FileViewData* data, uint64_t my_generation) {
    if (!data || !data->store) return;

    // Lazy-create the thread pool (4 concurrent decoders max)
    if (!data->thumb_pool) {
        data->thumb_pool = g_thread_pool_new(
            +[](gpointer job_data, gpointer) {
                auto* job = static_cast<ThumbJob*>(job_data);
                GdkPixbuf* thumb = FileItem::load_thumbnail(
                    job->path, job->uri, job->mime_type, job->size);

                if (!thumb) { delete job; return; }

                // Pass result back to main thread via g_idle_add
                struct IdleCtx {
                    FileViewData* view_data;
                    GtkListStore* store;
                    GdkPixbuf*    thumb;
                    std::string   item_path;
                    int           size;
                    uint64_t      generation;
                };
                auto* ctx = new IdleCtx{job->view_data, job->store,
                                        thumb, job->item_path,
                                        job->size, job->generation};
                g_object_ref(job->store);

                g_idle_add(+[](gpointer p) -> gboolean {
                    auto* c = static_cast<IdleCtx*>(p);

                    // Discard if user navigated away
                    if (c->generation != c->view_data->thumb_generation ||
                        !GTK_IS_LIST_STORE(c->store)) {
                        g_object_unref(c->store);
                        g_object_unref(c->thumb);
                        delete c;
                        return G_SOURCE_REMOVE;
                    }

                    // Find the matching row by path and update pixbuf columns
                    GtkTreeModel* model = GTK_TREE_MODEL(c->store);
                    GtkTreeIter iter;
                    if (gtk_tree_model_get_iter_first(model, &iter)) {
                        do {
                            gchar* row_path = nullptr;
                            gtk_tree_model_get(model, &iter, COL_PATH, &row_path, -1);
                            if (row_path && c->item_path == row_path) {
                                int pw = gdk_pixbuf_get_width(c->thumb);
                                int ph = gdk_pixbuf_get_height(c->thumb);
                                double scale = std::min(20.0 / pw, 20.0 / ph);
                                int sw = std::max(1, static_cast<int>(pw * scale));
                                int sh = std::max(1, static_cast<int>(ph * scale));
                                GdkPixbuf* sm = gdk_pixbuf_scale_simple(
                                    c->thumb, sw, sh, GDK_INTERP_BILINEAR);

                                gtk_list_store_set(c->store, &iter,
                                    COL_PIXBUF_LARGE, c->thumb,
                                    COL_PIXBUF_SMALL, sm ? sm : c->thumb,
                                    -1);
                                if (sm) g_object_unref(sm);
                                g_free(row_path);
                                break;
                            }
                            if (row_path) g_free(row_path);
                        } while (gtk_tree_model_iter_next(model, &iter));
                    }

                    g_object_unref(c->store);
                    g_object_unref(c->thumb);
                    delete c;
                    return G_SOURCE_REMOVE;
                }, ctx);

                delete job;
            },
            nullptr,
            4,      // max 4 concurrent decoder threads
            FALSE,  // not exclusive
            nullptr
        );
    }

    // Dispatch one thumbnail job per image/video file
    for (const auto& item : data->filtered_items) {
        if (item->is_directory) continue;
        bool needs_thumb = (item->mime_type.rfind("image/", 0) == 0 ||
                            item->mime_type.rfind("video/", 0) == 0);
        if (!needs_thumb) continue;

        auto* job = new ThumbJob{
            data,
            data->store,
            item->path,
            item->uri,
            item->mime_type,
            item->path,
            48,
            my_generation
        };
        g_thread_pool_push(data->thumb_pool, job, nullptr);
    }
}

// ── Drag and Drop handlers ──────────────────────────────────────────────────
static void on_drag_data_get(GtkWidget*, GdkDragContext*, GtkSelectionData* selection_data, guint, guint, gpointer user_data) {
    auto* data = static_cast<FileViewData*>(user_data);
    if (!data) return;
    auto paths = FileViewWidget::get_selected_paths(data->root_box);
    if (paths.empty()) return;

    std::string uris;
    for (const auto& p : paths) {
        GFile* f = g_file_parse_name(p.c_str());
        if (f) {
            char* u = g_file_get_uri(f);
            if (u) {
                uris += u;
                uris += "\r\n";
                g_free(u);
            }
            g_object_unref(f);
        }
    }
    GdkAtom target = gdk_atom_intern_static_string("text/uri-list");
    gtk_selection_data_set(selection_data, target, 8, (const guchar*)uris.c_str(), uris.length());
}

static void on_drag_data_received(GtkWidget*, GdkDragContext* context, gint, gint, GtkSelectionData* selection_data, guint, guint time, gpointer user_data) {
    auto* data = static_cast<FileViewData*>(user_data);
    if (!data || data->current_path.empty()) {
        gtk_drag_finish(context, FALSE, FALSE, time);
        return;
    }

    gchar** uris = gtk_selection_data_get_uris(selection_data);
    if (!uris) {
        gtk_drag_finish(context, FALSE, FALSE, time);
        return;
    }

    GdkDragAction action = gdk_drag_context_get_selected_action(context);
    bool is_move = (action == GDK_ACTION_MOVE);

    std::vector<std::string> src_paths;
    for (int i = 0; uris[i] != nullptr; ++i) {
        GFile* f = g_file_new_for_uri(uris[i]);
        if (f) {
            char* p = g_file_get_parse_name(f);
            if (p) {
                src_paths.push_back(p);
                g_free(p);
            }
            g_object_unref(f);
        }
    }
    g_strfreev(uris);

    if (!src_paths.empty()) {
        FileOperations::copy_to_clipboard(src_paths, is_move);
        GtkWindow* parent_win = GTK_WINDOW(gtk_widget_get_toplevel(data->root_box));
        FileOperations::paste_from_clipboard_with_progress(
            data->current_path,
            GTK_IS_WINDOW(parent_win) ? parent_win : nullptr
        );
        FileViewWidget::refresh(data->root_box);
        gtk_drag_finish(context, TRUE, is_move, time);
    } else {
        gtk_drag_finish(context, FALSE, FALSE, time);
    }
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

    // Filter uninteresting events (ignoring _CHANGED which fires thousands of times during downloads)
    if (event_type == G_FILE_MONITOR_EVENT_CHANGES_DONE_HINT ||
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

    GFile* gfile = g_file_parse_name(path.c_str());
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
    if (data && data->on_selection) {
        data->on_selection(FileViewWidget::get_selected_paths(data->root_box));
    }
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

        // AI Actions (Single File)
        if (selected.size() == 1 && fs::is_regular_file(selected[0])) {
            GtkWidget* ai_item = gtk_menu_item_new_with_label("✨ AI Analysis");
            GtkWidget* ai_menu = gtk_menu_new();
            gtk_menu_item_set_submenu(GTK_MENU_ITEM(ai_item), ai_menu);
            
            struct AiAction { const char* label; const char* action; };
            std::vector<AiAction> actions = {
                {"Extract Text", "extract_text"},
                {"Summarize Document", "summary"},
                {"Extract Tables", "tables"},
                {"Extract Invoice Info", "invoice"},
                {"Identify Product (Image)", "product"},
                {"Generate Tags", "tags"},
                {"Generate Keywords", "keywords"}
            };
            
            for (const auto& act : actions) {
                GtkWidget* sub_item = gtk_menu_item_new_with_label(act.label);
                
                // Pack data
                std::pair<std::string, std::string>* data_pair = new std::pair<std::string, std::string>(act.action, selected[0]);
                g_object_set_data_full(G_OBJECT(sub_item), "ai_data", data_pair, [](gpointer p) {
                    delete static_cast<std::pair<std::string, std::string>*>(p);
                });
                
                auto cb = +[](GtkMenuItem* mi, gpointer) {
                    auto* p = static_cast<std::pair<std::string, std::string>*>(g_object_get_data(G_OBJECT(mi), "ai_data"));
                    if (p) {
                        std::string cmd = "~/.local/share/zenithshell/zenith_ai.py " + p->first + " \"" + p->second + "\" &";
                        int ret = system(cmd.c_str());
                        (void)ret;
                    }
                };
                g_signal_connect(sub_item, "activate", G_CALLBACK(cb), nullptr);
                
                gtk_menu_shell_append(GTK_MENU_SHELL(ai_menu), sub_item);
            }
            gtk_menu_shell_append(GTK_MENU_SHELL(menu), ai_item);
            
            GtkWidget* sep = gtk_separator_menu_item_new();
            gtk_menu_shell_append(GTK_MENU_SHELL(menu), sep);
        }

        // "Add / Remove Favorite" for single directory
        if (selected.size() == 1 && fs::is_directory(selected[0])) {
            bool is_fav = PlacesSidebar::is_favorite(selected[0]);
            GtkWidget* item_fav = gtk_menu_item_new_with_label(is_fav ? "★ Remove from Favorites" : "★ Add to Favorites");
            std::string* p_fav = new std::string(selected[0]);
            g_object_set_data_full(G_OBJECT(item_fav), "fav_path", p_fav, [](gpointer p) { delete static_cast<std::string*>(p); });
            g_signal_connect(item_fav, "activate", G_CALLBACK(+[](GtkMenuItem* mi, gpointer) {
                auto* p = static_cast<std::string*>(g_object_get_data(G_OBJECT(mi), "fav_path"));
                if (p) {
                    if (PlacesSidebar::is_favorite(*p)) {
                        PlacesSidebar::remove_favorite(*p);
                    } else {
                        PlacesSidebar::add_favorite(*p);
                    }
                }
            }), nullptr);
            gtk_menu_shell_append(GTK_MENU_SHELL(menu), item_fav);
        }

        // "Quick Preview" for single file
        if (selected.size() == 1) {
            GtkWidget* item_qp = gtk_menu_item_new_with_label("Quick Preview (Space)");
            std::string* p_qp = new std::string(selected[0]);
            g_object_set_data_full(G_OBJECT(item_qp), "qp_path", p_qp, [](gpointer p) { delete static_cast<std::string*>(p); });
            g_signal_connect(item_qp, "activate", G_CALLBACK(+[](GtkMenuItem* mi, gpointer) {
                auto* p = static_cast<std::string*>(g_object_get_data(G_OBJECT(mi), "qp_path"));
                if (p) QuickPreview::toggle(nullptr, *p);
            }), nullptr);
            gtk_menu_shell_append(GTK_MENU_SHELL(menu), item_qp);
        }

        // "Open With…" only for single file selection
        if (selected.size() == 1) {
            struct stat st;
            if (!(stat(selected[0].c_str(), &st) == 0 && S_ISDIR(st.st_mode))) {
                GtkWidget* item_open_with = gtk_menu_item_new_with_label("Open With…");
                std::string* sel_path = new std::string(selected[0]);
                g_object_set_data_full(G_OBJECT(item_open_with), "item_path", sel_path, [](gpointer p) { delete static_cast<std::string*>(p); });
                g_signal_connect(item_open_with, "activate", G_CALLBACK(+[](GtkMenuItem* mi, gpointer udata) {
                    auto* p = static_cast<std::string*>(g_object_get_data(G_OBJECT(mi), "item_path"));
                    GtkWidget* widget = GTK_WIDGET(udata);
                    GtkWidget* toplevel = gtk_widget_get_toplevel(widget);
                    GtkWindow* parent_win = GTK_IS_WINDOW(toplevel) ? GTK_WINDOW(toplevel) : nullptr;
                    if (p) FileOperations::open_with_dialog(*p, parent_win);
                }), data->root_box);
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
            GFile* gf = g_file_parse_name(sp.c_str());
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

        // ── Share via QR (Local Network) ──────────────────────────────────────
        if (selected.size() == 1) {
            struct stat st;
            if (stat(selected[0].c_str(), &st) == 0 && !S_ISDIR(st.st_mode)) {
                GtkWidget* item_share = gtk_menu_item_new_with_label("Share via QR (Wi-Fi)");
                std::string* sel_path = new std::string(selected[0]);
                g_object_set_data_full(G_OBJECT(item_share), "item_path", sel_path, [](gpointer p) { delete static_cast<std::string*>(p); });
                g_signal_connect(item_share, "activate", G_CALLBACK(+[](GtkMenuItem* mi, gpointer udata) {
                    auto* p = static_cast<std::string*>(g_object_get_data(G_OBJECT(mi), "item_path"));
                    GtkWidget* widget = GTK_WIDGET(udata);
                    GtkWidget* toplevel = gtk_widget_get_toplevel(widget);
                    GtkWindow* parent_win = GTK_IS_WINDOW(toplevel) ? GTK_WINDOW(toplevel) : nullptr;
                    if (p) FileOperations::share_via_qr(*p, parent_win);
                }), data->root_box);
                gtk_menu_shell_append(GTK_MENU_SHELL(menu), item_share);
            }
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

        GtkWidget* item_copypath = gtk_menu_item_new_with_label("Copy Path (Ctrl+Shift+C)");
        g_signal_connect_swapped(item_copypath, "activate", G_CALLBACK(FileViewWidget::action_copy_path), data->root_box);
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), item_copypath);

        // "Set as Wallpaper" for single image selection
        if (selected.size() == 1) {
            struct stat st;
            if (stat(selected[0].c_str(), &st) == 0 && !S_ISDIR(st.st_mode)) {
                GFile* gf = g_file_parse_name(selected[0].c_str());
                GFileInfo* fi = g_file_query_info(gf, "standard::content-type", G_FILE_QUERY_INFO_NONE, nullptr, nullptr);
                if (fi) {
                    const char* ct = g_file_info_get_content_type(fi);
                    if (ct && g_str_has_prefix(ct, "image/")) {
                        GtkWidget* item_wp = gtk_menu_item_new_with_label("Set as Wallpaper");
                        std::string* wp_path = new std::string(selected[0]);
                        g_object_set_data_full(G_OBJECT(item_wp), "wp_path", wp_path, [](gpointer p) { delete static_cast<std::string*>(p); });
                        g_signal_connect(item_wp, "activate", G_CALLBACK(+[](GtkMenuItem* mi, gpointer) {
                            auto* p = static_cast<std::string*>(g_object_get_data(G_OBJECT(mi), "wp_path"));
                            // if (p) ThemeEngine::set_wallpaper(*p);
                        }), nullptr);
                        gtk_menu_shell_append(GTK_MENU_SHELL(menu), item_wp);
                    }
                    g_object_unref(fi);
                }
                g_object_unref(gf);
            }
        }

        gtk_menu_shell_append(GTK_MENU_SHELL(menu), gtk_separator_menu_item_new());

        // ── Rename / Delete ───────────────────────────────────────────────────
        GtkWidget* item_rename = gtk_menu_item_new_with_label(selected.size() == 1 ? "Rename (F2)" : "Bulk Rename (F2)");
        g_signal_connect_swapped(item_rename, "activate", G_CALLBACK(FileViewWidget::action_rename_selected), data->root_box);
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), item_rename);

        GtkWidget* item_trash = gtk_menu_item_new_with_label("Move to Trash (Del)");
        g_signal_connect_swapped(item_trash, "activate", G_CALLBACK(FileViewWidget::action_trash_selected), data->root_box);
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), item_trash);

        if (data->current_path == "trash:///") {
            GtkWidget* item_restore = gtk_menu_item_new_with_label("Restore from Trash");
            g_signal_connect_swapped(item_restore, "activate", G_CALLBACK(FileViewWidget::action_restore_selected), data->root_box);
            gtk_menu_shell_append(GTK_MENU_SHELL(menu), item_restore);
        }
        GtkWidget* item_delete = gtk_menu_item_new_with_label("Delete Permanently (Shift+Del)");
        g_signal_connect_swapped(item_delete, "activate", G_CALLBACK(FileViewWidget::action_delete_selected), data->root_box);
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), item_delete);

        gtk_menu_shell_append(GTK_MENU_SHELL(menu), gtk_separator_menu_item_new());



        GtkWidget* item_dup = gtk_menu_item_new_with_label("Find Duplicates...");
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), item_dup);
        g_signal_connect(item_dup, "activate", G_CALLBACK(+[](GtkMenuItem* mi, gpointer) {
            auto* p = static_cast<std::string*>(g_object_get_data(G_OBJECT(mi), "wp_path"));
            GtkWidget* win = gtk_widget_get_toplevel(GTK_WIDGET(mi));
            if (p && GTK_IS_WINDOW(win)) zenith::DuplicateFinderDialog::show(GTK_WINDOW(win), *p);
        }), nullptr);
        
        // Hide if multiple paths or if it's NOT a directory
        struct stat st_dup;
        if (selected.size() != 1 || stat(selected[0].c_str(), &st_dup) != 0 || !S_ISDIR(st_dup.st_mode)) {
            gtk_widget_hide(item_dup);
        } else {
            g_object_set_data_full(G_OBJECT(item_dup), "wp_path", new std::string(selected[0]), [](gpointer data) { delete static_cast<std::string*>(data); });
        }


        GtkWidget* item_analyzer = gtk_menu_item_new_with_label("Analyze Disk Usage...");
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), item_analyzer);
        g_signal_connect(item_analyzer, "activate", G_CALLBACK(+[](GtkMenuItem* mi, gpointer) {
            auto* p = static_cast<std::string*>(g_object_get_data(G_OBJECT(mi), "wp_path"));
            GtkWidget* win = gtk_widget_get_toplevel(GTK_WIDGET(mi));
            if (p && GTK_IS_WINDOW(win)) zenith::DiskAnalyzerDialog::show(GTK_WINDOW(win), *p);
        }), nullptr);
        
        // Hide if multiple paths or if it's NOT a directory
        struct stat st_analyzer;
        if (selected.size() != 1 || stat(selected[0].c_str(), &st_analyzer) != 0 || !S_ISDIR(st_analyzer.st_mode)) {
            gtk_widget_hide(item_analyzer);
        } else {
            g_object_set_data_full(G_OBJECT(item_analyzer), "wp_path", new std::string(selected[0]), [](gpointer data) { delete static_cast<std::string*>(data); });
        }

        GtkWidget* item_checksum = gtk_menu_item_new_with_label("Verify Checksum...");
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), item_checksum);
        g_signal_connect(item_checksum, "activate", G_CALLBACK(+[](GtkMenuItem* mi, gpointer) {
            auto* p = static_cast<std::string*>(g_object_get_data(G_OBJECT(mi), "wp_path"));
            GtkWidget* win = gtk_widget_get_toplevel(GTK_WIDGET(mi));
            if (p && GTK_IS_WINDOW(win)) zenith::ChecksumDialog::show(GTK_WINDOW(win), *p);
        }), nullptr);
        
        // Hide if multiple paths or if it's a directory
        struct stat st;
        if (selected.size() != 1 || stat(selected[0].c_str(), &st) != 0 || S_ISDIR(st.st_mode)) {
            gtk_widget_hide(item_checksum);
        } else {
            g_object_set_data_full(G_OBJECT(item_checksum), "wp_path", new std::string(selected[0]), [](gpointer data) { delete static_cast<std::string*>(data); });
        }

        GtkWidget* item_prop = gtk_menu_item_new_with_label("Properties");
        g_signal_connect_swapped(item_prop, "activate", G_CALLBACK(FileViewWidget::action_properties), data->root_box);
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), item_prop);
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), gtk_separator_menu_item_new());
        

        GtkWidget* item_analyzer_empty = gtk_menu_item_new_with_label("Analyze Disk Usage Here...");
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), item_analyzer_empty);
        g_signal_connect(item_analyzer_empty, "activate", G_CALLBACK(+[](GtkMenuItem* mi, gpointer udata) {
            auto* d = static_cast<FileViewData*>(udata);
            GtkWidget* win = gtk_widget_get_toplevel(d->root_box);
            if (GTK_IS_WINDOW(win)) zenith::DiskAnalyzerDialog::show(GTK_WINDOW(win), d->current_path);
        }), data);

        GtkWidget* item_dup_empty = gtk_menu_item_new_with_label("Find Duplicates Here...");
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), item_dup_empty);
        g_signal_connect(item_dup_empty, "activate", G_CALLBACK(+[](GtkMenuItem* mi, gpointer udata) {
            auto* d = static_cast<FileViewData*>(udata);
            GtkWidget* win = gtk_widget_get_toplevel(d->root_box);
            if (GTK_IS_WINDOW(win)) zenith::DuplicateFinderDialog::show(GTK_WINDOW(win), d->current_path);
        }), data);

    } else {
        // Empty space menu
        GtkWidget* item_folder = gtk_menu_item_new_with_label("New Folder (Ctrl+Shift+N)");
        g_signal_connect_swapped(item_folder, "activate", G_CALLBACK(FileViewWidget::action_new_folder), data->root_box);
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), item_folder);

        GtkWidget* item_file = gtk_menu_item_new_with_label("New Document");
        g_signal_connect_swapped(item_file, "activate", G_CALLBACK(FileViewWidget::action_new_file), data->root_box);
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), item_file);

        GtkWidget* item_recv = gtk_menu_item_new_with_label("Receive Files (Wi-Fi)");
        g_signal_connect(item_recv, "activate", G_CALLBACK(+[](GtkMenuItem*, gpointer udata) {
            auto* d = get_data(GTK_WIDGET(udata));
            if (!d || d->current_path.empty()) return;
            GtkWidget* toplevel = gtk_widget_get_toplevel(GTK_WIDGET(udata));
            GtkWindow* parent_win = GTK_IS_WINDOW(toplevel) ? GTK_WINDOW(toplevel) : nullptr;
            FileOperations::receive_via_qr(d->current_path, parent_win);
            FileViewWidget::refresh(GTK_WIDGET(udata));
        }), data->root_box);
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), item_recv);
        


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

static gboolean on_key_press(GtkWidget* widget, GdkEventKey* event, gpointer user_data) {
    auto* data = static_cast<FileViewData*>(user_data);
    if (!data) return FALSE;

    guint key = event->keyval;
    guint state = event->state & gtk_accelerator_get_default_mod_mask();

    // Type-Ahead Search intercept
    if (state == 0 || state == GDK_SHIFT_MASK) {
        guint32 unicode = gdk_keyval_to_unicode(key);
        if (unicode >= 32 && unicode != 127 && key != GDK_KEY_Escape && key != GDK_KEY_Return && key != GDK_KEY_KP_Enter && key != GDK_KEY_Tab) {
            if (!gtk_widget_get_visible(data->typeahead_popover)) {
                gtk_entry_set_text(GTK_ENTRY(data->typeahead_entry), "");
                gtk_popover_popup(GTK_POPOVER(data->typeahead_popover));
            }
            gtk_widget_grab_focus(data->typeahead_entry);
            // Forward event to entry so it inputs the character
            gtk_search_entry_handle_event(GTK_SEARCH_ENTRY(data->typeahead_entry), (GdkEvent*)event);
            return TRUE;
        }
    }

    if (state == 0) {
        if (key == GDK_KEY_Return || key == GDK_KEY_KP_Enter) {
            FileViewWidget::action_open_selected(data->root_box);
            return TRUE;
        } else if (key == GDK_KEY_BackSpace) {
            if (data->current_path != "/" && data->on_navigate) {
                GFile* current = g_file_parse_name(data->current_path.c_str());
                GFile* parent = g_file_get_parent(current);
                if (parent) {
                    char* parent_path = g_file_get_parse_name(parent);
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
        } else if (key == GDK_KEY_Left || key == GDK_KEY_Right) {
            if (data->view_mode == ViewMode::GRID) {
                GList* selected = gtk_icon_view_get_selected_items(GTK_ICON_VIEW(data->icon_view));
                if (selected) {
                    GtkTreePath* path = (GtkTreePath*)selected->data;
                    int idx = gtk_tree_path_get_indices(path)[0];
                    GtkTreeModel* model = gtk_icon_view_get_model(GTK_ICON_VIEW(data->icon_view));
                    int max_items = gtk_tree_model_iter_n_children(model, nullptr);
                    
                    if (key == GDK_KEY_Left && idx > 0) {
                        idx--;
                    } else if (key == GDK_KEY_Right && idx < max_items - 1) {
                        idx++;
                    } else {
                        g_list_free_full(selected, (GDestroyNotify)gtk_tree_path_free);
                        return FALSE; // Let default handle if out of bounds (though it won't do much)
                    }
                    
                    GtkTreePath* new_path = gtk_tree_path_new_from_indices(idx, -1);
                    gtk_icon_view_unselect_all(GTK_ICON_VIEW(data->icon_view));
                    gtk_icon_view_select_path(GTK_ICON_VIEW(data->icon_view), new_path);
                    gtk_icon_view_set_cursor(GTK_ICON_VIEW(data->icon_view), new_path, nullptr, FALSE);
                    gtk_icon_view_scroll_to_path(GTK_ICON_VIEW(data->icon_view), new_path, FALSE, 0.0, 0.0);
                    gtk_tree_path_free(new_path);
                    g_list_free_full(selected, (GDestroyNotify)gtk_tree_path_free);
                    return TRUE;
                } else {
                    // If nothing selected, select first item on Right, last on Left? 
                    // Let's just select the first item on any arrow key
                    GtkTreeModel* model = gtk_icon_view_get_model(GTK_ICON_VIEW(data->icon_view));
                    if (gtk_tree_model_iter_n_children(model, nullptr) > 0) {
                        GtkTreePath* new_path = gtk_tree_path_new_from_indices(0, -1);
                        gtk_icon_view_select_path(GTK_ICON_VIEW(data->icon_view), new_path);
                        gtk_icon_view_set_cursor(GTK_ICON_VIEW(data->icon_view), new_path, nullptr, FALSE);
                        gtk_icon_view_scroll_to_path(GTK_ICON_VIEW(data->icon_view), new_path, FALSE, 0.0, 0.0);
                        gtk_tree_path_free(new_path);
                        return TRUE;
                    }
                }
            }
        } else if (key == GDK_KEY_space) {
            auto paths = FileViewWidget::get_selected_paths(data->root_box);
            if (!paths.empty()) {
                GtkWindow* win = GTK_WINDOW(gtk_widget_get_toplevel(data->root_box));
                QuickPreview::toggle(win, paths[0]);
                return TRUE;
            }
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
        } else if (key == GDK_KEY_c || key == GDK_KEY_C) {
            FileViewWidget::action_copy_path(data->root_box);
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

GtkWidget* FileViewWidget::create(NavigateCallback on_navigate, StatusCallback on_status, SelectionCallback on_selection) {
    auto* data = new FileViewData();
    data->on_navigate = std::move(on_navigate);
    data->on_status = std::move(on_status);
    data->on_selection = std::move(on_selection);

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
        G_TYPE_POINTER,     // COL_ITEM_PTR
        G_TYPE_STRING       // COL_MARKUP
    );

    // 1. Grid View (GtkIconView)
    data->grid_scrolled = gtk_scrolled_window_new(nullptr, nullptr);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(data->grid_scrolled),
                                   GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

    data->icon_view = gtk_icon_view_new_with_model(GTK_TREE_MODEL(data->store));
    gtk_icon_view_set_selection_mode(GTK_ICON_VIEW(data->icon_view), GTK_SELECTION_MULTIPLE);
    gtk_icon_view_set_item_width(GTK_ICON_VIEW(data->icon_view), 96);
    
    gtk_cell_layout_clear(GTK_CELL_LAYOUT(data->icon_view));
    
    GtkCellRenderer* pix_ren = gtk_cell_renderer_pixbuf_new();
    gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(data->icon_view), pix_ren, FALSE);
    gtk_cell_layout_add_attribute(GTK_CELL_LAYOUT(data->icon_view), pix_ren, "pixbuf", COL_PIXBUF_LARGE);
    g_object_set(pix_ren, "xalign", 0.5, "yalign", 1.0, nullptr);

    GtkCellRenderer* txt_ren = gtk_cell_renderer_text_new();
    gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(data->icon_view), txt_ren, TRUE);
    gtk_cell_layout_add_attribute(GTK_CELL_LAYOUT(data->icon_view), txt_ren, "markup", COL_MARKUP);
    g_object_set(txt_ren, "alignment", PANGO_ALIGN_CENTER, "wrap-mode", PANGO_WRAP_WORD_CHAR, "wrap-width", 90, "xalign", 0.5, "yalign", 0.0, nullptr);
    gtk_icon_view_set_row_spacing(GTK_ICON_VIEW(data->icon_view), 12);
    gtk_icon_view_set_column_spacing(GTK_ICON_VIEW(data->icon_view), 12);
    gtk_icon_view_set_margin(GTK_ICON_VIEW(data->icon_view), 16);
    gtk_widget_add_css_class(data->icon_view, "files-icon-view");

    g_signal_connect(data->icon_view, "item-activated", G_CALLBACK(on_icon_activated), data);
    g_signal_connect(data->icon_view, "selection-changed", G_CALLBACK(on_selection_changed), data);
    g_signal_connect(data->icon_view, "button-press-event", G_CALLBACK(on_icon_button_press), data);
    g_signal_connect(data->icon_view, "key-press-event", G_CALLBACK(on_key_press), data);

    // DND configuration
    static const GtkTargetEntry dnd_targets[] = {
        {(gchar*)"text/uri-list", 0, 0}
    };
    gtk_drag_source_set(data->icon_view, GDK_BUTTON1_MASK, dnd_targets, 1, static_cast<GdkDragAction>(GDK_ACTION_COPY | GDK_ACTION_MOVE));
    g_signal_connect(data->icon_view, "drag-data-get", G_CALLBACK(on_drag_data_get), data);
    gtk_drag_dest_set(data->icon_view, GTK_DEST_DEFAULT_ALL, dnd_targets, 1, static_cast<GdkDragAction>(GDK_ACTION_COPY | GDK_ACTION_MOVE));
    g_signal_connect(data->icon_view, "drag-data-received", G_CALLBACK(on_drag_data_received), data);

    gtk_container_add(GTK_CONTAINER(data->grid_scrolled), data->icon_view);

    g_signal_connect(data->grid_scrolled, "scroll-event", G_CALLBACK(+[](GtkWidget*, GdkEventScroll* event, gpointer user_data) -> gboolean {
        if (event->state & GDK_CONTROL_MASK) {
            auto* d = static_cast<FileViewData*>(user_data);
            if (event->direction == GDK_SCROLL_UP || (event->direction == GDK_SCROLL_SMOOTH && event->delta_y < 0)) {
                FileViewWidget::zoom_in(d->root_box);
                return TRUE;
            } else if (event->direction == GDK_SCROLL_DOWN || (event->direction == GDK_SCROLL_SMOOTH && event->delta_y > 0)) {
                FileViewWidget::zoom_out(d->root_box);
                return TRUE;
            }
        }
        return FALSE;
    }), data);

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
        gtk_tree_view_column_add_attribute(col, txt_rend, "markup", COL_MARKUP);

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

    gtk_drag_source_set(data->tree_view, GDK_BUTTON1_MASK, dnd_targets, 1, static_cast<GdkDragAction>(GDK_ACTION_COPY | GDK_ACTION_MOVE));
    g_signal_connect(data->tree_view, "drag-data-get", G_CALLBACK(on_drag_data_get), data);
    gtk_drag_dest_set(data->tree_view, GTK_DEST_DEFAULT_ALL, dnd_targets, 1, static_cast<GdkDragAction>(GDK_ACTION_COPY | GDK_ACTION_MOVE));
    g_signal_connect(data->tree_view, "drag-data-received", G_CALLBACK(on_drag_data_received), data);



    gtk_container_add(GTK_CONTAINER(data->list_scrolled), data->tree_view);
    gtk_stack_add_named(GTK_STACK(data->stack), data->list_scrolled, "list");

    gtk_stack_set_visible_child_name(GTK_STACK(data->stack), "grid");


    // ── Type-Ahead Search Popover ──────────────────────────────────────────────
    data->typeahead_popover = gtk_popover_new(data->root_box);
    gtk_popover_set_position(GTK_POPOVER(data->typeahead_popover), GTK_POS_BOTTOM);
    gtk_widget_set_halign(data->typeahead_popover, GTK_ALIGN_END);
    gtk_widget_set_valign(data->typeahead_popover, GTK_ALIGN_END);
    
    data->typeahead_entry = gtk_search_entry_new();
    gtk_widget_set_margin_start(data->typeahead_entry, 6);
    gtk_widget_set_margin_end(data->typeahead_entry, 6);
    gtk_widget_set_margin_top(data->typeahead_entry, 6);
    gtk_widget_set_margin_bottom(data->typeahead_entry, 6);
    gtk_entry_set_width_chars(GTK_ENTRY(data->typeahead_entry), 25);
    gtk_container_add(GTK_CONTAINER(data->typeahead_popover), data->typeahead_entry);
    gtk_widget_show(data->typeahead_entry);

    g_signal_connect(data->typeahead_entry, "search-changed", G_CALLBACK(+[](GtkSearchEntry* entry, gpointer user_data) {
        auto* d = static_cast<FileViewData*>(user_data);
        const char* text = gtk_entry_get_text(GTK_ENTRY(entry));
        if (!text || !*text) return;
        
        std::string lower_query = g_utf8_strdown(text, -1);
        
        GtkTreeIter iter;
        gboolean valid = gtk_tree_model_get_iter_first(GTK_TREE_MODEL(d->store), &iter);
        while (valid) {
            gchar* name = nullptr;
            gtk_tree_model_get(GTK_TREE_MODEL(d->store), &iter, COL_NAME, &name, -1);
            if (name) {
                std::string lower_name = g_utf8_strdown(name, -1);
                g_free(name);
                if (lower_name.find(lower_query) == 0) {
                    GtkTreePath* path = gtk_tree_model_get_path(GTK_TREE_MODEL(d->store), &iter);
                    
                    if (d->view_mode == ViewMode::GRID) {
                        gtk_icon_view_select_path(GTK_ICON_VIEW(d->icon_view), path);
                        gtk_icon_view_scroll_to_path(GTK_ICON_VIEW(d->icon_view), path, FALSE, 0.5, 0.5);
                    } else {
                        GtkTreeSelection* sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(d->tree_view));
                        gtk_tree_selection_select_path(sel, path);
                        gtk_tree_view_scroll_to_cell(GTK_TREE_VIEW(d->tree_view), path, nullptr, FALSE, 0.5, 0.5);
                    }
                    gtk_tree_path_free(path);
                    break;
                }
            }
            valid = gtk_tree_model_iter_next(GTK_TREE_MODEL(d->store), &iter);
        }
    }), data);
    
    g_signal_connect(data->typeahead_entry, "activate", G_CALLBACK(+[](GtkEntry*, gpointer user_data) {
        auto* d = static_cast<FileViewData*>(user_data);
        gtk_popover_popdown(GTK_POPOVER(d->typeahead_popover));
        if (d->view_mode == ViewMode::GRID) gtk_widget_grab_focus(d->icon_view);
        else gtk_widget_grab_focus(d->tree_view);
        FileViewWidget::action_open_selected(d->root_box);
    }), data);

    g_signal_connect(data->typeahead_popover, "closed", G_CALLBACK(+[](GtkPopover*, gpointer user_data) {
        auto* d = static_cast<FileViewData*>(user_data);
        gtk_entry_set_text(GTK_ENTRY(d->typeahead_entry), "");
    }), data);

    g_object_set_data_full(G_OBJECT(data->root_box), "file_view_data", data, file_view_data_free);
    return data->root_box;
}

void FileViewWidget::load_directory(GtkWidget* widget, const std::string& path) {
    auto* data = get_data(widget);
    if (!data) return;

    if (data->is_searching) return;

    // ── Invalidate any in-flight thumbnails from a previous directory ─────────
    data->thumb_generation++;
    uint64_t my_generation = data->thumb_generation;

    data->current_path = path;
    data->all_items.clear();
    repopulate_store(data); // Clear the view immediately

    struct DirLoadTask {
        GtkWidget* widget;
        std::string path;
        uint64_t gen;
        std::vector<std::shared_ptr<FileItem>> items;
    };

    g_object_ref(widget);
    auto* task = new DirLoadTask{widget, path, my_generation, {}};

    std::thread([task]() {
        std::map<std::string, std::string> git_statuses;
        std::string check_git_cmd = "cd \"" + task->path + "\" && git rev-parse --is-inside-work-tree >/dev/null 2>&1";
        if (system(check_git_cmd.c_str()) == 0) {
            std::array<char, 256> buffer;
            std::string status_cmd = "cd \"" + task->path + "\" && git status --short . 2>/dev/null";
            std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(status_cmd.c_str(), "r"), pclose);
            if (pipe) {
                while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
                    std::string line = buffer.data();
                    if (line.size() >= 4) {
                        std::string status = line.substr(0, 2);
                        std::string file_path = line.substr(3);
                        if (!file_path.empty() && file_path.back() == '\n') file_path.pop_back();
                        if (!file_path.empty() && file_path.back() == '/') file_path.pop_back();
                        size_t slash_pos = file_path.find('/');
                        if (slash_pos != std::string::npos) file_path = file_path.substr(0, slash_pos);
                        
                        if (git_statuses.find(file_path) == git_statuses.end() || status != "??") {
                            git_statuses[file_path] = status;
                        }
                    }
                }
            }
        }

        GFile* dir = g_file_parse_name(task->path.c_str());
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
        } else if (enumerator) {
            while (true) {
                GFileInfo* info = g_file_enumerator_next_file(enumerator, nullptr, nullptr);
                if (!info) break;

                const char* name = g_file_info_get_name(info);
                GFile* child_file = g_file_get_child(dir, name);
                auto item = FileItem::from_file_info(child_file, info, 48, 20);
                if (item) {
                    if (git_statuses.count(item->name)) item->git_status = git_statuses[item->name];
                    if (item->is_directory) {
                        std::string dotgit = item->path + "/.git/HEAD";
                        FILE* f = fopen(dotgit.c_str(), "r");
                        if (f) {
                            char buf[128];
                            if (fgets(buf, sizeof(buf), f)) {
                                std::string line = buf;
                                if (!line.empty() && line.back() == '\n') line.pop_back();
                                if (line.find("ref: refs/heads/") == 0) item->git_branch = line.substr(16);
                                else item->git_branch = line.substr(0, 7);
                            }
                            fclose(f);
                        }
                    }
                    task->items.push_back(item);
                }
                g_object_unref(child_file);
                g_object_unref(info);
            }
            g_object_unref(enumerator);
        }
        if (dir) g_object_unref(dir);

        g_idle_add(+[](gpointer u) -> gboolean {
            auto* t = static_cast<DirLoadTask*>(u);
            auto* d = get_data(t->widget);
            if (d && d->thumb_generation == t->gen) {
                d->all_items = std::move(t->items);
                setup_file_monitor(d, t->path);
                repopulate_store(d);
                dispatch_thumbnails(d, t->gen);
                
                // Auto-select first item and grab focus
                if (!d->filtered_items.empty()) {
                    GtkTreePath* path = gtk_tree_path_new_from_indices(0, -1);
                    if (d->view_mode == ViewMode::GRID) {
                        gtk_icon_view_select_path(GTK_ICON_VIEW(d->icon_view), path);
                        gtk_icon_view_set_cursor(GTK_ICON_VIEW(d->icon_view), path, nullptr, FALSE);
                        gtk_widget_grab_focus(d->icon_view);
                    } else {
                        gtk_tree_view_set_cursor(GTK_TREE_VIEW(d->tree_view), path, nullptr, FALSE);
                        gtk_widget_grab_focus(d->tree_view);
                    }
                    gtk_tree_path_free(path);
                }
            }
            g_object_unref(t->widget);
            delete t;
            return G_SOURCE_REMOVE;
        }, task);
    }).detach();   // ← directory appears instantly with theme icons
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

void FileViewWidget::set_icon_size(GtkWidget* widget, int size_px) {
    auto* data = get_data(widget);
    if (!data) return;

    size_px = std::clamp(size_px, 48, 256);
    data->icon_size = size_px;
    if (data->icon_view) {
        gtk_icon_view_set_item_width(GTK_ICON_VIEW(data->icon_view), size_px);
    }
}

int FileViewWidget::get_icon_size(GtkWidget* widget) {
    auto* data = get_data(widget);
    return data ? data->icon_size : 96;
}

void FileViewWidget::zoom_in(GtkWidget* widget) {
    auto* data = get_data(widget);
    if (!data) return;
    set_icon_size(widget, data->icon_size + 16);
}

void FileViewWidget::zoom_out(GtkWidget* widget) {
    auto* data = get_data(widget);
    if (!data) return;
    set_icon_size(widget, data->icon_size - 16);
}

void FileViewWidget::zoom_reset(GtkWidget* widget) {
    set_icon_size(widget, 96);
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

struct SearchJob {
    FileViewData* data;
    std::string query;
    std::string root_path;
    uint64_t generation;
};

static void run_search_job(gpointer job_data, gpointer) {
    auto* job = static_cast<SearchJob*>(job_data);
    
    std::string q_lower = job->query;
    std::transform(q_lower.begin(), q_lower.end(), q_lower.begin(), ::tolower);
    
    std::vector<std::string> stack;
    stack.push_back(job->root_path);
    
    std::vector<std::shared_ptr<FileItem>> batch;
    
    while (!stack.empty()) {
        if (job->data->search_generation != job->generation) break;
        
        std::string current = stack.back();
        stack.pop_back();
        
        GFile* file = g_file_parse_name(current.c_str());
        GFileEnumerator* enumerator = g_file_enumerate_children(
            file,
            "standard::*,time::*,access::*,unix::*",
            G_FILE_QUERY_INFO_NONE,
            nullptr, nullptr
        );
        g_object_unref(file);
        
        if (!enumerator) continue;
        
        GFileInfo* info = nullptr;
        while ((info = g_file_enumerator_next_file(enumerator, nullptr, nullptr)) != nullptr) {
            if (job->data->search_generation != job->generation) {
                g_object_unref(info);
                break;
            }
            
            const char* name = g_file_info_get_name(info);
            if (!name || strcmp(name, ".") == 0 || strcmp(name, "..") == 0) {
                g_object_unref(info);
                continue;
            }
            
            bool is_dir = g_file_info_get_file_type(info) == G_FILE_TYPE_DIRECTORY;
            std::string child_path = current + "/" + name;
            
            if (is_dir) stack.push_back(child_path);
            
            std::string n_lower = name;
            std::transform(n_lower.begin(), n_lower.end(), n_lower.begin(), ::tolower);
            if (n_lower.find(q_lower) != std::string::npos) {
                GFile* child_file = g_file_parse_name(child_path.c_str());
                auto item = FileItem::from_file_info(child_file, info, 64, 24);
                if (item) batch.push_back(item);
                g_object_unref(child_file);
            }
            g_object_unref(info);
            
            if (batch.size() >= 20) {
                struct BatchData {
                    FileViewData* d;
                    uint64_t g;
                    std::vector<std::shared_ptr<FileItem>> items;
                };
                auto* bd = new BatchData{job->data, job->generation, std::move(batch)};
                batch.clear();
                
                g_idle_add(+[](gpointer u) -> gboolean {
                    auto* b = static_cast<BatchData*>(u);
                    if (b->d->search_generation == b->g) {
                        for (auto& i : b->items) b->d->all_items.push_back(i);
                        repopulate_store(b->d);
                    }
                    delete b;
                    return G_SOURCE_REMOVE;
                }, bd);
            }
        }
        g_object_unref(enumerator);
    }
    
    if (!batch.empty() && job->data->search_generation == job->generation) {
        struct BatchData {
            FileViewData* d;
            uint64_t g;
            std::vector<std::shared_ptr<FileItem>> items;
        };
        auto* bd = new BatchData{job->data, job->generation, std::move(batch)};
        g_idle_add(+[](gpointer u) -> gboolean {
            auto* b = static_cast<BatchData*>(u);
            if (b->d->search_generation == b->g) {
                for (auto& i : b->items) b->d->all_items.push_back(i);
                repopulate_store(b->d);
            }
            delete b;
            return G_SOURCE_REMOVE;
        }, bd);
    }
    
    delete job;
}

void FileViewWidget::set_search_query(GtkWidget* widget, const std::string& query) {
    auto* data = get_data(widget);
    if (!data) return;

    data->search_query = query;
    data->search_generation++;

    if (query.empty()) {
        data->is_searching = false;
        load_directory(widget, data->current_path);
        return;
    }

    data->is_searching = true;
    data->all_items.clear();
    repopulate_store(data);

    if (!data->search_pool) {
        data->search_pool = g_thread_pool_new(run_search_job, nullptr, 1, FALSE, nullptr);
    }

    auto* job = new SearchJob{data, query, data->current_path, data->search_generation};
    g_thread_pool_push(data->search_pool, job, nullptr);
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

void FileViewWidget::action_copy_path(GtkWidget* widget) {
    auto paths = get_selected_paths(widget);
    if (paths.empty()) {
        auto* data = get_data(widget);
        if (data && !data->current_path.empty()) {
            paths.push_back(data->current_path);
        }
    }
    if (paths.empty()) return;

    std::string text;
    for (size_t i = 0; i < paths.size(); ++i) {
        text += paths[i];
        if (i + 1 < paths.size()) text += "\n";
    }

    GtkClipboard* clip = gtk_clipboard_get(GDK_SELECTION_CLIPBOARD);
    if (clip) {
        gtk_clipboard_set_text(clip, text.c_str(), -1);
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
    if (paths.empty()) return;

    if (paths.size() > 1) {
        GtkWidget* toplevel = gtk_widget_get_toplevel(widget);
        BulkRenameDialog::show(
            GTK_IS_WINDOW(toplevel) ? GTK_WINDOW(toplevel) : nullptr,
            paths,
            [data]() {
                FileViewWidget::refresh(data->root_box);
            }
        );
        return;
    }

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

    GFile* file = g_file_parse_name(target_path.c_str());
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

    guint32 initial_mode = 0;
    if (g_file_info_has_attribute(info, G_FILE_ATTRIBUTE_UNIX_MODE)) {
        initial_mode = g_file_info_get_attribute_uint32(info, G_FILE_ATTRIBUTE_UNIX_MODE);
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

    // Checksum Row
    GtkWidget* chk_lbl = gtk_label_new("SHA256:");
    gtk_label_set_xalign(GTK_LABEL(chk_lbl), 0.0f);
    gtk_widget_add_css_class(chk_lbl, "files-prop-key");
    gtk_grid_attach(GTK_GRID(grid), chk_lbl, 0, row, 1, 1);
    
    GtkWidget* chk_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    GtkWidget* chk_btn = gtk_button_new_with_label("Calculate");
    gtk_box_pack_start(GTK_BOX(chk_box), chk_btn, FALSE, FALSE, 0);
    gtk_grid_attach(GTK_GRID(grid), chk_box, 1, row++, 1, 1);
    
    struct ChkData {
        GtkWidget* box;
        GtkWidget* btn;
        std::string path;
    };
    auto* chk_data = new ChkData{chk_box, chk_btn, item->path};
    g_signal_connect_data(chk_btn, "clicked", G_CALLBACK(+[](GtkButton* btn, gpointer udata) {
        auto* d = static_cast<ChkData*>(udata);
        gtk_widget_set_sensitive(d->btn, FALSE);
        gtk_button_set_label(GTK_BUTTON(d->btn), "Calculating...");
        
        std::thread([d]() {
            std::string cmd = "sha256sum \"" + d->path + "\" | awk '{print $1}'";
            char buffer[128];
            std::string result = "";
            FILE* pipe = popen(cmd.c_str(), "r");
            if (pipe) {
                while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
                    result += buffer;
                }
                pclose(pipe);
            }
            if (!result.empty() && result.back() == '\n') result.pop_back();
            
            struct ResultData { ChkData* d; std::string res; };
            auto* rd = new ResultData{d, result};
            
            g_idle_add(+[](gpointer ud) -> gboolean {
                auto* rd2 = static_cast<ResultData*>(ud);
                gtk_widget_destroy(rd2->d->btn);
                GtkWidget* res_lbl = gtk_label_new(rd2->res.c_str());
                gtk_label_set_selectable(GTK_LABEL(res_lbl), TRUE);
                gtk_widget_add_css_class(res_lbl, "files-prop-val");
                gtk_box_pack_start(GTK_BOX(rd2->d->box), res_lbl, FALSE, FALSE, 0);
                gtk_widget_show_all(rd2->d->box);
                delete rd2;
                return G_SOURCE_REMOVE;
            }, rd);
        }).detach();
    }), chk_data, [](gpointer d, GClosure*) { delete static_cast<ChkData*>(d); }, static_cast<GConnectFlags>(0));

    gtk_box_pack_start(GTK_BOX(content), grid, TRUE, TRUE, 0);

    gtk_box_pack_start(GTK_BOX(content), gtk_separator_new(GTK_ORIENTATION_HORIZONTAL), FALSE, FALSE, 0);

    GtkWidget* perm_label = gtk_label_new("Permissions");
    gtk_label_set_xalign(GTK_LABEL(perm_label), 0.0f);
    gtk_widget_add_css_class(perm_label, "files-prop-title");
    gtk_box_pack_start(GTK_BOX(content), perm_label, FALSE, FALSE, 0);

    GtkWidget* pgrid = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(pgrid), 4);
    gtk_grid_set_column_spacing(GTK_GRID(pgrid), 16);

    const char* owners[] = {"Owner", "Group", "Others"};
    int shifts[] = {6, 3, 0};

    gtk_grid_attach(GTK_GRID(pgrid), gtk_label_new("Read"), 1, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(pgrid), gtk_label_new("Write"), 2, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(pgrid), gtk_label_new("Execute"), 3, 0, 1, 1);

    struct PermState {
        std::string path;
        guint32 current_mode;
    };
    auto* perm_state = new PermState{target_path, initial_mode};

    for (int i = 0; i < 3; ++i) {
        GtkWidget* lbl = gtk_label_new(owners[i]);
        gtk_label_set_xalign(GTK_LABEL(lbl), 0.0f);
        gtk_grid_attach(GTK_GRID(pgrid), lbl, 0, i+1, 1, 1);
        for (int j = 0; j < 3; ++j) {
            GtkWidget* cb = gtk_check_button_new();
            int bit = 1 << (shifts[i] + (2 - j));
            gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(cb), (initial_mode & bit) != 0);
            auto* cb_data = new std::pair<PermState*, int>(perm_state, bit);
            auto cb_func = +[](GtkToggleButton* btn, gpointer udata) {
                auto* d = static_cast<std::pair<PermState*, int>*>(udata);
                if (gtk_toggle_button_get_active(btn)) d->first->current_mode |= d->second;
                else d->first->current_mode &= ~(d->second);
                GFile* f = g_file_parse_name(d->first->path.c_str());
                g_file_set_attribute_uint32(f, G_FILE_ATTRIBUTE_UNIX_MODE, d->first->current_mode, G_FILE_QUERY_INFO_NONE, nullptr, nullptr);
                g_object_unref(f);
            };
            g_signal_connect_data(cb, "toggled", G_CALLBACK(cb_func), cb_data, [](gpointer d, GClosure*) { delete static_cast<std::pair<PermState*, int>*>(d); }, static_cast<GConnectFlags>(0));
            gtk_grid_attach(GTK_GRID(pgrid), cb, j+1, i+1, 1, 1);
        }
    }
    gtk_box_pack_start(GTK_BOX(content), pgrid, TRUE, TRUE, 0);

    auto destroy_func = +[](GtkWidget*, gpointer u) { delete static_cast<PermState*>(u); };
    g_signal_connect_data(dialog, "destroy", G_CALLBACK(destroy_func), perm_state, nullptr, static_cast<GConnectFlags>(0));

    gtk_widget_show_all(dialog);
    gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);
}



void FileViewWidget::action_restore_selected(GtkWidget* widget) {
    auto paths = get_selected_paths(widget);
    if (!paths.empty()) {
        FileOperations::restore_from_trash(paths);
        refresh(widget);
    }
}

void FileViewWidget::action_empty_trash(GtkWidget* widget) {
    GtkWidget* toplevel = gtk_widget_get_toplevel(widget);
    GtkWidget* dialog = gtk_message_dialog_new(
        GTK_IS_WINDOW(toplevel) ? GTK_WINDOW(toplevel) : nullptr,
        static_cast<GtkDialogFlags>(GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT),
        GTK_MESSAGE_WARNING,
        GTK_BUTTONS_OK_CANCEL,
        "Are you sure you want to empty the trash? All items will be permanently deleted."
    );
    gtk_widget_add_css_class(dialog, "zenith-files-dialog");
    if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_OK) {
        FileOperations::empty_trash();
        refresh(widget);
    }
    gtk_widget_destroy(dialog);
}
} // namespace zenith
