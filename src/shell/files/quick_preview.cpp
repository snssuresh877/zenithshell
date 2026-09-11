#include "shell/files/quick_preview.hpp"
#include "shell/files/file_item.hpp"
#include "gtk3_compat.hpp"
#include <gio/gio.h>
#include <filesystem>
#include <fstream>
#include <iostream>

namespace fs = std::filesystem;

namespace zenith {

static GtkWidget* s_active_dialog = nullptr;

bool QuickPreview::is_open() {
    return s_active_dialog != nullptr;
}

void QuickPreview::close() {
    if (s_active_dialog) {
        gtk_widget_destroy(s_active_dialog);
        s_active_dialog = nullptr;
    }
}

void QuickPreview::toggle(GtkWindow* parent, const std::string& file_path) {
    if (file_path.empty()) return;

    if (s_active_dialog) {
        close();
        return;
    }

    std::string filename = fs::path(file_path).filename().string();
    if (filename.empty()) filename = file_path;

    GtkWidget* dialog = gtk_dialog_new();
    if (parent) {
        gtk_window_set_transient_for(GTK_WINDOW(dialog), parent);
        gtk_window_set_modal(GTK_WINDOW(dialog), TRUE);
    }
    gtk_window_set_title(GTK_WINDOW(dialog), filename.c_str());
    s_active_dialog = dialog;

    gtk_window_set_default_size(GTK_WINDOW(dialog), 760, 560);
    gtk_widget_add_css_class(dialog, "zenith-files-dialog");
    gtk_widget_add_css_class(dialog, "files-quick-preview-dialog");

    GtkWidget* content = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
    gtk_container_set_border_width(GTK_CONTAINER(content), 16);
    gtk_box_set_spacing(GTK_BOX(content), 12);

    GFile* gf = g_file_parse_name(file_path.c_str());
    GFileInfo* fi = gf ? g_file_query_info(gf, "standard::content-type", G_FILE_QUERY_INFO_NONE, nullptr, nullptr) : nullptr;
    std::string mime = fi ? (g_file_info_get_content_type(fi) ? g_file_info_get_content_type(fi) : "application/octet-stream") : "application/octet-stream";
    if (fi) g_object_unref(fi);
    if (gf) g_object_unref(gf);

    if (mime.rfind("image/", 0) == 0) {
        // Image preview
        GError* err = nullptr;
        GdkPixbuf* pix = gdk_pixbuf_new_from_file_at_scale(file_path.c_str(), 720, 480, TRUE, &err);
        if (pix) {
            GtkWidget* img = gtk_image_new_from_pixbuf(pix);
            gtk_box_pack_start(GTK_BOX(content), img, TRUE, TRUE, 0);
            g_object_unref(pix);
        } else {
            if (err) g_error_free(err);
            GtkWidget* lbl = gtk_label_new("Unable to load image preview");
            gtk_box_pack_start(GTK_BOX(content), lbl, TRUE, TRUE, 0);
        }
    } else if (mime.rfind("text/", 0) == 0 || mime == "application/json" || mime == "application/javascript" ||
               mime == "application/xml" || mime == "application/x-shellscript" || mime == "application/x-yaml") {
        // Text / Code preview
        GtkWidget* scroll = gtk_scrolled_window_new(nullptr, nullptr);
        gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
        gtk_widget_set_vexpand(scroll, TRUE);

        GtkWidget* text_view = gtk_text_view_new();
        gtk_text_view_set_editable(GTK_TEXT_VIEW(text_view), FALSE);
        gtk_text_view_set_cursor_visible(GTK_TEXT_VIEW(text_view), FALSE);
        gtk_text_view_set_monospace(GTK_TEXT_VIEW(text_view), TRUE);
        gtk_widget_add_css_class(text_view, "files-preview-code");

        GtkTextBuffer* buf = gtk_text_view_get_buffer(GTK_TEXT_VIEW(text_view));

        // Read up to 64 KB
        std::ifstream in(file_path);
        if (in.is_open()) {
            std::string line;
            std::string full_text;
            int count = 0;
            while (std::getline(in, line) && count++ < 600) {
                full_text += line + "\n";
            }
            gtk_text_buffer_set_text(buf, full_text.c_str(), -1);
        }

        gtk_container_add(GTK_CONTAINER(scroll), text_view);
        gtk_box_pack_start(GTK_BOX(content), scroll, TRUE, TRUE, 0);
    } else {
        // Generic File / Media info
        GtkWidget* box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 16);
        gtk_widget_set_valign(box, GTK_ALIGN_CENTER);

        GtkWidget* ico = gtk_image_new_from_icon_name(fs::is_directory(file_path) ? "folder" : "text-x-generic", GTK_ICON_SIZE_DIALOG);
        gtk_widget_set_size_request(ico, 128, 128);
        gtk_box_pack_start(GTK_BOX(box), ico, FALSE, FALSE, 0);

        GtkWidget* name_lbl = gtk_label_new(filename.c_str());
        gtk_widget_add_css_class(name_lbl, "files-prop-title");
        gtk_box_pack_start(GTK_BOX(box), name_lbl, FALSE, FALSE, 0);

        GtkWidget* mime_lbl = gtk_label_new(mime.c_str());
        gtk_widget_add_css_class(mime_lbl, "files-prop-subtitle");
        gtk_box_pack_start(GTK_BOX(box), mime_lbl, FALSE, FALSE, 0);

        std::error_code ec;
        if (fs::is_regular_file(file_path, ec)) {
            std::string sz = FileItem::format_size(fs::file_size(file_path, ec));
            GtkWidget* sz_lbl = gtk_label_new(sz.c_str());
            gtk_widget_add_css_class(sz_lbl, "files-prop-val");
            gtk_box_pack_start(GTK_BOX(box), sz_lbl, FALSE, FALSE, 0);
        }

        gtk_box_pack_start(GTK_BOX(content), box, TRUE, TRUE, 0);
    }

    // Dismiss on Space or Escape
    g_signal_connect(dialog, "key-press-event", G_CALLBACK(+[](GtkWidget*, GdkEventKey* event, gpointer) -> gboolean {
        if (event->keyval == GDK_KEY_space || event->keyval == GDK_KEY_Escape) {
            QuickPreview::close();
            return TRUE;
        }
        return FALSE;
    }), nullptr);

    g_signal_connect(dialog, "destroy", G_CALLBACK(+[](GtkWidget*, gpointer) {
        s_active_dialog = nullptr;
    }), nullptr);

    gtk_widget_show_all(dialog);
}

} // namespace zenith
