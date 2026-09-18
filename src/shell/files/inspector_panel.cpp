#include "shell/files/inspector_panel.hpp"
#include "shell/files/file_item.hpp"
#include <sys/stat.h>
#include <pwd.h>
#include <grp.h>
#include <filesystem>
#include <thread>
#include <atomic>
#include <glib.h>

namespace fs = std::filesystem;

namespace zenith {

struct InspectorData {
    GtkWidget* root_box;
    std::function<void()> on_close;

    GtkWidget* icon_preview;
    GtkWidget* title_label;
    GtkWidget* type_label;

    // Info
    GtkWidget* size_val;
    GtkWidget* items_val;
    GtkWidget* mod_val;
    GtkWidget* dim_val;
    GtkWidget* dim_row; 
    GtkWidget* items_row;

    // Location
    GtkWidget* loc_val;

    // Permissions
    GtkWidget* perm_val;
    GtkWidget* owner_val;

    // Checksums
    GtkWidget* checksum_box;
    GtkWidget* sha256_val;
    GtkWidget* md5_val;
    
    // Actions
    GtkWidget* btn_open;
    GtkWidget* btn_term;
    GtkWidget* btn_copy_path;

    std::string current_path;
    std::vector<std::string> selection;
    std::atomic<uint64_t> calculation_gen{0};
};

struct DirCalcResultCtx {
    InspectorData* data;
    uint64_t bytes;
    int count;
    uint64_t gen;
};

struct ChecksumResultCtx {
    InspectorData* data;
    std::string sha;
    std::string md5;
    uint64_t gen;
    GtkButton* btn;
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

static std::string format_perm_string(mode_t mode) {
    char buf[11];
    buf[0] = S_ISDIR(mode) ? 'd' : (S_ISLNK(mode) ? 'l' : '-');
    buf[1] = (mode & S_IRUSR) ? 'r' : '-';
    buf[2] = (mode & S_IWUSR) ? 'w' : '-';
    buf[3] = (mode & S_IXUSR) ? 'x' : '-';
    buf[4] = (mode & S_IRGRP) ? 'r' : '-';
    buf[5] = (mode & S_IWGRP) ? 'w' : '-';
    buf[6] = (mode & S_IXGRP) ? 'x' : '-';
    buf[7] = (mode & S_IROTH) ? 'r' : '-';
    buf[8] = (mode & S_IWOTH) ? 'w' : '-';
    buf[9] = (mode & S_IXOTH) ? 'x' : '-';
    buf[10] = '\0';
    return std::string(buf);
}

static GtkWidget* create_row(const char* label_text, GtkWidget*& val_lbl_out, const char* icon_name = nullptr) {
    GtkWidget* hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    if (icon_name) {
        GtkWidget* img = gtk_image_new_from_icon_name(icon_name, GTK_ICON_SIZE_MENU);
        gtk_box_pack_start(GTK_BOX(hbox), img, FALSE, FALSE, 0);
    }
    
    GtkWidget* lbl = gtk_label_new(label_text);
    gtk_label_set_xalign(GTK_LABEL(lbl), 0.0);
    gtk_style_context_add_class(gtk_widget_get_style_context(lbl), "files-prop-lbl");
    gtk_box_pack_start(GTK_BOX(hbox), lbl, FALSE, FALSE, 0);

    val_lbl_out = gtk_label_new("");
    gtk_label_set_xalign(GTK_LABEL(val_lbl_out), 1.0);
    gtk_label_set_ellipsize(GTK_LABEL(val_lbl_out), PANGO_ELLIPSIZE_START);
    gtk_style_context_add_class(gtk_widget_get_style_context(val_lbl_out), "files-prop-val");
    gtk_box_pack_end(GTK_BOX(hbox), val_lbl_out, TRUE, TRUE, 0);
    return hbox;
}

static GtkWidget* create_section(const char* title, std::vector<GtkWidget*> rows) {
    GtkWidget* vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    gtk_container_set_border_width(GTK_CONTAINER(vbox), 8);
    
    GtkWidget* hdr = gtk_label_new(title);
    gtk_label_set_xalign(GTK_LABEL(hdr), 0.0);
    gtk_style_context_add_class(gtk_widget_get_style_context(hdr), "files-prop-title"); 
    gtk_box_pack_start(GTK_BOX(vbox), hdr, FALSE, FALSE, 4);
    
    GtkWidget* sep = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_box_pack_start(GTK_BOX(vbox), sep, FALSE, FALSE, 2);

    for (auto* r : rows) {
        gtk_box_pack_start(GTK_BOX(vbox), r, FALSE, FALSE, 0);
    }
    return vbox;
}

GtkWidget* InspectorPanel::create(CloseCallback on_close) {
    auto* data = new InspectorData();
    data->on_close = on_close;

    GtkWidget* root = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_size_request(root, 320, -1);
    gtk_style_context_add_class(gtk_widget_get_style_context(root), "files-inspector-panel");
    data->root_box = root;

    // Header
    GtkWidget* header = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_container_set_border_width(GTK_CONTAINER(header), 12);
    gtk_box_pack_start(GTK_BOX(root), header, FALSE, FALSE, 0);

    GtkWidget* title = gtk_label_new("INSPECTOR");
    gtk_style_context_add_class(gtk_widget_get_style_context(title), "files-inspector-title");
    gtk_box_pack_start(GTK_BOX(header), title, TRUE, TRUE, 0);

    GtkWidget* close_btn = gtk_button_new_from_icon_name("window-close-symbolic", GTK_ICON_SIZE_BUTTON);
    gtk_button_set_relief(GTK_BUTTON(close_btn), GTK_RELIEF_NONE);
    g_signal_connect(close_btn, "clicked", G_CALLBACK(+[](GtkButton*, gpointer user_data) {
        auto* d = static_cast<InspectorData*>(user_data);
        if (d->on_close) d->on_close();
    }), data);
    gtk_box_pack_end(GTK_BOX(header), close_btn, FALSE, FALSE, 0);

    GtkWidget* scroll = gtk_scrolled_window_new(nullptr, nullptr);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);

    GtkWidget* content_vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 16);
    gtk_container_set_border_width(GTK_CONTAINER(content_vbox), 16);
    
    // 1. Large Preview
    GtkWidget* preview_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_box_pack_start(GTK_BOX(content_vbox), preview_box, FALSE, FALSE, 0);
    
    data->icon_preview = gtk_image_new_from_icon_name("folder", GTK_ICON_SIZE_DIALOG);
    gtk_image_set_pixel_size(GTK_IMAGE(data->icon_preview), 128);
    gtk_box_pack_start(GTK_BOX(preview_box), data->icon_preview, FALSE, FALSE, 16);
    
    data->title_label = gtk_label_new("Select an item");
    gtk_label_set_ellipsize(GTK_LABEL(data->title_label), PANGO_ELLIPSIZE_MIDDLE);
    gtk_label_set_xalign(GTK_LABEL(data->title_label), 0.5);
    gtk_label_set_markup(GTK_LABEL(data->title_label), "<b>Select an item</b>");
    gtk_box_pack_start(GTK_BOX(preview_box), data->title_label, FALSE, FALSE, 0);
    
    data->type_label = gtk_label_new("");
    gtk_label_set_xalign(GTK_LABEL(data->type_label), 0.5);
    gtk_style_context_add_class(gtk_widget_get_style_context(data->type_label), "files-prop-val");
    gtk_box_pack_start(GTK_BOX(preview_box), data->type_label, FALSE, FALSE, 0);

    // 2. Information Section
    GtkWidget* size_r = create_row("Size", data->size_val, "drive-harddisk-symbolic");
    data->items_row = create_row("Items", data->items_val, "folder-symbolic");
    data->dim_row = create_row("Dimensions", data->dim_val, "image-x-generic-symbolic");
    GtkWidget* mod_r = create_row("Modified", data->mod_val, "document-open-recent-symbolic");
    
    GtkWidget* info_sec = create_section("INFORMATION", {size_r, data->items_row, data->dim_row, mod_r});
    gtk_box_pack_start(GTK_BOX(content_vbox), info_sec, FALSE, FALSE, 0);
    
    // 3. Location Section
    GtkWidget* loc_r = create_row("Path", data->loc_val);
    GtkWidget* loc_sec = create_section("LOCATION", {loc_r});
    gtk_box_pack_start(GTK_BOX(content_vbox), loc_sec, FALSE, FALSE, 0);
    
    // 4. Permissions Section
    GtkWidget* perm_r = create_row("Access", data->perm_val);
    GtkWidget* own_r = create_row("Owner", data->owner_val);
    GtkWidget* perm_sec = create_section("PERMISSIONS", {own_r, perm_r});
    gtk_box_pack_start(GTK_BOX(content_vbox), perm_sec, FALSE, FALSE, 0);
    
    // Checksums
    data->checksum_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    gtk_container_set_border_width(GTK_CONTAINER(data->checksum_box), 8);
    GtkWidget* chk_hdr = gtk_label_new("CHECKSUMS");
    gtk_label_set_xalign(GTK_LABEL(chk_hdr), 0.0);
    gtk_style_context_add_class(gtk_widget_get_style_context(chk_hdr), "files-prop-title");
    gtk_box_pack_start(GTK_BOX(data->checksum_box), chk_hdr, FALSE, FALSE, 4);
    gtk_box_pack_start(GTK_BOX(data->checksum_box), gtk_separator_new(GTK_ORIENTATION_HORIZONTAL), FALSE, FALSE, 2);
    
    GtkWidget* sha_r = create_row("SHA-256", data->sha256_val);
    GtkWidget* md5_r = create_row("MD5", data->md5_val);
    gtk_label_set_ellipsize(GTK_LABEL(data->sha256_val), PANGO_ELLIPSIZE_START);
    gtk_label_set_ellipsize(GTK_LABEL(data->md5_val), PANGO_ELLIPSIZE_START);
    gtk_box_pack_start(GTK_BOX(data->checksum_box), sha_r, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(data->checksum_box), md5_r, FALSE, FALSE, 0);
    
    GtkWidget* calc_btn = gtk_button_new_with_label("Calculate Hashes");
    gtk_style_context_add_class(gtk_widget_get_style_context(calc_btn), "files-btn-tool");
    g_signal_connect(calc_btn, "clicked", G_CALLBACK(+[](GtkButton* b, gpointer user_data) {
        auto* d = static_cast<InspectorData*>(user_data);
        if (d->current_path.empty()) return;
        std::string p = d->current_path;
        uint64_t my_gen = d->calculation_gen;
        gtk_button_set_label(b, "Calculating...");
        gtk_widget_set_sensitive(GTK_WIDGET(b), FALSE);

        std::thread([d, p, my_gen, b]() {
            std::string sha = calculate_file_checksum(p, G_CHECKSUM_SHA256);
            std::string md5 = calculate_file_checksum(p, G_CHECKSUM_MD5);
            g_idle_add(+[](gpointer ptr) -> gboolean {
                auto* ctx = static_cast<ChecksumResultCtx*>(ptr);
                if (ctx->data->calculation_gen == ctx->gen) {
                    gtk_label_set_text(GTK_LABEL(ctx->data->sha256_val), ctx->sha.c_str());
                    gtk_label_set_text(GTK_LABEL(ctx->data->md5_val), ctx->md5.c_str());
                }
                gtk_button_set_label(ctx->btn, "Calculate Hashes");
                gtk_widget_set_sensitive(GTK_WIDGET(ctx->btn), TRUE);
                delete ctx;
                return G_SOURCE_REMOVE;
            }, new ChecksumResultCtx{d, sha, md5, my_gen, b});
        }).detach();
    }), data);
    gtk_box_pack_start(GTK_BOX(data->checksum_box), calc_btn, FALSE, FALSE, 4);
    gtk_box_pack_start(GTK_BOX(content_vbox), data->checksum_box, FALSE, FALSE, 0);

    // Actions Section
    GtkWidget* actions_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    gtk_container_set_border_width(GTK_CONTAINER(actions_box), 8);
    gtk_box_pack_start(GTK_BOX(content_vbox), actions_box, FALSE, FALSE, 8);
    
    auto create_action_btn = [](const char* label, const char* icon) {
        GtkWidget* btn = gtk_button_new();
        GtkWidget* box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
        gtk_container_set_border_width(GTK_CONTAINER(box), 4);
        gtk_box_pack_start(GTK_BOX(box), gtk_image_new_from_icon_name(icon, GTK_ICON_SIZE_BUTTON), FALSE, FALSE, 0);
        GtkWidget* l = gtk_label_new(label);
        gtk_label_set_xalign(GTK_LABEL(l), 0.0);
        gtk_box_pack_start(GTK_BOX(box), l, TRUE, TRUE, 0);
        gtk_container_add(GTK_CONTAINER(btn), box);
        return btn;
    };
    
    data->btn_open = create_action_btn("Open", "document-open-symbolic");
    data->btn_term = create_action_btn("Open in Terminal", "utilities-terminal-symbolic");
    data->btn_copy_path = create_action_btn("Copy Path", "edit-copy-symbolic");
    
    gtk_box_pack_start(GTK_BOX(actions_box), data->btn_open, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(actions_box), data->btn_term, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(actions_box), data->btn_copy_path, FALSE, FALSE, 0);
    
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

    gtk_container_add(GTK_CONTAINER(scroll), content_vbox);
    gtk_box_pack_start(GTK_BOX(root), scroll, TRUE, TRUE, 0);

    g_object_set_data_full(G_OBJECT(root), "inspector_data", data, [](gpointer p) {
        delete static_cast<InspectorData*>(p);
    });

    gtk_widget_show_all(root);
    return root;
}

void InspectorPanel::set_current_directory(GtkWidget* panel, const std::string& directory_path) {
    if (!panel) return;
    auto* data = static_cast<InspectorData*>(g_object_get_data(G_OBJECT(panel), "inspector_data"));
    if (!data) return;

    data->current_path = directory_path;
    data->selection = {directory_path};
    data->calculation_gen++;

    gtk_image_set_from_icon_name(GTK_IMAGE(data->icon_preview), "folder", GTK_ICON_SIZE_DIALOG);
    gtk_image_set_pixel_size(GTK_IMAGE(data->icon_preview), 128);
    
    std::string base = directory_path;
    size_t s = directory_path.find_last_of('/');
    if (s != std::string::npos && s + 1 < directory_path.length()) base = directory_path.substr(s + 1);
    if (base.empty()) base = "/";

    gtk_label_set_markup(GTK_LABEL(data->title_label), ("<span size='large'><b>" + base + "</b></span>").c_str());
    gtk_label_set_text(GTK_LABEL(data->type_label), "Folder");
    gtk_label_set_text(GTK_LABEL(data->loc_val), directory_path.c_str());
    gtk_widget_set_visible(data->dim_row, FALSE);
    gtk_widget_set_visible(data->checksum_box, FALSE);
    gtk_widget_set_visible(data->items_row, TRUE);
    gtk_widget_set_visible(data->btn_term, TRUE);

    struct stat st;
    if (stat(directory_path.c_str(), &st) == 0) {
        gtk_label_set_text(GTK_LABEL(data->perm_val), format_perm_string(st.st_mode).c_str());
        struct passwd* pw = getpwuid(st.st_uid);
        struct group* gr = getgrgid(st.st_gid);
        std::string own = (pw ? pw->pw_name : std::to_string(st.st_uid)) + ":" + (gr ? gr->gr_name : std::to_string(st.st_gid));
        gtk_label_set_text(GTK_LABEL(data->owner_val), own.c_str());

        char timebuf[64];
        struct tm* tm_info = localtime(&st.st_mtime);
        strftime(timebuf, sizeof(timebuf), "%b %d, %Y", tm_info);
        gtk_label_set_text(GTK_LABEL(data->mod_val), timebuf);
        
        gtk_label_set_text(GTK_LABEL(data->size_val), "Calculating…");
        gtk_label_set_text(GTK_LABEL(data->items_val), "Calculating…");

        // Calculate folder size in background
        uint64_t my_gen = data->calculation_gen;
        std::string folder_target = directory_path;
        std::thread([data, folder_target, my_gen]() {
            uint64_t total = 0;
            int count = 0;
            std::error_code ec;
            for (auto it = fs::recursive_directory_iterator(folder_target, fs::directory_options::skip_permission_denied, ec);
                 it != fs::recursive_directory_iterator(); ++it) {
                if (data->calculation_gen != my_gen) return;
                if (it->is_regular_file(ec)) total += it->file_size(ec);
                count++;
            }

            g_idle_add(+[](gpointer p) -> gboolean {
                auto* r = static_cast<DirCalcResultCtx*>(p);
                if (r->data->calculation_gen == r->gen) {
                    gtk_label_set_text(GTK_LABEL(r->data->size_val), FileItem::format_size(r->bytes).c_str());
                    gtk_label_set_text(GTK_LABEL(r->data->items_val), std::to_string(r->count).c_str());
                }
                delete r;
                return G_SOURCE_REMOVE;
            }, new DirCalcResultCtx{data, total, count, my_gen});
        }).detach();
    }
}

void InspectorPanel::update_selection(GtkWidget* panel, const std::vector<std::string>& selected_paths) {
    if (!panel) return;
    auto* data = static_cast<InspectorData*>(g_object_get_data(G_OBJECT(panel), "inspector_data"));
    if (!data) return;

    data->calculation_gen++;
    data->selection = selected_paths;

    if (selected_paths.empty()) {
        set_current_directory(panel, data->current_path); // fall back to dir
        return;
    }

    if (selected_paths.size() > 1) {
        gtk_image_set_from_icon_name(GTK_IMAGE(data->icon_preview), "emblem-documents", GTK_ICON_SIZE_DIALOG);
        gtk_image_set_pixel_size(GTK_IMAGE(data->icon_preview), 128);
        gtk_label_set_markup(GTK_LABEL(data->title_label), ("<span size='large'><b>" + std::to_string(selected_paths.size()) + " items selected</b></span>").c_str());
        gtk_label_set_text(GTK_LABEL(data->type_label), "Multiple Selection");
        gtk_label_set_text(GTK_LABEL(data->items_val), std::to_string(selected_paths.size()).c_str());
        gtk_widget_set_visible(data->items_row, TRUE);

        uint64_t total_size = 0;
        std::error_code ec;
        for (const auto& p : selected_paths) {
            if (fs::is_regular_file(p, ec)) total_size += fs::file_size(p, ec);
        }
        gtk_label_set_text(GTK_LABEL(data->size_val), FileItem::format_size(total_size).c_str());
        gtk_widget_set_visible(data->dim_row, FALSE);
        gtk_widget_set_visible(data->checksum_box, FALSE);
        gtk_widget_set_visible(data->btn_term, FALSE);
        return;
    }

    // Single item selection
    const std::string& p = selected_paths[0];
    data->current_path = p;

    std::string filename = fs::path(p).filename().string();
    if (filename.empty()) filename = p;

    gtk_label_set_markup(GTK_LABEL(data->title_label), ("<span size='large'><b>" + filename + "</b></span>").c_str());
    gtk_label_set_text(GTK_LABEL(data->loc_val), fs::path(p).parent_path().string().c_str());

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

    gtk_label_set_text(GTK_LABEL(data->type_label), mime.c_str());
    gtk_widget_set_visible(data->btn_term, is_dir);

    // Icon / Thumbnail preview
    GdkPixbuf* thumb = FileItem::load_thumbnail(p, "file://" + p, mime, 256); // larger thumb for inspector
    if (thumb) {
        gtk_image_set_from_pixbuf(GTK_IMAGE(data->icon_preview), thumb);
        g_object_unref(thumb);
    } else {
        gtk_image_set_from_icon_name(GTK_IMAGE(data->icon_preview), is_dir ? "folder" : "text-x-generic", GTK_ICON_SIZE_DIALOG);
        gtk_image_set_pixel_size(GTK_IMAGE(data->icon_preview), 128);
    }

    struct stat st;
    if (stat(p.c_str(), &st) == 0) {
        if (!is_dir) {
            gtk_label_set_text(GTK_LABEL(data->size_val), FileItem::format_size(st.st_size).c_str());
            gtk_widget_set_visible(data->checksum_box, TRUE);
            gtk_widget_set_visible(data->items_row, FALSE);
            gtk_label_set_text(GTK_LABEL(data->sha256_val), "—");
            gtk_label_set_text(GTK_LABEL(data->md5_val), "—");
        } else {
            gtk_label_set_text(GTK_LABEL(data->size_val), "Calculating…");
            gtk_label_set_text(GTK_LABEL(data->items_val), "Calculating…");
            gtk_widget_set_visible(data->checksum_box, FALSE);
            gtk_widget_set_visible(data->items_row, TRUE);

            // Calculate folder size in background
            uint64_t my_gen = data->calculation_gen;
            std::string folder_target = p;
            std::thread([data, folder_target, my_gen]() {
                uint64_t total = 0;
                int count = 0;
                std::error_code ec;
                for (auto it = fs::recursive_directory_iterator(folder_target, fs::directory_options::skip_permission_denied, ec);
                     it != fs::recursive_directory_iterator(); ++it) {
                    if (data->calculation_gen != my_gen) return;
                    if (it->is_regular_file(ec)) total += it->file_size(ec);
                    count++;
                }

                g_idle_add(+[](gpointer ptr) -> gboolean {
                    auto* r = static_cast<DirCalcResultCtx*>(ptr);
                    if (r->data->calculation_gen == r->gen) {
                        gtk_label_set_text(GTK_LABEL(r->data->size_val), FileItem::format_size(r->bytes).c_str());
                        gtk_label_set_text(GTK_LABEL(r->data->items_val), std::to_string(r->count).c_str());
                    }
                    delete r;
                    return G_SOURCE_REMOVE;
                }, new DirCalcResultCtx{data, total, count, my_gen});
            }).detach();
        }

        char timebuf[64];
        struct tm* tm_info = localtime(&st.st_mtime);
        strftime(timebuf, sizeof(timebuf), "%b %d, %Y", tm_info);
        gtk_label_set_text(GTK_LABEL(data->mod_val), timebuf);

        gtk_label_set_text(GTK_LABEL(data->perm_val), format_perm_string(st.st_mode).c_str());

        struct passwd* pw = getpwuid(st.st_uid);
        struct group* gr = getgrgid(st.st_gid);
        std::string own = (pw ? pw->pw_name : std::to_string(st.st_uid)) + ":" + (gr ? gr->gr_name : std::to_string(st.st_gid));
        gtk_label_set_text(GTK_LABEL(data->owner_val), own.c_str());
    }

    // Image dimensions
    if (mime.rfind("image/", 0) == 0) {
        GdkPixbufFormat* fmt = nullptr;
        gint w = 0, h = 0;
        if (gdk_pixbuf_get_file_info(p.c_str(), &w, &h)) {
            std::string dim = std::to_string(w) + " × " + std::to_string(h);
            gtk_label_set_text(GTK_LABEL(data->dim_val), dim.c_str());
            gtk_widget_set_visible(data->dim_row, TRUE);
        } else {
            gtk_widget_set_visible(data->dim_row, FALSE);
        }
    } else {
        gtk_widget_set_visible(data->dim_row, FALSE);
    }
}

} // namespace zenith
