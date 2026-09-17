#include "shell/files/duplicate_finder_dialog.hpp"
#include <thread>
#include <vector>
#include <string>
#include <unordered_map>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <glib.h>

namespace fs = std::filesystem;

namespace zenith {

struct DuplicateGroup {
    uintmax_t size;
    std::string hash;
    std::vector<std::string> paths;
};

struct DupFinderContext {
    GtkWidget* window;
    GtkWidget* tree_view;
    GtkTreeStore* tree_store;
    GtkWidget* status_label;
    GtkWidget* spinner;
    GtkWidget* delete_btn;
    
    std::string root_path;
    std::vector<DuplicateGroup> duplicates;
    
    std::atomic<bool> cancel_flag{false};
    std::atomic<int> files_scanned{0};
};

// UI Update Messages
struct DupUpdateMsg {
    DupFinderContext* ctx;
    int files_scanned;
};

enum {
    COL_CHECK = 0,
    COL_TEXT,
    COL_IS_PARENT,
    COL_PATH,
    NUM_COLS
};

static std::string calculate_hash(const std::string& path, bool full = true) {
    GChecksum* sha = g_checksum_new(G_CHECKSUM_SHA256);
    std::ifstream file(path, std::ios::binary);
    if (file) {
        char buffer[65536];
        if (!full) {
            if (file.read(buffer, sizeof(buffer)) || file.gcount() > 0) {
                g_checksum_update(sha, (const guchar*)buffer, file.gcount());
            }
        } else {
            while (file.read(buffer, sizeof(buffer))) {
                g_checksum_update(sha, (const guchar*)buffer, file.gcount());
            }
            if (file.gcount() > 0) {
                g_checksum_update(sha, (const guchar*)buffer, file.gcount());
            }
        }
    }
    std::string result = g_checksum_get_string(sha) ? g_checksum_get_string(sha) : "";
    g_checksum_free(sha);
    return result;
}

static std::string format_size(uintmax_t bytes) {
    char buf[64];
    if (bytes < 1024) snprintf(buf, sizeof(buf), "%ju B", bytes);
    else if (bytes < 1024 * 1024) snprintf(buf, sizeof(buf), "%.1f KB", bytes / 1024.0);
    else if (bytes < 1024 * 1024 * 1024) snprintf(buf, sizeof(buf), "%.2f MB", bytes / (1024.0 * 1024.0));
    else snprintf(buf, sizeof(buf), "%.2f GB", bytes / (1024.0 * 1024.0 * 1024.0));
    return buf;
}

static gboolean on_scan_update(gpointer data) {
    auto* msg = static_cast<DupUpdateMsg*>(data);
    auto* ctx = msg->ctx;
    
    char buf[256];
    snprintf(buf, sizeof(buf), "Scanning... (%d files analyzed)", msg->files_scanned);
    gtk_label_set_text(GTK_LABEL(ctx->status_label), buf);
    
    delete msg;
    return G_SOURCE_REMOVE;
}

static gboolean on_scan_complete(gpointer data) {
    auto* ctx = static_cast<DupFinderContext*>(data);
    
    gtk_spinner_stop(GTK_SPINNER(ctx->spinner));
    gtk_widget_hide(ctx->spinner);
    
    if (ctx->duplicates.empty()) {
        gtk_label_set_text(GTK_LABEL(ctx->status_label), "No duplicate files found.");
    } else {
        char buf[256];
        snprintf(buf, sizeof(buf), "Found %zu groups of duplicates.", ctx->duplicates.size());
        gtk_label_set_text(GTK_LABEL(ctx->status_label), buf);
        
        gtk_tree_store_clear(ctx->tree_store);
        
        for (const auto& group : ctx->duplicates) {
            GtkTreeIter parent_iter;
            std::string parent_text = std::to_string(group.paths.size()) + " identical files (" + format_size(group.size) + " each)";
            
            gtk_tree_store_append(ctx->tree_store, &parent_iter, nullptr);
            gtk_tree_store_set(ctx->tree_store, &parent_iter,
                               COL_CHECK, FALSE,
                               COL_TEXT, parent_text.c_str(),
                               COL_IS_PARENT, TRUE,
                               COL_PATH, "",
                               -1);
                               
            // By default, leave the first file unchecked, and check the rest so it's easy to delete copies
            bool first = true;
            for (const auto& p : group.paths) {
                GtkTreeIter child_iter;
                gtk_tree_store_append(ctx->tree_store, &child_iter, &parent_iter);
                gtk_tree_store_set(ctx->tree_store, &child_iter,
                                   COL_CHECK, !first,
                                   COL_TEXT, p.c_str(),
                                   COL_IS_PARENT, FALSE,
                                   COL_PATH, p.c_str(),
                                   -1);
                first = false;
            }
        }
        
        gtk_tree_view_expand_all(GTK_TREE_VIEW(ctx->tree_view));
        gtk_widget_set_sensitive(ctx->delete_btn, TRUE);
    }
    
    return G_SOURCE_REMOVE;
}

static void scanner_thread(DupFinderContext* ctx) {
    std::unordered_map<uintmax_t, std::vector<std::string>> size_map;
    
    int scanned = 0;
    try {
        for (const auto& entry : fs::recursive_directory_iterator(ctx->root_path, fs::directory_options::skip_permission_denied)) {
            if (ctx->cancel_flag) break;
            if (entry.is_regular_file()) {
                uintmax_t size = entry.file_size();
                if (size > 0) { // ignore empty files
                    size_map[size].push_back(entry.path().string());
                }
                scanned++;
                if (scanned % 500 == 0) {
                    auto* msg = new DupUpdateMsg{ctx, scanned};
                    g_idle_add(on_scan_update, msg);
                }
            }
        }
    } catch (...) {}
    
    // Step 2: Partial Hash for size matches
    std::unordered_map<std::string, std::vector<std::string>> partial_hash_map;
    for (const auto& [size, paths] : size_map) {
        if (ctx->cancel_flag) break;
        if (paths.size() > 1) {
            for (const auto& p : paths) {
                std::string phash = calculate_hash(p, false);
                std::string key = std::to_string(size) + "_" + phash;
                partial_hash_map[key].push_back(p);
            }
        }
    }
    
    // Step 3: Full Hash
    for (const auto& [key, paths] : partial_hash_map) {
        if (ctx->cancel_flag) break;
        if (paths.size() > 1) {
            std::unordered_map<std::string, std::vector<std::string>> full_hash_map;
            for (const auto& p : paths) {
                full_hash_map[calculate_hash(p, true)].push_back(p);
            }
            
            for (const auto& [hash, exact_paths] : full_hash_map) {
                if (exact_paths.size() > 1) {
                    DuplicateGroup g;
                    g.hash = hash;
                    g.paths = exact_paths;
                    
                    try {
                        g.size = fs::file_size(exact_paths[0]);
                    } catch(...) { g.size = 0; }
                    
                    ctx->duplicates.push_back(g);
                }
            }
        }
    }
    
    if (!ctx->cancel_flag) {
        g_idle_add(on_scan_complete, ctx);
    }
}

static void on_cell_toggled(GtkCellRendererToggle*, gchar* path_string, gpointer user_data) {
    auto* ctx = static_cast<DupFinderContext*>(user_data);
    GtkTreeIter iter;
    if (gtk_tree_model_get_iter_from_string(GTK_TREE_MODEL(ctx->tree_store), &iter, path_string)) {
        gboolean is_parent;
        gtk_tree_model_get(GTK_TREE_MODEL(ctx->tree_store), &iter, COL_IS_PARENT, &is_parent, -1);
        if (is_parent) return; // don't toggle parents
        
        gboolean val;
        gtk_tree_model_get(GTK_TREE_MODEL(ctx->tree_store), &iter, COL_CHECK, &val, -1);
        gtk_tree_store_set(ctx->tree_store, &iter, COL_CHECK, !val, -1);
    }
}

static void on_delete_clicked(GtkButton*, gpointer user_data) {
    auto* ctx = static_cast<DupFinderContext*>(user_data);
    
    GtkTreeIter iter;
    bool valid = gtk_tree_model_get_iter_first(GTK_TREE_MODEL(ctx->tree_store), &iter);
    
    int deleted = 0;
    while (valid) {
        GtkTreeIter child;
        bool has_child = gtk_tree_model_iter_children(GTK_TREE_MODEL(ctx->tree_store), &child, &iter);
        while (has_child) {
            gboolean checked;
            gchar* path_ptr = nullptr;
            gtk_tree_model_get(GTK_TREE_MODEL(ctx->tree_store), &child, COL_CHECK, &checked, COL_PATH, &path_ptr, -1);
            
            if (checked && path_ptr && strlen(path_ptr) > 0) {
                try {
                    fs::remove(path_ptr);
                    deleted++;
                } catch(...) {}
            }
            g_free(path_ptr);
            has_child = gtk_tree_model_iter_next(GTK_TREE_MODEL(ctx->tree_store), &child);
        }
        valid = gtk_tree_model_iter_next(GTK_TREE_MODEL(ctx->tree_store), &iter);
    }
    
    gtk_widget_set_sensitive(ctx->delete_btn, FALSE);
    char buf[256];
    snprintf(buf, sizeof(buf), "Deleted %d duplicate files.", deleted);
    gtk_label_set_text(GTK_LABEL(ctx->status_label), buf);
    gtk_tree_store_clear(ctx->tree_store);
}

void DuplicateFinderDialog::show(GtkWindow* parent, const std::string& directory_path) {
    auto* ctx = new DupFinderContext();
    ctx->root_path = directory_path;
    
    std::string dirname = g_path_get_basename(directory_path.c_str());
    
    ctx->window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(ctx->window), ("Find Duplicates: " + dirname).c_str());
    gtk_window_set_default_size(GTK_WINDOW(ctx->window), 700, 500);
    gtk_window_set_position(GTK_WINDOW(ctx->window), GTK_WIN_POS_CENTER_ON_PARENT);
    gtk_window_set_transient_for(GTK_WINDOW(ctx->window), parent);
    gtk_window_set_destroy_with_parent(GTK_WINDOW(ctx->window), TRUE);
    
    g_signal_connect(ctx->window, "destroy", G_CALLBACK(+[](GtkWidget*, gpointer data) {
        auto* c = static_cast<DupFinderContext*>(data);
        c->cancel_flag = true;
        delete c; // thread will detach and die safely due to fast checks
    }), ctx);
    
    GtkWidget* vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_container_set_border_width(GTK_CONTAINER(vbox), 12);
    gtk_container_add(GTK_CONTAINER(ctx->window), vbox);
    
    GtkWidget* header_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_box_pack_start(GTK_BOX(vbox), header_box, FALSE, FALSE, 0);
    
    ctx->status_label = gtk_label_new("Initializing scan...");
    gtk_label_set_xalign(GTK_LABEL(ctx->status_label), 0.0);
    gtk_box_pack_start(GTK_BOX(header_box), ctx->status_label, TRUE, TRUE, 0);
    
    ctx->spinner = gtk_spinner_new();
    gtk_spinner_start(GTK_SPINNER(ctx->spinner));
    gtk_box_pack_end(GTK_BOX(header_box), ctx->spinner, FALSE, FALSE, 0);
    
    // TreeView
    GtkWidget* scroll = gtk_scrolled_window_new(nullptr, nullptr);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(scroll), GTK_SHADOW_IN);
    gtk_widget_set_vexpand(scroll, TRUE);
    gtk_box_pack_start(GTK_BOX(vbox), scroll, TRUE, TRUE, 0);
    
    ctx->tree_store = gtk_tree_store_new(NUM_COLS, G_TYPE_BOOLEAN, G_TYPE_STRING, G_TYPE_BOOLEAN, G_TYPE_STRING);
    ctx->tree_view = gtk_tree_view_new_with_model(GTK_TREE_MODEL(ctx->tree_store));
    gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(ctx->tree_view), FALSE);
    
    // Checkbox column
    GtkCellRenderer* toggle = gtk_cell_renderer_toggle_new();
    g_signal_connect(toggle, "toggled", G_CALLBACK(on_cell_toggled), ctx);
    GtkTreeViewColumn* col_check = gtk_tree_view_column_new_with_attributes("", toggle, "active", COL_CHECK, "visible", COL_CHECK, nullptr);
    
    // Make checkboxes invisible for parent rows
    gtk_tree_view_column_set_cell_data_func(col_check, toggle, [](GtkTreeViewColumn*, GtkCellRenderer* cell, GtkTreeModel* model, GtkTreeIter* iter, gpointer) {
        gboolean is_parent;
        gtk_tree_model_get(model, iter, COL_IS_PARENT, &is_parent, -1);
        g_object_set(cell, "visible", !is_parent, nullptr);
    }, nullptr, nullptr);
    gtk_tree_view_append_column(GTK_TREE_VIEW(ctx->tree_view), col_check);
    
    // Text column
    GtkCellRenderer* text_renderer = gtk_cell_renderer_text_new();
    g_object_set(text_renderer, "ellipsize", PANGO_ELLIPSIZE_START, nullptr);
    GtkTreeViewColumn* col_text = gtk_tree_view_column_new_with_attributes("File", text_renderer, "text", COL_TEXT, nullptr);
    gtk_tree_view_column_set_expand(col_text, TRUE);
    gtk_tree_view_append_column(GTK_TREE_VIEW(ctx->tree_view), col_text);
    
    gtk_container_add(GTK_CONTAINER(scroll), ctx->tree_view);
    
    GtkWidget* btn_box = gtk_button_box_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_button_box_set_layout(GTK_BUTTON_BOX(btn_box), GTK_BUTTONBOX_END);
    gtk_box_pack_start(GTK_BOX(vbox), btn_box, FALSE, FALSE, 0);
    
    ctx->delete_btn = gtk_button_new_with_label("Delete Selected");
    gtk_widget_set_sensitive(ctx->delete_btn, FALSE);
    gtk_style_context_add_class(gtk_widget_get_style_context(ctx->delete_btn), "destructive-action");
    g_signal_connect(ctx->delete_btn, "clicked", G_CALLBACK(on_delete_clicked), ctx);
    gtk_container_add(GTK_CONTAINER(btn_box), ctx->delete_btn);
    
    gtk_widget_show_all(ctx->window);
    
    std::thread t(scanner_thread, ctx);
    t.detach();
}

} // namespace zenith
