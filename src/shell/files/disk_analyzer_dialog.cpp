#include "shell/files/disk_analyzer_dialog.hpp"
#include <thread>
#include <vector>
#include <string>
#include <filesystem>
#include <algorithm>
#include <atomic>
#include <glib.h>

namespace fs = std::filesystem;

namespace zenith {

struct DirNode {
    std::string name;
    std::string path;
    bool is_dir = false;
    uintmax_t size = 0;
    int items = 0;
    std::vector<DirNode*> children;
    
    ~DirNode() {
        for (auto* c : children) delete c;
    }
};

struct DiskAnalyzerContext {
    GtkWidget* window;
    GtkWidget* tree_view;
    GtkTreeStore* tree_store;
    GtkWidget* status_label;
    GtkWidget* spinner;
    
    std::string root_path;
    DirNode* root_node = nullptr;
    
    std::atomic<bool> cancel_flag{false};
    std::atomic<int> dirs_scanned{0};
};

// UI Update Messages
struct AnalyzeUpdateMsg {
    DiskAnalyzerContext* ctx;
    int dirs_scanned;
};

enum {
    COL_NAME = 0,
    COL_SIZE_STR,
    COL_PERCENT,
    COL_ITEMS,
    COL_PATH,
    NUM_COLS
};

static std::string format_size(uintmax_t bytes) {
    char buf[64];
    if (bytes < 1024) snprintf(buf, sizeof(buf), "%ju B", bytes);
    else if (bytes < 1024 * 1024) snprintf(buf, sizeof(buf), "%.1f KB", bytes / 1024.0);
    else if (bytes < 1024 * 1024 * 1024) snprintf(buf, sizeof(buf), "%.2f MB", bytes / (1024.0 * 1024.0));
    else snprintf(buf, sizeof(buf), "%.2f GB", bytes / (1024.0 * 1024.0 * 1024.0));
    return buf;
}

static gboolean on_analyze_update(gpointer data) {
    auto* msg = static_cast<AnalyzeUpdateMsg*>(data);
    auto* ctx = msg->ctx;
    
    char buf[256];
    snprintf(buf, sizeof(buf), "Scanning... (%d directories analyzed)", msg->dirs_scanned);
    gtk_label_set_text(GTK_LABEL(ctx->status_label), buf);
    
    delete msg;
    return G_SOURCE_REMOVE;
}

static void populate_store(DiskAnalyzerContext* ctx, DirNode* node, GtkTreeIter* parent_iter, uintmax_t parent_size) {
    if (!node || ctx->cancel_flag) return;
    
    GtkTreeIter iter;
    gtk_tree_store_append(ctx->tree_store, &iter, parent_iter);
    
    int percent = parent_size > 0 ? (int)((node->size * 100) / parent_size) : 0;
    if (percent > 100) percent = 100;
    
    std::string items_str = node->is_dir ? std::to_string(node->items) + " items" : "File";
    
    gtk_tree_store_set(ctx->tree_store, &iter,
                       COL_NAME, node->name.c_str(),
                       COL_SIZE_STR, format_size(node->size).c_str(),
                       COL_PERCENT, percent,
                       COL_ITEMS, items_str.c_str(),
                       COL_PATH, node->path.c_str(),
                       -1);
    
    // Only populate children if it's a directory and it has children (avoids huge memory use for massive dirs, we only show top ones)
    // To keep UI fast, we sort children by size and take the top 50
    std::sort(node->children.begin(), node->children.end(), [](DirNode* a, DirNode* b) {
        return a->size > b->size;
    });
    
    int count = 0;
    for (auto* child : node->children) {
        if (count++ >= 50) break; // Limit to top 50 per folder to keep UI snappy
        populate_store(ctx, child, &iter, node->size);
    }
}

static gboolean on_analyze_complete(gpointer data) {
    auto* ctx = static_cast<DiskAnalyzerContext*>(data);
    
    gtk_spinner_stop(GTK_SPINNER(ctx->spinner));
    gtk_widget_hide(ctx->spinner);
    
    if (ctx->root_node) {
        char buf[256];
        snprintf(buf, sizeof(buf), "Total Size: %s", format_size(ctx->root_node->size).c_str());
        gtk_label_set_text(GTK_LABEL(ctx->status_label), buf);
        
        gtk_tree_store_clear(ctx->tree_store);
        
        // Don't show the root itself, just its children at the top level
        std::sort(ctx->root_node->children.begin(), ctx->root_node->children.end(), [](DirNode* a, DirNode* b) {
            return a->size > b->size;
        });
        
        for (auto* child : ctx->root_node->children) {
            populate_store(ctx, child, nullptr, ctx->root_node->size);
        }
    } else {
        gtk_label_set_text(GTK_LABEL(ctx->status_label), "Analysis failed.");
    }
    
    return G_SOURCE_REMOVE;
}

static DirNode* analyze_directory(DiskAnalyzerContext* ctx, const std::string& path) {
    if (ctx->cancel_flag) return nullptr;
    
    auto* node = new DirNode();
    node->path = path;
    node->name = g_path_get_basename(path.c_str());
    
    try {
        if (fs::is_directory(path)) {
            node->is_dir = true;
            for (const auto& entry : fs::directory_iterator(path, fs::directory_options::skip_permission_denied)) {
                if (ctx->cancel_flag) break;
                
                if (entry.is_directory() && !entry.is_symlink()) {
                    auto* child = analyze_directory(ctx, entry.path().string());
                    if (child) {
                        node->children.push_back(child);
                        node->size += child->size;
                        node->items += child->items + 1; // +1 for the folder itself
                    }
                } else if (entry.is_regular_file() && !entry.is_symlink()) {
                    auto* child = new DirNode();
                    child->path = entry.path().string();
                    child->name = entry.path().filename().string();
                    child->is_dir = false;
                    try {
                        child->size = entry.file_size();
                    } catch(...) { child->size = 0; }
                    node->children.push_back(child);
                    node->size += child->size;
                    node->items += 1;
                }
            }
            
            ctx->dirs_scanned++;
            if (ctx->dirs_scanned % 50 == 0) {
                auto* msg = new AnalyzeUpdateMsg{ctx, ctx->dirs_scanned.load()};
                g_idle_add(on_analyze_update, msg);
            }
        }
    } catch (...) {}
    
    return node;
}

static void analyzer_thread(DiskAnalyzerContext* ctx) {
    ctx->root_node = analyze_directory(ctx, ctx->root_path);
    
    if (!ctx->cancel_flag) {
        g_idle_add(on_analyze_complete, ctx);
    }
}

void DiskAnalyzerDialog::show(GtkWindow* parent, const std::string& directory_path) {
    auto* ctx = new DiskAnalyzerContext();
    ctx->root_path = directory_path;
    
    std::string dirname = g_path_get_basename(directory_path.c_str());
    
    ctx->window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(ctx->window), ("Disk Analyzer: " + dirname).c_str());
    gtk_window_set_default_size(GTK_WINDOW(ctx->window), 800, 600);
    gtk_window_set_position(GTK_WINDOW(ctx->window), GTK_WIN_POS_CENTER_ON_PARENT);
    gtk_window_set_transient_for(GTK_WINDOW(ctx->window), parent);
    gtk_window_set_destroy_with_parent(GTK_WINDOW(ctx->window), TRUE);
    
    g_signal_connect(ctx->window, "destroy", G_CALLBACK(+[](GtkWidget*, gpointer data) {
        auto* c = static_cast<DiskAnalyzerContext*>(data);
        c->cancel_flag = true;
        // Don't delete ctx immediately because the thread might still be running and accessing root_node.
        // We will leak the ctx intentionally if closed while scanning to prevent segfaults,
        // or we could join the thread. A detached thread is fine since this is just an analyzer.
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
    
    ctx->tree_store = gtk_tree_store_new(NUM_COLS, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_INT, G_TYPE_STRING, G_TYPE_STRING);
    ctx->tree_view = gtk_tree_view_new_with_model(GTK_TREE_MODEL(ctx->tree_store));
    
    // Name column
    GtkCellRenderer* text_renderer = gtk_cell_renderer_text_new();
    g_object_set(text_renderer, "ellipsize", PANGO_ELLIPSIZE_START, nullptr);
    GtkTreeViewColumn* col_name = gtk_tree_view_column_new_with_attributes("Name", text_renderer, "text", COL_NAME, nullptr);
    gtk_tree_view_column_set_expand(col_name, TRUE);
    gtk_tree_view_column_set_resizable(col_name, TRUE);
    gtk_tree_view_append_column(GTK_TREE_VIEW(ctx->tree_view), col_name);
    
    // Size column
    GtkCellRenderer* size_renderer = gtk_cell_renderer_text_new();
    GtkTreeViewColumn* col_size = gtk_tree_view_column_new_with_attributes("Size", size_renderer, "text", COL_SIZE_STR, nullptr);
    gtk_tree_view_column_set_resizable(col_size, TRUE);
    gtk_tree_view_append_column(GTK_TREE_VIEW(ctx->tree_view), col_size);
    
    // Percent Progress column
    GtkCellRenderer* prog_renderer = gtk_cell_renderer_progress_new();
    GtkTreeViewColumn* col_prog = gtk_tree_view_column_new_with_attributes("%", prog_renderer, "value", COL_PERCENT, nullptr);
    gtk_tree_view_column_set_min_width(col_prog, 120);
    gtk_tree_view_append_column(GTK_TREE_VIEW(ctx->tree_view), col_prog);
    
    // Items column
    GtkCellRenderer* items_renderer = gtk_cell_renderer_text_new();
    GtkTreeViewColumn* col_items = gtk_tree_view_column_new_with_attributes("Items", items_renderer, "text", COL_ITEMS, nullptr);
    gtk_tree_view_append_column(GTK_TREE_VIEW(ctx->tree_view), col_items);
    
    gtk_container_add(GTK_CONTAINER(scroll), ctx->tree_view);
    
    gtk_widget_show_all(ctx->window);
    
    std::thread t(analyzer_thread, ctx);
    t.detach();
}

} // namespace zenith
