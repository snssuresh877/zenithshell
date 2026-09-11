#include "shell/files/command_palette.hpp"
#include "shell/files/file_shortcuts.hpp"
#include "../../gtk3_compat.hpp"
#include <algorithm>
#include <iostream>

namespace zenith {

struct PaletteContext {
    GtkWidget* dialog{nullptr};
    GtkWidget* entry{nullptr};
    GtkWidget* listbox{nullptr};
    CommandPalette::ActionHandler handler;
    std::vector<PaletteAction> actions;
    std::vector<GtkWidget*> visible_rows;
};

static std::vector<PaletteAction> get_default_actions() {
    return {
        // Navigation
        {"new_tab", "New Tab", "Navigation", "tab-new-symbolic", "new_tab"},
        {"close_tab", "Close Current Tab", "Navigation", "window-close-symbolic", "close_tab"},
        {"split_view", "Toggle Split View (Dual Pane)", "Navigation", "view-restore-symbolic", "split_view"},
        {"edit_path", "Edit Path Bar Directly", "Navigation", "document-edit-symbolic", "edit_path"},
        {"nav_back", "Go Back in History", "Navigation", "go-previous-symbolic", "nav_back"},
        {"nav_forward", "Go Forward in History", "Navigation", "go-next-symbolic", "nav_forward"},
        {"nav_up", "Go to Parent Directory", "Navigation", "go-up-symbolic", "nav_up"},
        {"nav_home", "Go to Home Folder", "Navigation", "user-home-symbolic", "nav_home"},

        // File Operations
        {"new_folder", "Create New Folder", "File Operations", "folder-new-symbolic", "new_folder"},
        {"new_file", "Create New Document", "File Operations", "document-new-symbolic", ""},
        {"rename_item", "Rename / Bulk Rename Selected", "File Operations", "edit-rename-symbolic", "rename_item"},
        {"copy_items", "Copy Selected Items", "File Operations", "edit-copy-symbolic", "copy_items"},
        {"cut_items", "Cut Selected Items", "File Operations", "edit-cut-symbolic", "cut_items"},
        {"paste_items", "Paste from Clipboard", "File Operations", "edit-paste-symbolic", "paste_items"},
        {"copy_path", "Copy Full File Path", "File Operations", "edit-copy-symbolic", "copy_path"},
        {"trash_items", "Move Selected to Trash", "File Operations", "user-trash-symbolic", "trash_items"},
        {"delete_permanent", "Permanently Delete Selected", "File Operations", "edit-delete-symbolic", "delete_permanent"},
        {"select_all", "Select All Files", "File Operations", "edit-select-all-symbolic", "select_all"},
        {"refresh_view", "Refresh Folder", "File Operations", "view-refresh-symbolic", "refresh_view"},

        // Tools & View
        {"search_files", "Search in Current Directory", "View & Tools", "edit-find-symbolic", "search_files"},
        {"open_terminal", "Open Terminal Here", "View & Tools", "utilities-terminal-symbolic", "open_terminal"},
        {"inspector", "Toggle Inspector Panel", "View & Tools", "dialog-information-symbolic", "inspector"},
        {"quick_preview", "Quick Preview Selected (QuickLook)", "View & Tools", "view-reveal-symbolic", "quick_preview"},
        {"toggle_hidden", "Toggle Show Hidden Files", "View & Tools", "view-conceal-symbolic", "toggle_hidden"},
        {"view_grid", "Switch to Grid View", "View & Tools", "view-grid-symbolic", "view_grid"},
        {"view_list", "Switch to List View", "View & Tools", "view-list-symbolic", "view_list"},
        {"zoom_in", "Zoom In (Bigger Icons)", "View & Tools", "zoom-in-symbolic", "zoom_in"},
        {"zoom_out", "Zoom Out (Smaller Icons)", "View & Tools", "zoom-out-symbolic", "zoom_out"},
        {"zoom_reset", "Reset Zoom to Normal (96px)", "View & Tools", "zoom-original-symbolic", "zoom_reset"},
        {"set_wallpaper", "Set Selected as Wallpaper", "View & Tools", "preferences-desktop-wallpaper-symbolic", ""},
        {"properties", "Show File Properties & Permissions", "View & Tools", "document-properties-symbolic", ""},
        {"preferences", "Open Zenith Preferences & Shortcuts", "Application", "emblem-system-symbolic", "preferences"}
    };
}

static void filter_actions(PaletteContext* ctx, const std::string& query) {
    if (!ctx || !ctx->listbox) return;

    // Clear listbox
    GList* children = gtk_container_get_children(GTK_CONTAINER(ctx->listbox));
    for (GList* iter = children; iter != nullptr; iter = g_list_next(iter)) {
        gtk_widget_destroy(GTK_WIDGET(iter->data));
    }
    g_list_free(children);
    ctx->visible_rows.clear();

    std::string q = query;
    std::transform(q.begin(), q.end(), q.begin(), ::tolower);

    for (const auto& act : ctx->actions) {
        std::string t = act.title;
        std::string c = act.category;
        std::string t_low = t;
        std::string c_low = c;
        std::transform(t_low.begin(), t_low.end(), t_low.begin(), ::tolower);
        std::transform(c_low.begin(), c_low.end(), c_low.begin(), ::tolower);

        if (!q.empty() && t_low.find(q) == std::string::npos && c_low.find(q) == std::string::npos) {
            continue;
        }

        GtkWidget* row = gtk_list_box_row_new();
        gtk_widget_add_css_class(row, "files-palette-row");

        GtkWidget* box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
        gtk_container_set_border_width(GTK_CONTAINER(box), 8);

        GtkWidget* icon = gtk_image_new_from_icon_name(act.icon_name.c_str(), GTK_ICON_SIZE_MENU);
        gtk_box_pack_start(GTK_BOX(box), icon, FALSE, FALSE, 0);

        GtkWidget* lbl_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
        GtkWidget* title_lbl = gtk_label_new(act.title.c_str());
        gtk_label_set_xalign(GTK_LABEL(title_lbl), 0.0f);
        gtk_widget_add_css_class(title_lbl, "files-palette-title");
        gtk_box_pack_start(GTK_BOX(lbl_box), title_lbl, FALSE, FALSE, 0);

        GtkWidget* cat_lbl = gtk_label_new(act.category.c_str());
        gtk_label_set_xalign(GTK_LABEL(cat_lbl), 0.0f);
        gtk_widget_add_css_class(cat_lbl, "files-palette-category");
        gtk_box_pack_start(GTK_BOX(lbl_box), cat_lbl, FALSE, FALSE, 0);

        gtk_box_pack_start(GTK_BOX(box), lbl_box, TRUE, TRUE, 0);

        // Shortcut badge if any
        if (!act.shortcut_id.empty()) {
            std::string badge = FileShortcuts::get_badge_markup(act.shortcut_id);
            if (!badge.empty()) {
                GtkWidget* sc_lbl = gtk_label_new(nullptr);
                gtk_label_set_markup(GTK_LABEL(sc_lbl), badge.c_str());
                gtk_box_pack_end(GTK_BOX(box), sc_lbl, FALSE, FALSE, 0);
            }
        }

        gtk_container_add(GTK_CONTAINER(row), box);

        std::string* act_id = new std::string(act.id);
        g_object_set_data_full(G_OBJECT(row), "action_id", act_id, +[](gpointer d) {
            delete static_cast<std::string*>(d);
        });

        gtk_list_box_insert(GTK_LIST_BOX(ctx->listbox), row, -1);
        ctx->visible_rows.push_back(row);
    }

    if (ctx->visible_rows.empty()) {
        GtkWidget* empty_lbl = gtk_label_new("No matching commands found");
        gtk_widget_add_css_class(empty_lbl, "files-palette-empty");
        gtk_list_box_insert(GTK_LIST_BOX(ctx->listbox), empty_lbl, -1);
    }

    gtk_widget_show_all(ctx->listbox);

    // Select first item by default
    if (!ctx->visible_rows.empty()) {
        gtk_list_box_select_row(GTK_LIST_BOX(ctx->listbox), GTK_LIST_BOX_ROW(ctx->visible_rows[0]));
    }
}

void CommandPalette::show(GtkWindow* parent, ActionHandler handler) {
    auto* ctx = new PaletteContext();
    ctx->handler = std::move(handler);
    ctx->actions = get_default_actions();

    GtkWidget* dialog = gtk_dialog_new();
    if (parent) {
        gtk_window_set_transient_for(GTK_WINDOW(dialog), parent);
        gtk_window_set_modal(GTK_WINDOW(dialog), TRUE);
    }
    ctx->dialog = dialog;

    gtk_window_set_title(GTK_WINDOW(dialog), "Command Palette");
    gtk_window_set_default_size(GTK_WINDOW(dialog), 540, 420);
    gtk_widget_add_css_class(dialog, "zenith-files-dialog");
    gtk_widget_add_css_class(dialog, "files-command-palette");

    GtkWidget* content = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
    gtk_container_set_border_width(GTK_CONTAINER(content), 12);
    gtk_box_set_spacing(GTK_BOX(content), 8);

    // 1. Search Entry
    GtkWidget* search_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_widget_add_css_class(search_box, "files-palette-search-box");

    GtkWidget* entry = gtk_entry_new();
    ctx->entry = entry;
    gtk_entry_set_icon_from_icon_name(GTK_ENTRY(entry), GTK_ENTRY_ICON_PRIMARY, "system-search-symbolic");
    gtk_entry_set_placeholder_text(GTK_ENTRY(entry), "Type a command or action (e.g. Terminal, Split, Tab, Zoom)...");
    gtk_box_pack_start(GTK_BOX(search_box), entry, TRUE, TRUE, 0);

    gtk_box_pack_start(GTK_BOX(content), search_box, FALSE, FALSE, 0);

    // 2. Results List in Scrolled Window
    GtkWidget* scroll = gtk_scrolled_window_new(nullptr, nullptr);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_widget_set_vexpand(scroll, TRUE);

    GtkWidget* listbox = gtk_list_box_new();
    ctx->listbox = listbox;
    gtk_list_box_set_selection_mode(GTK_LIST_BOX(listbox), GTK_SELECTION_SINGLE);
    gtk_widget_add_css_class(listbox, "files-palette-list");
    gtk_container_add(GTK_CONTAINER(scroll), listbox);

    gtk_box_pack_start(GTK_BOX(content), scroll, TRUE, TRUE, 0);

    // Filter typing
    g_signal_connect(entry, "changed", G_CALLBACK(+[](GtkEditable* e, gpointer user_data) {
        auto* c = static_cast<PaletteContext*>(user_data);
        const char* txt = gtk_entry_get_text(GTK_ENTRY(e));
        filter_actions(c, txt ? txt : "");
    }), ctx);

    // Execute selected row helper
    auto execute_selected = [ctx]() {
        GtkListBoxRow* row = gtk_list_box_get_selected_row(GTK_LIST_BOX(ctx->listbox));
        if (row) {
            auto* id_ptr = static_cast<std::string*>(g_object_get_data(G_OBJECT(row), "action_id"));
            if (id_ptr && ctx->handler) {
                std::string chosen_id = *id_ptr;
                gtk_widget_destroy(ctx->dialog);
                ctx->handler(chosen_id);
                delete ctx;
                return;
            }
        }
        gtk_widget_destroy(ctx->dialog);
        delete ctx;
    };

    // Row activated (double click or Enter on row)
    g_signal_connect(listbox, "row-activated", G_CALLBACK(+[](GtkListBox*, GtkListBoxRow* row, gpointer user_data) {
        auto* c = static_cast<PaletteContext*>(user_data);
        if (row) {
            auto* id_ptr = static_cast<std::string*>(g_object_get_data(G_OBJECT(row), "action_id"));
            if (id_ptr && c->handler) {
                std::string chosen_id = *id_ptr;
                gtk_widget_destroy(c->dialog);
                c->handler(chosen_id);
                delete c;
            }
        }
    }), ctx);

    // Key press in entry for Up/Down/Enter/Escape
    g_signal_connect(entry, "key-press-event", G_CALLBACK(+[](GtkWidget*, GdkEventKey* event, gpointer user_data) -> gboolean {
        auto* c = static_cast<PaletteContext*>(user_data);
        if (event->keyval == GDK_KEY_Escape) {
            gtk_widget_destroy(c->dialog);
            delete c;
            return TRUE;
        } else if (event->keyval == GDK_KEY_Return || event->keyval == GDK_KEY_KP_Enter) {
            GtkListBoxRow* row = gtk_list_box_get_selected_row(GTK_LIST_BOX(c->listbox));
            if (row) {
                auto* id_ptr = static_cast<std::string*>(g_object_get_data(G_OBJECT(row), "action_id"));
                if (id_ptr && c->handler) {
                    std::string chosen_id = *id_ptr;
                    gtk_widget_destroy(c->dialog);
                    c->handler(chosen_id);
                    delete c;
                    return TRUE;
                }
            }
        } else if (event->keyval == GDK_KEY_Down) {
            GtkListBoxRow* current = gtk_list_box_get_selected_row(GTK_LIST_BOX(c->listbox));
            int idx = current ? gtk_list_box_row_get_index(current) : -1;
            if (idx + 1 < static_cast<int>(c->visible_rows.size())) {
                gtk_list_box_select_row(GTK_LIST_BOX(c->listbox), GTK_LIST_BOX_ROW(c->visible_rows[idx + 1]));
            }
            return TRUE;
        } else if (event->keyval == GDK_KEY_Up) {
            GtkListBoxRow* current = gtk_list_box_get_selected_row(GTK_LIST_BOX(c->listbox));
            int idx = current ? gtk_list_box_row_get_index(current) : 1;
            if (idx - 1 >= 0 && !c->visible_rows.empty()) {
                gtk_list_box_select_row(GTK_LIST_BOX(c->listbox), GTK_LIST_BOX_ROW(c->visible_rows[idx - 1]));
            }
            return TRUE;
        }
        return FALSE;
    }), ctx);

    filter_actions(ctx, "");

    gtk_widget_show_all(dialog);
    gtk_widget_grab_focus(entry);
}

} // namespace zenith
