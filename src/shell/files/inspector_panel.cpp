#include "shell/files/inspector_panel.hpp"
#include "shell/files/file_item.hpp"
#include <sys/stat.h>
#include <pwd.h>
#include <grp.h>
#include <filesystem>
#include <thread>
#include <atomic>
#include <glib.h>
#include <array>
#include <memory>

namespace fs = std::filesystem;

namespace zenith {

struct InspectorData {
    GtkWidget* root_box;
    std::function<void()> on_close;

    GtkWidget* scroll_window;
    GtkWidget* scroll_content;
    
    // Top Preview Block
    GtkWidget* icon_preview;
    GtkWidget* title_label;
    GtkWidget* type_label;

    // Dynamic Sections Container
    GtkWidget* dynamic_box;

    // Sticky Actions Block
    GtkWidget* actions_box;
    GtkWidget* btn_open;
    GtkWidget* btn_term;
    GtkWidget* btn_copy_path;
    GtkWidget* btn_more;
    GtkWidget* pop_box;

    std::string current_path;
    std::vector<std::string> selection;
    std::atomic<uint64_t> calculation_gen{0};
};

static std::string calculate_file_checksum(const std::string& path, GChecksumType type) {
    GChecksum* checksum = g_checksum_new(type);
    FILE* f = fopen(path.c_str(), "rb");
    if (!f) {
        g_checksum_free(checksum);
        return "Error reading file";
    }

    char buffer[65536];
    size_t bytes_read;
    while ((bytes_read = fread(buffer, 1, sizeof(buffer), f)) > 0) {
        g_checksum_update(checksum, (const guchar*)buffer, bytes_read);
    }
    fclose(f);

    std::string result = g_checksum_get_string(checksum);
    g_checksum_free(checksum);
    return result;
}

static std::string format_perm_detailed(mode_t mode, int shift) {
    bool r = (mode & (S_IRUSR >> shift));
    bool w = (mode & (S_IWUSR >> shift));
    bool x = (mode & (S_IXUSR >> shift));
    std::string res = "";
    if (r) res += "Read ";
    if (w) res += "Write ";
    if (x) res += "Execute";
    if (res.empty()) res = "None";
    return res;
}

static std::string exec_cmd(const char* cmd) {
    std::array<char, 128> buffer;
    std::string result;
    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd, "r"), pclose);
    if (!pipe) return "";
    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
        result += buffer.data();
    }
    if (!result.empty() && result.back() == '\n') result.pop_back();
    return result;
}

static GtkWidget* create_row(const char* label_text, const char* value_text, const char* icon_name = nullptr) {
    GtkWidget* hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    if (icon_name) {
        GtkWidget* img = gtk_image_new_from_icon_name(icon_name, GTK_ICON_SIZE_MENU);
        gtk_box_pack_start(GTK_BOX(hbox), img, FALSE, FALSE, 0);
    }
    
    GtkWidget* lbl = gtk_label_new(label_text);
    gtk_label_set_xalign(GTK_LABEL(lbl), 0.0);
    gtk_style_context_add_class(gtk_widget_get_style_context(lbl), "files-prop-lbl");
    gtk_box_pack_start(GTK_BOX(hbox), lbl, FALSE, FALSE, 0);

    GtkWidget* val_lbl = gtk_label_new(value_text);
    gtk_label_set_xalign(GTK_LABEL(val_lbl), 1.0);
    gtk_label_set_ellipsize(GTK_LABEL(val_lbl), PANGO_ELLIPSIZE_START);
    gtk_style_context_add_class(gtk_widget_get_style_context(val_lbl), "files-prop-val");
    gtk_box_pack_end(GTK_BOX(hbox), val_lbl, TRUE, TRUE, 0);
    return hbox;
}

static GtkWidget* create_row_widget(const char* label_text, GtkWidget* val_widget) {
    GtkWidget* hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    GtkWidget* lbl = gtk_label_new(label_text);
    gtk_label_set_xalign(GTK_LABEL(lbl), 0.0);
    gtk_style_context_add_class(gtk_widget_get_style_context(lbl), "files-prop-lbl");
    gtk_box_pack_start(GTK_BOX(hbox), lbl, FALSE, FALSE, 0);
    gtk_box_pack_end(GTK_BOX(hbox), val_widget, TRUE, TRUE, 0);
    return hbox;
}

static GtkWidget* create_section(const char* title) {
    GtkWidget* vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    gtk_container_set_border_width(GTK_CONTAINER(vbox), 8);
    
    if (title) {
        GtkWidget* hdr = gtk_label_new(title);
        gtk_label_set_xalign(GTK_LABEL(hdr), 0.0);
        gtk_style_context_add_class(gtk_widget_get_style_context(hdr), "files-prop-title"); 
        gtk_box_pack_start(GTK_BOX(vbox), hdr, FALSE, FALSE, 4);
        
        // Separator removed for cleaner modern look
    }
    return vbox;
}

static GtkWidget* create_action_btn(const char* label, const char* icon) {
    GtkWidget* btn = gtk_button_new();
    GtkWidget* box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_container_set_border_width(GTK_CONTAINER(box), 4);
    gtk_box_pack_start(GTK_BOX(box), gtk_image_new_from_icon_name(icon, GTK_ICON_SIZE_BUTTON), FALSE, FALSE, 0);
    GtkWidget* l = gtk_label_new(label);
    gtk_label_set_xalign(GTK_LABEL(l), 0.0);
    gtk_box_pack_start(GTK_BOX(box), l, TRUE, TRUE, 0);
    gtk_container_add(GTK_CONTAINER(btn), box);
    return btn;
}

GtkWidget* InspectorPanel::create(CloseCallback on_close) {
    auto* data = new InspectorData();
    data->on_close = on_close;

    GtkWidget* root = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_size_request(root, 340, -1);
    gtk_style_context_add_class(gtk_widget_get_style_context(root), "files-inspector-panel");
    data->root_box = root;

    // Header
    GtkWidget* header = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_container_set_border_width(GTK_CONTAINER(header), 8);
    gtk_box_pack_start(GTK_BOX(root), header, FALSE, FALSE, 0);

    GtkWidget* info_icon = gtk_image_new_from_icon_name("dialog-information-symbolic", GTK_ICON_SIZE_MENU);
    gtk_box_pack_start(GTK_BOX(header), info_icon, FALSE, FALSE, 0);
    GtkWidget* title = gtk_label_new("Inspector");
    gtk_style_context_add_class(gtk_widget_get_style_context(title), "files-inspector-title");
    gtk_box_pack_start(GTK_BOX(header), title, TRUE, TRUE, 0);

    GtkWidget* close_btn = gtk_button_new_from_icon_name("window-close-symbolic", GTK_ICON_SIZE_BUTTON);
    gtk_button_set_relief(GTK_BUTTON(close_btn), GTK_RELIEF_NONE);
    g_signal_connect(close_btn, "clicked", G_CALLBACK(+[](GtkButton*, gpointer user_data) {
        auto* d = static_cast<InspectorData*>(user_data);
        if (d->on_close) d->on_close();
    }), data);
    gtk_box_pack_end(GTK_BOX(header), close_btn, FALSE, FALSE, 0);

    // Scrollable Region
    data->scroll_window = gtk_scrolled_window_new(nullptr, nullptr);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(data->scroll_window), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_box_pack_start(GTK_BOX(root), data->scroll_window, TRUE, TRUE, 0);

    data->scroll_content = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_container_set_border_width(GTK_CONTAINER(data->scroll_content), 12);
    gtk_container_add(GTK_CONTAINER(data->scroll_window), data->scroll_content);
    
    // Top Preview block
    GtkWidget* preview_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    gtk_box_pack_start(GTK_BOX(data->scroll_content), preview_box, FALSE, FALSE, 0);
    
    data->icon_preview = gtk_image_new_from_icon_name("folder", GTK_ICON_SIZE_DIALOG);
    gtk_image_set_pixel_size(GTK_IMAGE(data->icon_preview), 110);
    gtk_box_pack_start(GTK_BOX(preview_box), data->icon_preview, FALSE, FALSE, 8);
    
    data->title_label = gtk_label_new("Select an item");
    gtk_label_set_xalign(GTK_LABEL(data->title_label), 0.5);
    gtk_label_set_justify(GTK_LABEL(data->title_label), GTK_JUSTIFY_CENTER);
    gtk_label_set_line_wrap(GTK_LABEL(data->title_label), TRUE);
    gtk_label_set_lines(GTK_LABEL(data->title_label), 3);
    gtk_label_set_ellipsize(GTK_LABEL(data->title_label), PANGO_ELLIPSIZE_END);
    gtk_label_set_markup(GTK_LABEL(data->title_label), "<b>Select an item</b>");
    gtk_box_pack_start(GTK_BOX(preview_box), data->title_label, FALSE, FALSE, 0);
    
    data->type_label = gtk_label_new("");
    gtk_label_set_xalign(GTK_LABEL(data->type_label), 0.5);
    gtk_style_context_add_class(gtk_widget_get_style_context(data->type_label), "files-prop-val");
    gtk_box_pack_start(GTK_BOX(preview_box), data->type_label, FALSE, FALSE, 8);

    // Dynamic Box where all sections will be packed dynamically
    data->dynamic_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_box_pack_start(GTK_BOX(data->scroll_content), data->dynamic_box, FALSE, FALSE, 0);

    // Sticky Actions Box at the bottom of the ROOT (outside scrolled window)
    data->actions_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    gtk_container_set_border_width(GTK_CONTAINER(data->actions_box), 12);
    
    // Removed ACTIONS header

    gtk_box_pack_end(GTK_BOX(root), data->actions_box, FALSE, FALSE, 0);

    data->btn_open = create_action_btn("Open", "document-open-symbolic");
    gtk_style_context_add_class(gtk_widget_get_style_context(data->btn_open), "suggested-action");
    data->btn_term = create_action_btn("Open in Terminal", "utilities-terminal-symbolic");
    data->btn_copy_path = create_action_btn("Copy Path", "edit-copy-symbolic");
    data->btn_more = create_action_btn("More", "view-more-symbolic");
    
    gtk_box_pack_start(GTK_BOX(data->actions_box), data->btn_open, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(data->actions_box), data->btn_term, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(data->actions_box), data->btn_copy_path, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(data->actions_box), data->btn_more, FALSE, FALSE, 0);
    
    GtkWidget* popover = gtk_popover_new(data->btn_more);
    data->pop_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    
    auto add_pop_btn = [&](const char* label) {
        GtkWidget* b = gtk_button_new_with_label(label);
        gtk_button_set_relief(GTK_BUTTON(b), GTK_RELIEF_NONE);
        gtk_label_set_xalign(GTK_LABEL(gtk_bin_get_child(GTK_BIN(b))), 0.0);
        gtk_box_pack_start(GTK_BOX(data->pop_box), b, FALSE, FALSE, 0);
        return b;
    };
    
    add_pop_btn("Rename");
    add_pop_btn("Move to...");
    add_pop_btn("Copy to...");
    add_pop_btn("Create Link");
    add_pop_btn("Compress...");
    add_pop_btn("Properties");
    
    gtk_container_add(GTK_CONTAINER(popover), data->pop_box);
    gtk_widget_show_all(data->pop_box);
    
    g_signal_connect(data->btn_more, "clicked", G_CALLBACK(+[](GtkButton*, gpointer user_data) {
        gtk_popover_popup(GTK_POPOVER(user_data));
    }), popover);

    g_signal_connect(data->btn_open, "clicked", G_CALLBACK(+[](GtkButton*, gpointer user_data) {
        auto* d = static_cast<InspectorData*>(user_data);
        for (const auto& p : d->selection) {
            std::string cmd = "xdg-open \"" + p + "\" &";
            system(cmd.c_str());
        }
    }), data);
    
    g_signal_connect(data->btn_term, "clicked", G_CALLBACK(+[](GtkButton*, gpointer user_data) {
        auto* d = static_cast<InspectorData*>(user_data);
        if (!d->selection.empty()) {
            std::string p = d->selection[0];
            if (!fs::is_directory(p)) p = fs::path(p).parent_path().string();
            std::string cmd = "foot --working-directory=\"" + p + "\" &";
            system(cmd.c_str());
        }
    }), data);
    
    g_signal_connect(data->btn_copy_path, "clicked", G_CALLBACK(+[](GtkButton*, gpointer user_data) {
        auto* d = static_cast<InspectorData*>(user_data);
        if (!d->selection.empty()) {
            GtkClipboard* clip = gtk_clipboard_get(GDK_SELECTION_CLIPBOARD);
            gtk_clipboard_set_text(clip, d->selection[0].c_str(), -1);
        }
    }), data);

    g_object_set_data_full(G_OBJECT(root), "inspector_data", data, [](gpointer p) {
        delete static_cast<InspectorData*>(p);
    });

    gtk_widget_show_all(root);
    return root;
}

void InspectorPanel::set_current_directory(GtkWidget* panel, const std::string& directory_path) {
    update_selection(panel, {directory_path});
}

struct InfoThreadCtx {
    InspectorData* data;
    uint64_t gen;
    std::string path;
    
    std::string thumb_file;
    std::string pdf_pages;
    std::string pdf_version;
    std::string vid_res;
    std::string vid_dur;
    
    uint64_t dir_bytes = 0;
    int dir_files = 0;
    int dir_folders = 0;
    bool is_dir = false;
};

struct ChecksumResultCtx {
    InspectorData* data;
    std::string sha;
    uint64_t gen;
    GtkButton* btn;
};

static void clear_container(GtkWidget* container) {
    GList* children = gtk_container_get_children(GTK_CONTAINER(container));
    for (GList* iter = children; iter != nullptr; iter = g_list_next(iter)) {
        gtk_widget_destroy(GTK_WIDGET(iter->data));
    }
    g_list_free(children);
}

void InspectorPanel::update_selection(GtkWidget* panel, const std::vector<std::string>& selected_paths) {
    if (!panel) return;
    auto* data = static_cast<InspectorData*>(g_object_get_data(G_OBJECT(panel), "inspector_data"));
    if (!data) return;

    data->calculation_gen++;
    data->selection = selected_paths;

    clear_container(data->dynamic_box);

    if (selected_paths.empty()) {
        data->current_path = "";
        gtk_image_set_from_icon_name(GTK_IMAGE(data->icon_preview), "folder", GTK_ICON_SIZE_DIALOG);
        gtk_label_set_markup(GTK_LABEL(data->title_label), "<span size='large'><b>Select an item</b></span>");
        gtk_widget_set_tooltip_text(data->title_label, nullptr);
        gtk_label_set_text(GTK_LABEL(data->type_label), "");
        gtk_widget_set_visible(data->actions_box, FALSE);
        return;
    }

    gtk_widget_set_visible(data->actions_box, TRUE);
    gtk_widget_set_visible(data->btn_open, TRUE);
    gtk_widget_set_visible(data->btn_copy_path, TRUE);
    gtk_widget_set_visible(data->btn_more, TRUE);

    if (selected_paths.size() > 1) {
        gtk_image_set_from_icon_name(GTK_IMAGE(data->icon_preview), "emblem-documents", GTK_ICON_SIZE_DIALOG);
        gtk_image_set_pixel_size(GTK_IMAGE(data->icon_preview), 110);
        gtk_label_set_markup(GTK_LABEL(data->title_label), ("<span size='large'><b>" + std::to_string(selected_paths.size()) + " items selected</b></span>").c_str());
        gtk_widget_set_tooltip_text(data->title_label, nullptr);
        gtk_label_set_text(GTK_LABEL(data->type_label), "Multiple Selection");
        
        gtk_widget_set_visible(data->btn_term, FALSE);

        uint64_t total_size = 0;
        std::error_code ec;
        for (const auto& p : selected_paths) {
            if (fs::is_regular_file(p, ec)) total_size += fs::file_size(p, ec);
        }
        
        GtkWidget* sec = create_section("INFORMATION");
        gtk_box_pack_start(GTK_BOX(sec), create_row("Items", std::to_string(selected_paths.size()).c_str()), FALSE, FALSE, 0);
        gtk_box_pack_start(GTK_BOX(sec), create_row("Total Size", FileItem::format_size(total_size).c_str()), FALSE, FALSE, 0);
        gtk_box_pack_start(GTK_BOX(data->dynamic_box), sec, FALSE, FALSE, 0);
        
        gtk_widget_show_all(data->dynamic_box);
        return;
    }

    // Single item selection
    const std::string& p = selected_paths[0];
    data->current_path = p;

    std::string filename = fs::path(p).filename().string();
    if (filename.empty()) filename = p;

    gtk_label_set_markup(GTK_LABEL(data->title_label), ("<span size='large'><b>" + filename + "</b></span>").c_str());
    gtk_widget_set_tooltip_text(data->title_label, filename.c_str());

    GFile* gf = g_file_parse_name(p.c_str());
    GFileInfo* fi = gf ? g_file_query_info(gf, "standard::*,time::unix", G_FILE_QUERY_INFO_NONE, nullptr, nullptr) : nullptr;

    bool is_dir = false;
    std::string mime = "application/octet-stream";
    if (fi) {
        is_dir = (g_file_info_get_file_type(fi) == G_FILE_TYPE_DIRECTORY);
        const char* ct = g_file_info_get_content_type(fi);
        if (ct) mime = ct;
        g_object_unref(fi);
    }
    if (gf) g_object_unref(gf);

    std::string display_mime = mime;
    if (is_dir) display_mime = "Folder";
    
    if (mime == "application/pdf") display_mime = "PDF Document";
    else if (mime.starts_with("image/")) display_mime = "Image";
    else if (mime.starts_with("video/")) display_mime = "Video";
    
    gtk_label_set_text(GTK_LABEL(data->type_label), display_mime.c_str());
    gtk_widget_set_visible(data->btn_term, is_dir);

    // Thumbnail Preview Block
    GdkPixbuf* thumb = FileItem::load_thumbnail(p, "file://" + p, mime, 256); 
    if (thumb) {
        gtk_image_set_from_pixbuf(GTK_IMAGE(data->icon_preview), thumb);
        g_object_unref(thumb);
    } else {
        gtk_image_set_from_icon_name(GTK_IMAGE(data->icon_preview), is_dir ? "folder" : "text-x-generic", GTK_ICON_SIZE_DIALOG);
        gtk_image_set_pixel_size(GTK_IMAGE(data->icon_preview), 110);
    }

    struct stat st;
    if (stat(p.c_str(), &st) != 0) return;
    
    char timebuf[64];
    
    struct tm* tm_info = localtime(&st.st_mtime);
    strftime(timebuf, sizeof(timebuf), "%b %d, %Y", tm_info);
    
    // --- INJECT BADGES ---
    GtkWidget* badge_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    gtk_widget_set_halign(badge_box, GTK_ALIGN_CENTER);
    
    auto create_badge = [](const std::string& text) {
        GtkWidget* l = gtk_label_new(text.c_str());
        gtk_style_context_add_class(gtk_widget_get_style_context(l), "files-badge");
        return l;
    };
    
    std::string type_badge = is_dir ? "FOLDER" : (mime == "application/pdf" ? "PDF" : (mime.starts_with("image/") ? "IMAGE" : "FILE"));
    gtk_box_pack_start(GTK_BOX(badge_box), create_badge(type_badge), FALSE, FALSE, 0);
    
    GtkWidget* size_badge = create_badge(is_dir ? "CALCULATING..." : FileItem::format_size(st.st_size));
    gtk_box_pack_start(GTK_BOX(badge_box), size_badge, FALSE, FALSE, 0);
    
    // Add it to the preview_box. But wait, we don't have access to preview_box here.
    // We will add it to dynamic_box first.
    gtk_box_pack_start(GTK_BOX(data->dynamic_box), badge_box, FALSE, FALSE, 8);

    
    // BUILD DYNAMIC UI STRUCTURE
    GtkWidget* sec_info = create_section(is_dir ? "CONTENTS" : "INFORMATION");
    GtkWidget* sec_loc = create_section("LOCATION");
    GtkWidget* sec_perm = create_section("PERMISSIONS");

    GtkWidget* lbl_size = nullptr;
    GtkWidget* lbl_files = nullptr;
    GtkWidget* lbl_folders = nullptr;
    GtkWidget* lbl_pages = nullptr;
    GtkWidget* lbl_vid_res = nullptr;
    GtkWidget* lbl_vid_dur = nullptr;

    if (is_dir) {
        lbl_files = gtk_label_new("Calculating…");
        gtk_label_set_xalign(GTK_LABEL(lbl_files), 1.0);
        gtk_style_context_add_class(gtk_widget_get_style_context(lbl_files), "files-prop-val");
        gtk_box_pack_start(GTK_BOX(sec_info), create_row_widget("Files", lbl_files), FALSE, FALSE, 0);
        
        lbl_folders = gtk_label_new("Calculating…");
        gtk_label_set_xalign(GTK_LABEL(lbl_folders), 1.0);
        gtk_style_context_add_class(gtk_widget_get_style_context(lbl_folders), "files-prop-val");
        gtk_box_pack_start(GTK_BOX(sec_info), create_row_widget("Folders", lbl_folders), FALSE, FALSE, 0);
        
        lbl_size = gtk_label_new("Calculating…");
        gtk_label_set_xalign(GTK_LABEL(lbl_size), 1.0);
        gtk_style_context_add_class(gtk_widget_get_style_context(lbl_size), "files-prop-val");
        gtk_box_pack_start(GTK_BOX(sec_info), create_row_widget("Total Size", lbl_size), FALSE, FALSE, 0);
    } else {
        gtk_box_pack_start(GTK_BOX(sec_info), create_row("Size", FileItem::format_size(st.st_size).c_str(), "drive-harddisk-symbolic"), FALSE, FALSE, 0);
    }
    
    if (mime.starts_with("image/")) {
        gint w = 0, h = 0;
        if (gdk_pixbuf_get_file_info(p.c_str(), &w, &h)) {
            std::string dim = std::to_string(w) + " × " + std::to_string(h);
            gtk_box_pack_start(GTK_BOX(sec_info), create_row("Dimensions", dim.c_str(), "image-x-generic-symbolic"), FALSE, FALSE, 0);
        }
    } else if (mime == "application/pdf") {
        lbl_pages = gtk_label_new("Reading…");
        gtk_label_set_xalign(GTK_LABEL(lbl_pages), 1.0);
        gtk_style_context_add_class(gtk_widget_get_style_context(lbl_pages), "files-prop-val");
        gtk_box_pack_start(GTK_BOX(sec_info), create_row_widget("Pages", lbl_pages), FALSE, FALSE, 0);
    } else if (mime.starts_with("video/")) {
        lbl_vid_res = gtk_label_new("Reading…");
        gtk_label_set_xalign(GTK_LABEL(lbl_vid_res), 1.0);
        gtk_style_context_add_class(gtk_widget_get_style_context(lbl_vid_res), "files-prop-val");
        gtk_box_pack_start(GTK_BOX(sec_info), create_row_widget("Resolution", lbl_vid_res), FALSE, FALSE, 0);
        
        lbl_vid_dur = gtk_label_new("Reading…");
        gtk_label_set_xalign(GTK_LABEL(lbl_vid_dur), 1.0);
        gtk_style_context_add_class(gtk_widget_get_style_context(lbl_vid_dur), "files-prop-val");
        gtk_box_pack_start(GTK_BOX(sec_info), create_row_widget("Duration", lbl_vid_dur), FALSE, FALSE, 0);
    }
    
    gtk_box_pack_start(GTK_BOX(sec_info), create_row("Modified", timebuf, "document-open-recent-symbolic"), FALSE, FALSE, 0);
    
    // Location
    GtkWidget* loc_hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
    GtkWidget* l_val = gtk_label_new(p.c_str());
    gtk_label_set_xalign(GTK_LABEL(l_val), 0.0);
    gtk_label_set_ellipsize(GTK_LABEL(l_val), PANGO_ELLIPSIZE_START);
    gtk_style_context_add_class(gtk_widget_get_style_context(l_val), "files-prop-val");
    gtk_box_pack_start(GTK_BOX(loc_hbox), l_val, TRUE, TRUE, 0);
    
    GtkWidget* l_btn = gtk_button_new_from_icon_name("edit-copy-symbolic", GTK_ICON_SIZE_MENU);
    gtk_button_set_relief(GTK_BUTTON(l_btn), GTK_RELIEF_NONE);
    g_signal_connect(l_btn, "clicked", G_CALLBACK(+[](GtkButton*, gpointer user_data) {
        GtkClipboard* clip = gtk_clipboard_get(GDK_SELECTION_CLIPBOARD);
        gtk_clipboard_set_text(clip, static_cast<const char*>(user_data), -1);
    }), (gpointer)strdup(p.c_str()));
    gtk_box_pack_end(GTK_BOX(loc_hbox), l_btn, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(sec_loc), loc_hbox, FALSE, FALSE, 0);
    
    // Permissions
    struct passwd* pw = getpwuid(st.st_uid);
    struct group* gr = getgrgid(st.st_gid);
    std::string own = pw ? pw->pw_name : std::to_string(st.st_uid);
    std::string group = gr ? gr->gr_name : std::to_string(st.st_gid);
    gtk_box_pack_start(GTK_BOX(sec_perm), create_row("Owner", own.c_str(), "avatar-default-symbolic"), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(sec_perm), create_row("Group", group.c_str(), "system-users-symbolic"), FALSE, FALSE, 0);
    
    GtkWidget* perm_expander = gtk_expander_new("Details");
    GtkWidget* p_vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    gtk_container_set_border_width(GTK_CONTAINER(p_vbox), 8);
    gtk_box_pack_start(GTK_BOX(p_vbox), create_row("Owner", format_perm_detailed(st.st_mode, 6).c_str()), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(p_vbox), create_row("Group", format_perm_detailed(st.st_mode, 3).c_str()), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(p_vbox), create_row("Others", format_perm_detailed(st.st_mode, 0).c_str()), FALSE, FALSE, 0);
    gtk_container_add(GTK_CONTAINER(perm_expander), p_vbox);
    gtk_box_pack_start(GTK_BOX(sec_perm), perm_expander, FALSE, FALSE, 4);

    gtk_box_pack_start(GTK_BOX(data->dynamic_box), sec_info, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(data->dynamic_box), sec_loc, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(data->dynamic_box), sec_perm, FALSE, FALSE, 0);

    // Checksums (Only for files)
    if (!is_dir) {
        GtkWidget* sec_check = create_section("CHECKSUMS");
        GtkWidget* s256 = gtk_label_new("—");
        gtk_box_pack_start(GTK_BOX(sec_check), create_row_widget("SHA-256", s256), FALSE, FALSE, 0);
        gtk_label_set_xalign(GTK_LABEL(s256), 1.0); gtk_style_context_add_class(gtk_widget_get_style_context(s256), "files-prop-val");
        
        GtkWidget* c_btn = gtk_button_new_with_label("Calculate SHA-256");
        gtk_style_context_add_class(gtk_widget_get_style_context(c_btn), "files-btn-tool");
        gtk_box_pack_start(GTK_BOX(sec_check), c_btn, FALSE, FALSE, 4);
        
        g_object_set_data_full(G_OBJECT(c_btn), "val_lbl", s256, nullptr);
        
        g_signal_connect(c_btn, "clicked", G_CALLBACK(+[](GtkButton* b, gpointer user_data) {
            auto* d = static_cast<InspectorData*>(user_data);
            if (d->current_path.empty()) return;
            std::string p = d->current_path;
            uint64_t my_gen = d->calculation_gen;
            gtk_button_set_label(b, "Calculating...");
            gtk_widget_set_sensitive(GTK_WIDGET(b), FALSE);

            std::thread([d, p, my_gen, b]() {
                std::string sha = calculate_file_checksum(p, G_CHECKSUM_SHA256);
                g_idle_add(+[](gpointer ptr) -> gboolean {
                    auto* ctx = static_cast<ChecksumResultCtx*>(ptr);
                    if (ctx->data->calculation_gen == ctx->gen) {
                        GtkWidget* vlbl = static_cast<GtkWidget*>(g_object_get_data(G_OBJECT(ctx->btn), "val_lbl"));
                        gtk_label_set_text(GTK_LABEL(vlbl), ctx->sha.c_str());
                        gtk_widget_hide(GTK_WIDGET(ctx->btn)); // Hide button after calculate per user spec
                    }
                    delete ctx;
                    return G_SOURCE_REMOVE;
                }, new ChecksumResultCtx{d, sha, my_gen, b});
            }).detach();
        }), data);
        gtk_box_pack_start(GTK_BOX(data->dynamic_box), sec_check, FALSE, FALSE, 0);
    }

    gtk_widget_show_all(data->dynamic_box);
    
    // Background Thread for Heavy Lifting (Previews + Metadata)
    uint64_t gen = data->calculation_gen;
    std::thread([data, gen, p, mime, is_dir, thumb, lbl_size, lbl_files, lbl_folders, lbl_pages, lbl_vid_res, lbl_vid_dur]() {
        auto* ctx = new InfoThreadCtx{data, gen, p};
        ctx->is_dir = is_dir;
        
        if (is_dir) {
            std::error_code ec;
            for (auto it = fs::recursive_directory_iterator(p, fs::directory_options::skip_permission_denied, ec);
                 it != fs::recursive_directory_iterator(); ++it) {
                if (data->calculation_gen != gen) break;
                if (it->is_regular_file(ec)) {
                    ctx->dir_bytes += it->file_size(ec);
                    ctx->dir_files++;
                } else if (it->is_directory(ec)) {
                    ctx->dir_folders++;
                }
            }
        } else {
            // PDF Extractor
            if (mime == "application/pdf") {
                if (!thumb) {
                    std::string out = "/tmp/zenith_pdf_" + std::to_string(gen);
                    std::string cmd = "pdftocairo -png -singlefile -scale-to 256 \"" + p + "\" \"" + out + "\"";
                    system(cmd.c_str());
                    if (fs::exists(out + ".png")) ctx->thumb_file = out + ".png";
                }
                
                std::string info = exec_cmd(("pdfinfo \"" + p + "\"").c_str());
                if (!info.empty()) {
                    std::istringstream stream(info);
                    std::string line;
                    while (std::getline(stream, line)) {
                        if (line.starts_with("Pages:")) ctx->pdf_pages = line.substr(line.find(':')+1);
                        else if (line.starts_with("PDF version:")) ctx->pdf_version = line.substr(line.find(':')+1);
                    }
                }
            }
            else if (mime.starts_with("video/")) {
                if (!thumb) {
                    std::string out = "/tmp/zenith_vid_" + std::to_string(gen) + ".png";
                    std::string cmd = "ffmpeg -y -i \"" + p + "\" -ss 00:00:01.000 -vframes 1 -vf scale=256:-1 \"" + out + "\" 2>/dev/null";
                    system(cmd.c_str());
                    if (fs::exists(out)) ctx->thumb_file = out;
                }
                std::string res = exec_cmd(("ffprobe -v error -select_streams v:0 -show_entries stream=width,height -of csv=s=x:p=0 \"" + p + "\"").c_str());
                std::string dur = exec_cmd(("ffprobe -v error -show_entries format=duration -of default=noprint_wrappers=1:nokey=1 \"" + p + "\"").c_str());
                ctx->vid_res = res;
                if (!dur.empty()) {
                    try {
                        int seconds = std::stoi(dur);
                        int h = seconds / 3600;
                        int m = (seconds % 3600) / 60;
                        int s = seconds % 60;
                        char buf[32];
                        if (h > 0) snprintf(buf, sizeof(buf), "%02d:%02d:%02d", h, m, s);
                        else snprintf(buf, sizeof(buf), "%02d:%02d", m, s);
                        ctx->vid_dur = buf;
                    } catch(...) {}
                }
            }
        }
        
        g_idle_add(+[](gpointer ptr) -> gboolean {
            auto* pctx = static_cast<std::pair<InfoThreadCtx*, std::vector<GtkWidget*>>*>(ptr);
            auto* c = pctx->first;
            auto& lbls = pctx->second;
            
            if (c->data->calculation_gen == c->gen) {
                if (c->is_dir) {
                    if (lbls[0]) gtk_label_set_text(GTK_LABEL(lbls[0]), FileItem::format_size(c->dir_bytes).c_str());
                    if (lbls[1]) gtk_label_set_text(GTK_LABEL(lbls[1]), std::to_string(c->dir_files).c_str());
                    if (lbls[2]) gtk_label_set_text(GTK_LABEL(lbls[2]), std::to_string(c->dir_folders).c_str());
                } else {
                    if (!c->thumb_file.empty()) {
                        GdkPixbuf* pb = gdk_pixbuf_new_from_file_at_scale(c->thumb_file.c_str(), -1, 160, TRUE, nullptr);
                        if (pb) {
                            gtk_image_set_from_pixbuf(GTK_IMAGE(c->data->icon_preview), pb);
                            g_object_unref(pb);
                        }
                        unlink(c->thumb_file.c_str());
                    }
                    if (lbls[3] && !c->pdf_pages.empty()) {
                        std::string clean = c->pdf_pages;
                        while(!clean.empty() && clean[0] == ' ') clean = clean.substr(1);
                        gtk_label_set_text(GTK_LABEL(lbls[3]), clean.c_str());
                    }
                    if (lbls[4] && !c->vid_res.empty()) gtk_label_set_text(GTK_LABEL(lbls[4]), c->vid_res.c_str());
                    if (lbls[5] && !c->vid_dur.empty()) gtk_label_set_text(GTK_LABEL(lbls[5]), c->vid_dur.c_str());
                }
            }
            delete c;
            delete pctx;
            return G_SOURCE_REMOVE;
        }, new std::pair<InfoThreadCtx*, std::vector<GtkWidget*>>{ctx, {lbl_size, lbl_files, lbl_folders, lbl_pages, lbl_vid_res, lbl_vid_dur}});
    }).detach();
}

} // namespace zenith
