#include "shell/files/path_bar_widget.hpp"
#include "gtk3_compat.hpp"
#include <filesystem>
#include <vector>
#include <iostream>

namespace fs = std::filesystem;

namespace zenith {

struct PathBarData {
    PathBarWidget::NavigateCallback on_navigate;
    std::string current_path;
    GtkWidget* breadcrumb_box{nullptr};
    GtkWidget* entry{nullptr};
    GtkWidget* stack{nullptr};
    bool in_edit_mode{false};
};

static void rebuild_breadcrumbs(PathBarData* data) {
    if (!data || !data->breadcrumb_box) return;

    // Clear existing buttons
    GList* children = gtk_container_get_children(GTK_CONTAINER(data->breadcrumb_box));
    for (GList* iter = children; iter != nullptr; iter = g_list_next(iter)) {
        gtk_widget_destroy(GTK_WIDGET(iter->data));
    }
    g_list_free(children);

    std::string p_str = data->current_path;
    if (p_str.empty()) p_str = "/";

    std::string home_dir = g_get_home_dir() ? g_get_home_dir() : "/home";

    // Deconstruct path into segments
    struct Segment {
        std::string label;
        std::string full_path;
    };
    std::vector<Segment> segments;

    if (p_str == home_dir || p_str.rfind(home_dir + "/", 0) == 0) {
        // Under home directory: Root segment is ~
        segments.push_back({"~", home_dir});
        std::string sub = p_str.substr(home_dir.length());
        fs::path rel(sub);
        std::string accum = home_dir;
        for (const auto& part : rel) {
            std::string s = part.string();
            if (s.empty() || s == "/") continue;
            accum += "/" + s;
            segments.push_back({s, accum});
        }
    } else {
        // System root
        segments.push_back({"File System", "/"});
        fs::path root(p_str);
        std::string accum = "";
        for (const auto& part : root) {
            std::string s = part.string();
            if (s.empty() || s == "/") continue;
            accum += "/" + s;
            segments.push_back({s, accum});
        }
    }

    size_t total = segments.size();
    for (size_t i = 0; i < total; ++i) {
        const auto& seg = segments[i];
        bool is_last = (i == total - 1);

        GtkWidget* btn = gtk_button_new();
        gtk_widget_add_css_class(btn, "files-pathbar-btn");
        if (is_last) {
            gtk_widget_add_css_class(btn, "active");
        }

        GtkWidget* btn_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
        if (seg.label == "~") {
            GtkWidget* ico = gtk_label_new("");
            gtk_widget_add_css_class(ico, "files-pathbar-icon");
            gtk_box_pack_start(GTK_BOX(btn_box), ico, FALSE, FALSE, 0);
        } else if (seg.label == "File System") {
            GtkWidget* ico = gtk_label_new("󰋊");
            gtk_widget_add_css_class(ico, "files-pathbar-icon");
            gtk_box_pack_start(GTK_BOX(btn_box), ico, FALSE, FALSE, 0);
        }

        GtkWidget* lbl = gtk_label_new(seg.label.c_str());
        gtk_widget_add_css_class(lbl, "files-pathbar-label");
        gtk_box_pack_start(GTK_BOX(btn_box), lbl, FALSE, FALSE, 0);

        gtk_container_add(GTK_CONTAINER(btn), btn_box);

        std::string* dest = new std::string(seg.full_path);
        g_object_set_data_full(G_OBJECT(btn), "target_path", dest, +[](gpointer d) {
            delete static_cast<std::string*>(d);
        });

        g_signal_connect(btn, "clicked", G_CALLBACK(+[](GtkButton* b, gpointer user_data) {
            auto* d = static_cast<PathBarData*>(user_data);
            auto* target = static_cast<std::string*>(g_object_get_data(G_OBJECT(b), "target_path"));
            if (d && d->on_navigate && target) {
                d->on_navigate(*target);
            }
        }), data);

        gtk_box_pack_start(GTK_BOX(data->breadcrumb_box), btn, FALSE, FALSE, 0);

        // Add chevron separator if not last
        if (!is_last) {
            GtkWidget* chevron = gtk_label_new("");
            gtk_widget_add_css_class(chevron, "files-pathbar-sep");
            gtk_box_pack_start(GTK_BOX(data->breadcrumb_box), chevron, FALSE, FALSE, 0);
        }
    }

    // Trailing edit button to click and type path directly
    GtkWidget* edit_trigger = gtk_button_new();
    gtk_widget_add_css_class(edit_trigger, "files-pathbar-edit-btn");
    GtkWidget* edit_icon = gtk_label_new("󰏫");
    gtk_container_add(GTK_CONTAINER(edit_trigger), edit_icon);
    gtk_widget_set_tooltip_text(edit_trigger, "Edit path directly (Ctrl+L)");

    g_signal_connect(edit_trigger, "clicked", G_CALLBACK(+[](GtkButton*, gpointer user_data) {
        auto* d = static_cast<PathBarData*>(user_data);
        if (d && d->stack) {
            PathBarWidget::enter_edit_mode(d->stack);
        }
    }), data);

    gtk_box_pack_start(GTK_BOX(data->breadcrumb_box), edit_trigger, FALSE, FALSE, 0);

    gtk_widget_show_all(data->breadcrumb_box);
}

GtkWidget* PathBarWidget::create(NavigateCallback on_navigate) {
    auto* data = new PathBarData();
    data->on_navigate = std::move(on_navigate);
    data->current_path = g_get_home_dir() ? g_get_home_dir() : "/";

    GtkWidget* stack = gtk_stack_new();
    gtk_stack_set_transition_type(GTK_STACK(stack), GTK_STACK_TRANSITION_TYPE_CROSSFADE);
    gtk_stack_set_transition_duration(GTK_STACK(stack), 120);
    data->stack = stack;

    // 1. Breadcrumb Mode
    GtkWidget* scroll = gtk_scrolled_window_new(nullptr, nullptr);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll), GTK_POLICY_AUTOMATIC, GTK_POLICY_NEVER);
    gtk_widget_add_css_class(scroll, "files-pathbar-scroll");

    GtkWidget* b_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 2);
    gtk_widget_add_css_class(b_box, "files-pathbar-container");
    data->breadcrumb_box = b_box;
    gtk_container_add(GTK_CONTAINER(scroll), b_box);

    gtk_stack_add_named(GTK_STACK(stack), scroll, "breadcrumbs");

    // 2. Direct Text Entry Mode
    GtkWidget* entry = gtk_entry_new();
    gtk_widget_add_css_class(entry, "files-pathbar-entry");
    gtk_entry_set_icon_from_icon_name(GTK_ENTRY(entry), GTK_ENTRY_ICON_PRIMARY, "folder");
    gtk_entry_set_icon_from_icon_name(GTK_ENTRY(entry), GTK_ENTRY_ICON_SECONDARY, "window-close");
    data->entry = entry;

    g_signal_connect(entry, "activate", G_CALLBACK(+[](GtkEntry* e, gpointer user_data) {
        auto* d = static_cast<PathBarData*>(user_data);
        const char* txt = gtk_entry_get_text(e);
        if (d && d->on_navigate && txt && *txt) {
            std::string t = txt;
            if (!t.empty() && t[0] == '~') {
                t = std::string(g_get_home_dir()) + t.substr(1);
            }
            d->on_navigate(t);
            PathBarWidget::exit_edit_mode(d->stack);
        }
    }), data);

    g_signal_connect(entry, "icon-release", G_CALLBACK(+[](GtkEntry*, GtkEntryIconPosition pos, GdkEvent*, gpointer user_data) {
        if (pos == GTK_ENTRY_ICON_SECONDARY) {
            auto* d = static_cast<PathBarData*>(user_data);
            if (d && d->stack) {
                PathBarWidget::exit_edit_mode(d->stack);
            }
        }
    }), data);

    g_signal_connect(entry, "key-press-event", G_CALLBACK(+[](GtkWidget*, GdkEventKey* event, gpointer user_data) -> gboolean {
        if (event->keyval == GDK_KEY_Escape) {
            auto* d = static_cast<PathBarData*>(user_data);
            if (d && d->stack) {
                PathBarWidget::exit_edit_mode(d->stack);
                return TRUE;
            }
        }
        return FALSE;
    }), data);

    gtk_stack_add_named(GTK_STACK(stack), entry, "entry");

    g_object_set_data_full(G_OBJECT(stack), "pathbar_data", data, +[](gpointer d) {
        delete static_cast<PathBarData*>(d);
    });

    rebuild_breadcrumbs(data);
    return stack;
}

void PathBarWidget::set_path(GtkWidget* widget, const std::string& path) {
    if (!widget) return;
    auto* data = static_cast<PathBarData*>(g_object_get_data(G_OBJECT(widget), "pathbar_data"));
    if (!data) return;

    data->current_path = path;
    if (data->entry) {
        gtk_entry_set_text(GTK_ENTRY(data->entry), path.c_str());
    }
    rebuild_breadcrumbs(data);
}

std::string PathBarWidget::get_path(GtkWidget* widget) {
    if (!widget) return "";
    auto* data = static_cast<PathBarData*>(g_object_get_data(G_OBJECT(widget), "pathbar_data"));
    return data ? data->current_path : "";
}

void PathBarWidget::enter_edit_mode(GtkWidget* widget) {
    if (!widget) return;
    auto* data = static_cast<PathBarData*>(g_object_get_data(G_OBJECT(widget), "pathbar_data"));
    if (!data || !data->stack || !data->entry) return;

    data->in_edit_mode = true;
    gtk_entry_set_text(GTK_ENTRY(data->entry), data->current_path.c_str());
    gtk_stack_set_visible_child_name(GTK_STACK(data->stack), "entry");
    gtk_widget_grab_focus(data->entry);
}

void PathBarWidget::exit_edit_mode(GtkWidget* widget) {
    if (!widget) return;
    auto* data = static_cast<PathBarData*>(g_object_get_data(G_OBJECT(widget), "pathbar_data"));
    if (!data || !data->stack) return;

    data->in_edit_mode = false;
    gtk_stack_set_visible_child_name(GTK_STACK(data->stack), "breadcrumbs");
}

} // namespace zenith
