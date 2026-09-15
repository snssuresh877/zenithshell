#include "shell/files/bulk_rename_dialog.hpp"
#include <gio/gio.h>
#include <glib/gstdio.h>
#include <filesystem>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <regex>

namespace fs = std::filesystem;

namespace zenith {

enum {
    COL_ORIGINAL_PATH = 0,
    COL_OLD_NAME,
    COL_NEW_NAME,
    NUM_COLS
};

struct BulkRenameState {
    GtkWidget* dialog{nullptr};
    GtkListStore* store{nullptr};
    
    GtkWidget* prefix_entry{nullptr};
    GtkWidget* suffix_entry{nullptr};
    GtkWidget* find_entry{nullptr};
    GtkWidget* replace_entry{nullptr};
    GtkWidget* number_check{nullptr};
    GtkWidget* number_start_spin{nullptr};

    std::vector<std::string> original_paths;
    std::function<void()> on_complete;
};

static void update_preview(BulkRenameState* state) {
    const char* prefix = gtk_entry_get_text(GTK_ENTRY(state->prefix_entry));
    const char* suffix = gtk_entry_get_text(GTK_ENTRY(state->suffix_entry));
    const char* find_str = gtk_entry_get_text(GTK_ENTRY(state->find_entry));
    const char* replace_str = gtk_entry_get_text(GTK_ENTRY(state->replace_entry));
    gboolean do_number = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(state->number_check));
    int num_start = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(state->number_start_spin));

    std::string p(prefix ? prefix : "");
    std::string s(suffix ? suffix : "");
    std::string f(find_str ? find_str : "");
    std::string r(replace_str ? replace_str : "");

    GtkTreeIter iter;
    gboolean valid = gtk_tree_model_get_iter_first(GTK_TREE_MODEL(state->store), &iter);
    int current_num = num_start;

    while (valid) {
        gchar* old_name_c = nullptr;
        gtk_tree_model_get(GTK_TREE_MODEL(state->store), &iter, COL_OLD_NAME, &old_name_c, -1);
        if (old_name_c) {
            std::string old_name(old_name_c);
            g_free(old_name_c);

            // Separate name and extension
            size_t dot_pos = old_name.find_last_of('.');
            std::string base = old_name;
            std::string ext = "";
            if (dot_pos != std::string::npos && dot_pos > 0) {
                base = old_name.substr(0, dot_pos);
                ext = old_name.substr(dot_pos);
            }

            // Find and Replace
            if (!f.empty()) {
                size_t pos = 0;
                while ((pos = base.find(f, pos)) != std::string::npos) {
                    base.replace(pos, f.length(), r);
                    pos += r.length();
                }
            }

            // Add Prefix and Suffix
            std::string new_name = p + base + s;

            // Numbering
            if (do_number) {
                std::ostringstream ss;
                ss << std::setw(3) << std::setfill('0') << current_num;
                new_name += "_" + ss.str();
                current_num++;
            }

            new_name += ext;

            gtk_list_store_set(state->store, &iter, COL_NEW_NAME, new_name.c_str(), -1);
        }
        valid = gtk_tree_model_iter_next(GTK_TREE_MODEL(state->store), &iter);
    }
}

static void on_input_changed(GtkWidget*, gpointer user_data) {
    update_preview(static_cast<BulkRenameState*>(user_data));
}

static void execute_rename(BulkRenameState* state) {
    GtkTreeIter iter;
    gboolean valid = gtk_tree_model_get_iter_first(GTK_TREE_MODEL(state->store), &iter);

    while (valid) {
        gchar* orig_path_c = nullptr;
        gchar* new_name_c = nullptr;
        gtk_tree_model_get(GTK_TREE_MODEL(state->store), &iter, 
            COL_ORIGINAL_PATH, &orig_path_c, 
            COL_NEW_NAME, &new_name_c, -1);

        if (orig_path_c && new_name_c) {
            fs::path src(orig_path_c);
            fs::path dst = src.parent_path() / new_name_c;
            
            if (src != dst && !fs::exists(dst)) {
                try {
                    fs::rename(src, dst);
                } catch (...) {}
            }
        }

        g_free(orig_path_c);
        g_free(new_name_c);

        valid = gtk_tree_model_iter_next(GTK_TREE_MODEL(state->store), &iter);
    }
}

void BulkRenameDialog::show(GtkWindow* parent, const std::vector<std::string>& paths, std::function<void()> on_complete) {
    auto* state = new BulkRenameState();
    state->original_paths = paths;
    state->on_complete = on_complete;

    state->dialog = gtk_dialog_new_with_buttons(
        "Bulk Rename",
        parent,
        static_cast<GtkDialogFlags>(GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT),
        "_Cancel", GTK_RESPONSE_CANCEL,
        "_Rename All", GTK_RESPONSE_ACCEPT,
        nullptr
    );
    gtk_window_set_default_size(GTK_WINDOW(state->dialog), 600, 400);
    gtk_style_context_add_class(gtk_widget_get_style_context(state->dialog), "zenith-files-dialog");

    GtkWidget* content = gtk_dialog_get_content_area(GTK_DIALOG(state->dialog));
    gtk_container_set_border_width(GTK_CONTAINER(content), 16);

    // Split horizontally: Inputs on Left, Preview on Right
    GtkWidget* hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 16);
    gtk_box_pack_start(GTK_BOX(content), hbox, TRUE, TRUE, 0);

    // --- Inputs Panel ---
    GtkWidget* vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
    gtk_box_pack_start(GTK_BOX(hbox), vbox, FALSE, FALSE, 0);

    GtkWidget* grid = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(grid), 8);
    gtk_grid_set_column_spacing(GTK_GRID(grid), 12);
    gtk_box_pack_start(GTK_BOX(vbox), grid, FALSE, FALSE, 0);

    state->find_entry = gtk_entry_new();
    state->replace_entry = gtk_entry_new();
    state->prefix_entry = gtk_entry_new();
    state->suffix_entry = gtk_entry_new();

    gtk_grid_attach(GTK_GRID(grid), gtk_label_new("Find:"), 0, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), state->find_entry, 1, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), gtk_label_new("Replace:"), 0, 1, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), state->replace_entry, 1, 1, 1, 1);
    
    gtk_grid_attach(GTK_GRID(grid), gtk_label_new("Insert Prefix:"), 0, 2, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), state->prefix_entry, 1, 2, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), gtk_label_new("Insert Suffix:"), 0, 3, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), state->suffix_entry, 1, 3, 1, 1);

    // Numbering block
    GtkWidget* num_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    state->number_check = gtk_check_button_new_with_label("Add Numbering");
    state->number_start_spin = gtk_spin_button_new_with_range(1, 9999, 1);
    gtk_box_pack_start(GTK_BOX(num_box), state->number_check, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(num_box), gtk_label_new("Start at:"), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(num_box), state->number_start_spin, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), num_box, FALSE, FALSE, 0);

    // --- Preview Panel ---
    state->store = gtk_list_store_new(NUM_COLS, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING);
    GtkWidget* tree_view = gtk_tree_view_new_with_model(GTK_TREE_MODEL(state->store));
    g_object_unref(state->store); // TreeView holds reference

    gtk_tree_view_append_column(GTK_TREE_VIEW(tree_view), 
        gtk_tree_view_column_new_with_attributes("Original Name", gtk_cell_renderer_text_new(), "text", COL_OLD_NAME, nullptr));
    gtk_tree_view_append_column(GTK_TREE_VIEW(tree_view), 
        gtk_tree_view_column_new_with_attributes("New Name", gtk_cell_renderer_text_new(), "text", COL_NEW_NAME, nullptr));
    
    GtkWidget* scroll = gtk_scrolled_window_new(nullptr, nullptr);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_widget_set_size_request(scroll, 350, -1);
    gtk_container_add(GTK_CONTAINER(scroll), tree_view);
    gtk_box_pack_start(GTK_BOX(hbox), scroll, TRUE, TRUE, 0);

    // Populate Store
    for (const auto& path : paths) {
        GFile* f = g_file_parse_name(path.c_str());
        char* name = g_file_get_basename(f);
        if (name) {
            GtkTreeIter iter;
            gtk_list_store_append(state->store, &iter);
            gtk_list_store_set(state->store, &iter, 
                COL_ORIGINAL_PATH, path.c_str(),
                COL_OLD_NAME, name,
                COL_NEW_NAME, name,
                -1);
            g_free(name);
        }
        g_object_unref(f);
    }

    // Connect signals
    g_signal_connect(state->find_entry, "changed", G_CALLBACK(on_input_changed), state);
    g_signal_connect(state->replace_entry, "changed", G_CALLBACK(on_input_changed), state);
    g_signal_connect(state->prefix_entry, "changed", G_CALLBACK(on_input_changed), state);
    g_signal_connect(state->suffix_entry, "changed", G_CALLBACK(on_input_changed), state);
    g_signal_connect(state->number_check, "toggled", G_CALLBACK(on_input_changed), state);
    g_signal_connect(state->number_start_spin, "value-changed", G_CALLBACK(on_input_changed), state);

    g_signal_connect(state->dialog, "response", G_CALLBACK(+[](GtkDialog* dialog, gint response_id, gpointer user_data) {
        auto* s = static_cast<BulkRenameState*>(user_data);
        if (response_id == GTK_RESPONSE_ACCEPT) {
            execute_rename(s);
            if (s->on_complete) s->on_complete();
        }
        gtk_widget_destroy(GTK_WIDGET(dialog));
        delete s;
    }), state);

    gtk_widget_show_all(state->dialog);
}

} // namespace zenith
