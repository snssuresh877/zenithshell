#include "shell/files/file_preferences_dialog.hpp"
#include "shell/files/file_shortcuts.hpp"
#include "shell/files/file_view_widget.hpp"
#include "gtk3_compat.hpp"
#include <vector>
#include <iostream>

namespace zenith {

// Forward declaration of internal state
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

static void show_rebind_dialog(GtkWindow* parent, const ShortcutDef& def, std::function<void()> on_rebound) {
    GtkWidget* dialog = gtk_dialog_new_with_buttons(
        "Rebind Shortcut",
        parent,
        static_cast<GtkDialogFlags>(GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT),
        "_Cancel", GTK_RESPONSE_CANCEL,
        nullptr
    );
    gtk_window_set_default_size(GTK_WINDOW(dialog), 380, 180);
    gtk_widget_add_css_class(dialog, "zenith-files-dialog");

    GtkWidget* content = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
    gtk_container_set_border_width(GTK_CONTAINER(content), 20);
    gtk_box_set_spacing(GTK_BOX(content), 12);

    GtkWidget* title = gtk_label_new(("Rebind: <b>" + def.label + "</b>").c_str());
    gtk_label_set_use_markup(GTK_LABEL(title), TRUE);
    gtk_widget_add_css_class(title, "files-prop-title");
    gtk_box_pack_start(GTK_BOX(content), title, FALSE, FALSE, 0);

    GtkWidget* prompt = gtk_label_new("Press the new key combination now…\n(e.g. Ctrl+Alt+T, F4, Space, etc.)");
    gtk_label_set_xalign(GTK_LABEL(prompt), 0.5f);
    gtk_label_set_justify(GTK_LABEL(prompt), GTK_JUSTIFY_CENTER);
    gtk_widget_add_css_class(prompt, "files-prop-subtitle");
    gtk_box_pack_start(GTK_BOX(content), prompt, FALSE, FALSE, 8);

    struct KeyCaptureCtx {
        std::string action_id;
        GtkWidget* dialog;
        std::function<void()> callback;
    };
    auto* ctx = new KeyCaptureCtx{def.id, dialog, std::move(on_rebound)};

    g_signal_connect(dialog, "key-press-event", G_CALLBACK(+[](GtkWidget*, GdkEventKey* event, gpointer user_data) -> gboolean {
        auto* c = static_cast<KeyCaptureCtx*>(user_data);
        guint key = event->keyval;
        guint state = event->state & gtk_accelerator_get_default_mod_mask();

        // Ignore standalone modifier presses
        if (key == GDK_KEY_Control_L || key == GDK_KEY_Control_R ||
            key == GDK_KEY_Alt_L || key == GDK_KEY_Alt_R ||
            key == GDK_KEY_Shift_L || key == GDK_KEY_Shift_R ||
            key == GDK_KEY_Super_L || key == GDK_KEY_Super_R) {
            return TRUE;
        }

        if (key == GDK_KEY_Escape) {
            gtk_widget_destroy(c->dialog);
            delete c;
            return TRUE;
        }

        FileShortcuts::update_shortcut(c->action_id, key, state);
        if (c->callback) c->callback();

        gtk_widget_destroy(c->dialog);
        delete c;
        return TRUE;
    }), ctx);

    gtk_widget_show_all(dialog);
}

void FilePreferencesDialog::show(FileManagerState* state) {
    if (!state || !state->window) return;

    GtkWidget* dialog = gtk_dialog_new_with_buttons(
        "Zenith Files — Preferences & Configuration",
        GTK_WINDOW(state->window),
        static_cast<GtkDialogFlags>(GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT),
        "_Close", GTK_RESPONSE_CLOSE,
        nullptr
    );
    gtk_window_set_default_size(GTK_WINDOW(dialog), 660, 540);
    gtk_widget_add_css_class(dialog, "zenith-files-dialog");

    GtkWidget* content = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
    gtk_container_set_border_width(GTK_CONTAINER(content), 16);
    gtk_box_set_spacing(GTK_BOX(content), 12);

    GtkWidget* notebook = gtk_notebook_new();
    gtk_widget_add_css_class(notebook, "files-settings-notebook");

    // ── 1. General Tab ────────────────────────────────────────────────────────
    {
        GtkWidget* vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 14);
        gtk_container_set_border_width(GTK_CONTAINER(vbox), 16);

        GtkWidget* sec1 = gtk_label_new("<b>Startup & Behavior</b>");
        gtk_label_set_use_markup(GTK_LABEL(sec1), TRUE);
        gtk_label_set_xalign(GTK_LABEL(sec1), 0.0f);
        gtk_widget_add_css_class(sec1, "files-prop-title");
        gtk_box_pack_start(GTK_BOX(vbox), sec1, FALSE, FALSE, 0);

        GtkWidget* chk_del = gtk_check_button_new_with_label("Ask confirmation before permanently deleting files (Shift+Del)");
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(chk_del), TRUE);
        gtk_box_pack_start(GTK_BOX(vbox), chk_del, FALSE, FALSE, 0);

        GtkWidget* chk_folder_first = gtk_check_button_new_with_label("Sort folders before regular files");
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(chk_folder_first), TRUE);
        gtk_box_pack_start(GTK_BOX(vbox), chk_folder_first, FALSE, FALSE, 0);

        gtk_box_pack_start(GTK_BOX(vbox), gtk_separator_new(GTK_ORIENTATION_HORIZONTAL), FALSE, FALSE, 4);

        GtkWidget* sec2 = gtk_label_new("<b>Terminal Integration</b>");
        gtk_label_set_use_markup(GTK_LABEL(sec2), TRUE);
        gtk_label_set_xalign(GTK_LABEL(sec2), 0.0f);
        gtk_widget_add_css_class(sec2, "files-prop-title");
        gtk_box_pack_start(GTK_BOX(vbox), sec2, FALSE, FALSE, 0);

        GtkWidget* term_desc = gtk_label_new("Default terminal emulator command for 'Open Terminal Here' (F4):");
        gtk_label_set_xalign(GTK_LABEL(term_desc), 0.0f);
        gtk_widget_add_css_class(term_desc, "files-prop-subtitle");
        gtk_box_pack_start(GTK_BOX(vbox), term_desc, FALSE, FALSE, 0);

        GtkWidget* term_entry = gtk_entry_new();
        gtk_entry_set_text(GTK_ENTRY(term_entry), "foot --working-directory=\"%d\"");
        gtk_box_pack_start(GTK_BOX(vbox), term_entry, FALSE, FALSE, 0);

        gtk_notebook_append_page(GTK_NOTEBOOK(notebook), vbox, gtk_label_new("General"));
    }

    // ── 2. Appearance & Sizing Tab ────────────────────────────────────────────
    {
        GtkWidget* vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 14);
        gtk_container_set_border_width(GTK_CONTAINER(vbox), 16);

        GtkWidget* size_header = gtk_label_new("<b>Grid View Folder & Icon Size</b>");
        gtk_label_set_use_markup(GTK_LABEL(size_header), TRUE);
        gtk_label_set_xalign(GTK_LABEL(size_header), 0.0f);
        gtk_widget_add_css_class(size_header, "files-prop-title");
        gtk_box_pack_start(GTK_BOX(vbox), size_header, FALSE, FALSE, 0);

        GtkWidget* size_desc = gtk_label_new("Adjust size dynamically (also Ctrl+ScrollWheel, Ctrl++ / Ctrl+-):");
        gtk_label_set_xalign(GTK_LABEL(size_desc), 0.0f);
        gtk_widget_add_css_class(size_desc, "files-prop-subtitle");
        gtk_box_pack_start(GTK_BOX(vbox), size_desc, FALSE, FALSE, 0);

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
        gtk_box_pack_start(GTK_BOX(vbox), scale, FALSE, FALSE, 6);

        // Zoom buttons row
        GtkWidget* zoom_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
        GtkWidget* btn_smaller = gtk_button_new_with_label("➖ Smaller (Ctrl+-)");
        gtk_widget_add_css_class(btn_smaller, "files-btn-tool");
        g_signal_connect_swapped(btn_smaller, "clicked", G_CALLBACK(+[](FileManagerState* s) {
            for (auto& tab : s->tabs) {
                FileViewWidget::zoom_out(tab->left_pane.file_view);
                if (tab->is_dual) FileViewWidget::zoom_out(tab->right_pane.file_view);
            }
        }), state);
        gtk_box_pack_start(GTK_BOX(zoom_box), btn_smaller, FALSE, FALSE, 0);

        GtkWidget* btn_reset = gtk_button_new_with_label("↺ Reset (Ctrl+0)");
        gtk_widget_add_css_class(btn_reset, "files-btn-tool");
        g_signal_connect_swapped(btn_reset, "clicked", G_CALLBACK(+[](FileManagerState* s) {
            for (auto& tab : s->tabs) {
                FileViewWidget::zoom_reset(tab->left_pane.file_view);
                if (tab->is_dual) FileViewWidget::zoom_reset(tab->right_pane.file_view);
            }
        }), state);
        gtk_box_pack_start(GTK_BOX(zoom_box), btn_reset, FALSE, FALSE, 0);

        GtkWidget* btn_larger = gtk_button_new_with_label("➕ Larger (Ctrl++)");
        gtk_widget_add_css_class(btn_larger, "files-btn-tool");
        g_signal_connect_swapped(btn_larger, "clicked", G_CALLBACK(+[](FileManagerState* s) {
            for (auto& tab : s->tabs) {
                FileViewWidget::zoom_in(tab->left_pane.file_view);
                if (tab->is_dual) FileViewWidget::zoom_in(tab->right_pane.file_view);
            }
        }), state);
        gtk_box_pack_start(GTK_BOX(zoom_box), btn_larger, FALSE, FALSE, 0);
        gtk_box_pack_start(GTK_BOX(vbox), zoom_box, FALSE, FALSE, 0);

        gtk_box_pack_start(GTK_BOX(vbox), gtk_separator_new(GTK_ORIENTATION_HORIZONTAL), FALSE, FALSE, 6);

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
        gtk_box_pack_start(GTK_BOX(vbox), chk_hidden, FALSE, FALSE, 0);

        gtk_notebook_append_page(GTK_NOTEBOOK(notebook), vbox, gtk_label_new("Appearance"));
    }

    // ── 3. Performance Tab ────────────────────────────────────────────────────
    {
        GtkWidget* vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 14);
        gtk_container_set_border_width(GTK_CONTAINER(vbox), 16);

        GtkWidget* perf_hdr = gtk_label_new("<b>Performance & Background Services</b>");
        gtk_label_set_use_markup(GTK_LABEL(perf_hdr), TRUE);
        gtk_label_set_xalign(GTK_LABEL(perf_hdr), 0.0f);
        gtk_widget_add_css_class(perf_hdr, "files-prop-title");
        gtk_box_pack_start(GTK_BOX(vbox), perf_hdr, FALSE, FALSE, 0);

        GtkWidget* chk_thumbs = gtk_check_button_new_with_label("Enable multi-threaded background thumbnail generation (Freedesktop Cache)");
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(chk_thumbs), TRUE);
        gtk_box_pack_start(GTK_BOX(vbox), chk_thumbs, FALSE, FALSE, 0);

        GtkWidget* chk_monitor = gtk_check_button_new_with_label("Real-time live directory monitoring (GFileMonitor)");
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(chk_monitor), TRUE);
        gtk_box_pack_start(GTK_BOX(vbox), chk_monitor, FALSE, FALSE, 0);

        GtkWidget* chk_anim = gtk_check_button_new_with_label("Smooth UI transitions and cross-fades");
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(chk_anim), TRUE);
        gtk_box_pack_start(GTK_BOX(vbox), chk_anim, FALSE, FALSE, 0);

        gtk_notebook_append_page(GTK_NOTEBOOK(notebook), vbox, gtk_label_new("Performance"));
    }

    // ── 4. Customizable Shortcuts Tab ─────────────────────────────────────────
    {
        GtkWidget* scroll = gtk_scrolled_window_new(nullptr, nullptr);
        gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);

        GtkWidget* vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
        gtk_container_set_border_width(GTK_CONTAINER(vbox), 16);

        GtkWidget* tip = gtk_label_new("💡 Click on any shortcut below to rebind it to your preferred keys.");
        gtk_label_set_xalign(GTK_LABEL(tip), 0.0f);
        gtk_widget_add_css_class(tip, "files-prop-subtitle");
        gtk_box_pack_start(GTK_BOX(vbox), tip, FALSE, FALSE, 0);

        GtkWidget* list_container = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
        gtk_box_pack_start(GTK_BOX(vbox), list_container, TRUE, TRUE, 0);

        auto populate_shortcuts_list = [list_container, dialog](auto& self) -> void {
            GList* children = gtk_container_get_children(GTK_CONTAINER(list_container));
            for (GList* it = children; it != nullptr; it = g_list_next(it)) {
                gtk_widget_destroy(GTK_WIDGET(it->data));
            }
            g_list_free(children);

            const auto& all = FileShortcuts::get_all();
            std::string last_cat;

            for (const auto& s : all) {
                if (s.category != last_cat) {
                    last_cat = s.category;
                    GtkWidget* cat_hdr = gtk_label_new(("<b>" + last_cat + "</b>").c_str());
                    gtk_label_set_use_markup(GTK_LABEL(cat_hdr), TRUE);
                    gtk_label_set_xalign(GTK_LABEL(cat_hdr), 0.0f);
                    gtk_widget_add_css_class(cat_hdr, "files-prop-title");
                    gtk_box_pack_start(GTK_BOX(list_container), cat_hdr, FALSE, FALSE, 4);
                }

                GtkWidget* row_btn = gtk_button_new();
                gtk_widget_add_css_class(row_btn, "files-btn-nav");

                GtkWidget* box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
                gtk_container_set_border_width(GTK_CONTAINER(box), 6);

                GtkWidget* lbl = gtk_label_new(s.label.c_str());
                gtk_label_set_xalign(GTK_LABEL(lbl), 0.0f);
                gtk_widget_add_css_class(lbl, "files-prop-val");
                gtk_box_pack_start(GTK_BOX(box), lbl, TRUE, TRUE, 0);

                std::string badge = FileShortcuts::format_badge_markup(s.current_keyval(), s.current_modifiers());
                GtkWidget* badge_lbl = gtk_label_new(nullptr);
                gtk_label_set_markup(GTK_LABEL(badge_lbl), badge.c_str());
                gtk_widget_add_css_class(badge_lbl, "files-prop-key");
                gtk_box_pack_end(GTK_BOX(box), badge_lbl, FALSE, FALSE, 0);

                gtk_container_add(GTK_CONTAINER(row_btn), box);

                struct RebindBtnCtx {
                    ShortcutDef s;
                    GtkWindow* parent;
                    std::function<void()> refresh_fn;
                };
                auto* cd = new RebindBtnCtx{s, GTK_WINDOW(dialog), [&self]() { self(self); }};
                auto rebind_click_cb = +[](GtkButton*, gpointer user_data) {
                    auto* c = static_cast<RebindBtnCtx*>(user_data);
                    show_rebind_dialog(c->parent, c->s, c->refresh_fn);
                };
                g_signal_connect_data(row_btn, "clicked", G_CALLBACK(rebind_click_cb), cd, [](gpointer d, GClosure*) { delete static_cast<RebindBtnCtx*>(d); }, static_cast<GConnectFlags>(0));

                gtk_box_pack_start(GTK_BOX(list_container), row_btn, FALSE, FALSE, 2);
            }

            // Reset defaults button
            GtkWidget* reset_btn = gtk_button_new_with_label("↺ Reset All Shortcuts to Defaults");
            gtk_widget_add_css_class(reset_btn, "files-btn-tool");
            auto* refresh_ptr = new std::function<void()>([&self]() { self(self); });
            auto reset_click_cb = +[](GtkButton*, gpointer user_data) {
                auto* rp = static_cast<std::function<void()>*>(user_data);
                FileShortcuts::reset_to_defaults();
                if (*rp) (*rp)();
            };
            g_signal_connect_data(reset_btn, "clicked", G_CALLBACK(reset_click_cb), refresh_ptr, [](gpointer d, GClosure*) { delete static_cast<std::function<void()>*>(d); }, static_cast<GConnectFlags>(0));

            gtk_box_pack_start(GTK_BOX(list_container), reset_btn, FALSE, FALSE, 12);
            gtk_widget_show_all(list_container);
        };

        populate_shortcuts_list(populate_shortcuts_list);

        gtk_container_add(GTK_CONTAINER(scroll), vbox);
        gtk_notebook_append_page(GTK_NOTEBOOK(notebook), scroll, gtk_label_new("Shortcuts"));
    }

    gtk_box_pack_start(GTK_BOX(content), notebook, TRUE, TRUE, 0);

    gtk_widget_show_all(dialog);
    gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);
}

} // namespace zenith
