#include "shell/files/inspector_panel.hpp"
#include "shell/files/file_item.hpp"
#include "gtk3_compat.hpp"
#include <gio/gio.h>
#include <filesystem>
#include <sys/stat.h>
#include <pwd.h>
#include <grp.h>
#include <thread>
#include <iostream>

namespace fs = std::filesystem;

namespace zenith {

struct InspectorData {
    GtkWidget* root_box{nullptr};
    GtkWidget* icon_preview{nullptr};
    GtkWidget* title_label{nullptr};
    GtkWidget* type_label{nullptr};

    GtkWidget* grid{nullptr};
    GtkWidget* size_val{nullptr};
    GtkWidget* dim_row{nullptr};
    GtkWidget* dim_val{nullptr};
    GtkWidget* mod_val{nullptr};
    GtkWidget* loc_val{nullptr};
    GtkWidget* perm_val{nullptr};
    GtkWidget* owner_val{nullptr};

    GtkWidget* checksum_box{nullptr};
    GtkWidget* checksum_btn{nullptr};
    GtkWidget* sha256_val{nullptr};
    GtkWidget* md5_val{nullptr};

    std::string current_path;
    uint64_t calculation_gen{0};
};

struct ChecksumResultCtx {
    InspectorData* data;
    std::string sha;
    std::string md5;
    uint64_t gen;
    GtkButton* btn;
};

struct DirCalcResultCtx {
    InspectorData* data;
    uint64_t bytes;
    int count;
    uint64_t gen;
};

static std::string format_perm_string(mode_t mode) {
    std::string p = (S_ISDIR(mode)) ? "d" : "-";
    p += (mode & S_IRUSR) ? "r" : "-";
    p += (mode & S_IWUSR) ? "w" : "-";
    p += (mode & S_IXUSR) ? "x" : "-";
    p += (mode & S_IRGRP) ? "r" : "-";
    p += (mode & S_IWGRP) ? "w" : "-";
    p += (mode & S_IXGRP) ? "x" : "-";
    p += (mode & S_IROTH) ? "r" : "-";
    p += (mode & S_IWOTH) ? "w" : "-";
    p += (mode & S_IXOTH) ? "x" : "-";
    return p;
}

static std::string calculate_file_checksum(const std::string& path, GChecksumType type) {
    GChecksum* cs = g_checksum_new(type);
    if (!cs) return "";

    FILE* fp = fopen(path.c_str(), "rb");
    if (!fp) {
        g_checksum_free(cs);
        return "";
    }

    guchar buffer[8192];
    size_t n = 0;
    while ((n = fread(buffer, 1, sizeof(buffer), fp)) > 0) {
        g_checksum_update(cs, buffer, n);
    }
    fclose(fp);

    const char* str = g_checksum_get_string(cs);
    std::string res = str ? str : "";
    g_checksum_free(cs);
    return res;
}

static GtkWidget* create_meta_row(GtkWidget* grid, int row, const char* label, GtkWidget** out_val) {
    GtkWidget* key = gtk_label_new(label);
    gtk_label_set_xalign(GTK_LABEL(key), 0.0f);
    gtk_widget_add_css_class(key, "files-prop-key");
    gtk_grid_attach(GTK_GRID(grid), key, 0, row, 1, 1);

    GtkWidget* val = gtk_label_new("—");
    gtk_label_set_xalign(GTK_LABEL(val), 0.0f);
    gtk_label_set_line_wrap(GTK_LABEL(val), TRUE);
    gtk_label_set_ellipsize(GTK_LABEL(val), PANGO_ELLIPSIZE_MIDDLE);
    gtk_widget_add_css_class(val, "files-prop-val");
    gtk_grid_attach(GTK_GRID(grid), val, 1, row, 1, 1);

    if (out_val) *out_val = val;
    return key;
}

GtkWidget* InspectorPanel::create() {
    auto* data = new InspectorData();

    GtkWidget* root = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    data->root_box = root;
    gtk_widget_add_css_class(root, "files-inspector-panel");
    gtk_widget_set_size_request(root, 280, -1);

    // Header
    GtkWidget* header_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_container_set_border_width(GTK_CONTAINER(header_box), 12);
    gtk_widget_add_css_class(header_box, "files-inspector-header");

    GtkWidget* hdr_lbl = gtk_label_new("INSPECTOR");
    gtk_label_set_xalign(GTK_LABEL(hdr_lbl), 0.0f);
    gtk_widget_add_css_class(hdr_lbl, "files-prop-title");
    gtk_box_pack_start(GTK_BOX(header_box), hdr_lbl, TRUE, TRUE, 0);

    gtk_box_pack_start(GTK_BOX(root), header_box, FALSE, FALSE, 0);

    // Scrolled Content
    GtkWidget* scroll = gtk_scrolled_window_new(nullptr, nullptr);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_widget_set_vexpand(scroll, TRUE);

    GtkWidget* content_vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 14);
    gtk_container_set_border_width(GTK_CONTAINER(content_vbox), 14);

    // Preview
    data->icon_preview = gtk_image_new_from_icon_name("folder-symbolic", GTK_ICON_SIZE_DIALOG);
    gtk_widget_set_size_request(data->icon_preview, 96, 96);
    gtk_widget_add_css_class(data->icon_preview, "files-inspector-preview");
    gtk_box_pack_start(GTK_BOX(content_vbox), data->icon_preview, FALSE, FALSE, 0);

    // Title & Type
    data->title_label = gtk_label_new("No Selection");
    gtk_label_set_xalign(GTK_LABEL(data->title_label), 0.5f);
    gtk_label_set_line_wrap(GTK_LABEL(data->title_label), TRUE);
    gtk_widget_add_css_class(data->title_label, "files-inspector-title");
    gtk_box_pack_start(GTK_BOX(content_vbox), data->title_label, FALSE, FALSE, 0);

    data->type_label = gtk_label_new("Folder");
    gtk_label_set_xalign(GTK_LABEL(data->type_label), 0.5f);
    gtk_widget_add_css_class(data->type_label, "files-prop-subtitle");
    gtk_box_pack_start(GTK_BOX(content_vbox), data->type_label, FALSE, FALSE, 0);

    gtk_box_pack_start(GTK_BOX(content_vbox), gtk_separator_new(GTK_ORIENTATION_HORIZONTAL), FALSE, FALSE, 4);

    // Metadata Grid
    data->grid = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(data->grid), 8);
    gtk_grid_set_column_spacing(GTK_GRID(data->grid), 12);

    int r = 0;
    create_meta_row(data->grid, r++, "Size", &data->size_val);
    data->dim_row = create_meta_row(data->grid, r++, "Dimensions", &data->dim_val);
    create_meta_row(data->grid, r++, "Modified", &data->mod_val);
    create_meta_row(data->grid, r++, "Location", &data->loc_val);
    create_meta_row(data->grid, r++, "Permissions", &data->perm_val);
    create_meta_row(data->grid, r++, "Owner", &data->owner_val);

    gtk_box_pack_start(GTK_BOX(content_vbox), data->grid, FALSE, FALSE, 0);

    gtk_box_pack_start(GTK_BOX(content_vbox), gtk_separator_new(GTK_ORIENTATION_HORIZONTAL), FALSE, FALSE, 4);

    // Checksum Section
    data->checksum_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    GtkWidget* cs_hdr = gtk_label_new("<b>Checksums</b>");
    gtk_label_set_use_markup(GTK_LABEL(cs_hdr), TRUE);
    gtk_label_set_xalign(GTK_LABEL(cs_hdr), 0.0f);
    gtk_widget_add_css_class(cs_hdr, "files-prop-key");
    gtk_box_pack_start(GTK_BOX(data->checksum_box), cs_hdr, FALSE, FALSE, 0);

    data->checksum_btn = gtk_button_new_with_label("Calculate Hashes");
    gtk_widget_add_css_class(data->checksum_btn, "files-btn-tool");
    gtk_box_pack_start(GTK_BOX(data->checksum_box), data->checksum_btn, FALSE, FALSE, 0);

    GtkWidget* cs_grid = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(cs_grid), 4);
    gtk_grid_set_column_spacing(GTK_GRID(cs_grid), 8);

    create_meta_row(cs_grid, 0, "SHA-256", &data->sha256_val);
    create_meta_row(cs_grid, 1, "MD5", &data->md5_val);
    gtk_box_pack_start(GTK_BOX(data->checksum_box), cs_grid, FALSE, FALSE, 0);

    g_signal_connect(data->checksum_btn, "clicked", G_CALLBACK(+[](GtkButton* b, gpointer ud) {
        auto* d = static_cast<InspectorData*>(ud);
        if (d->current_path.empty()) return;

        gtk_button_set_label(b, "Computing…");
        gtk_widget_set_sensitive(GTK_WIDGET(b), FALSE);

        std::string target = d->current_path;
        uint64_t my_gen = d->calculation_gen;

        std::thread([d, target, my_gen, b]() {
            std::string sha = calculate_file_checksum(target, G_CHECKSUM_SHA256);
            std::string md5 = calculate_file_checksum(target, G_CHECKSUM_MD5);

            g_idle_add(+[](gpointer p) -> gboolean {
                auto* ctx = static_cast<ChecksumResultCtx*>(p);
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

    gtk_box_pack_start(GTK_BOX(content_vbox), data->checksum_box, FALSE, FALSE, 0);

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
    data->calculation_gen++;

    gtk_image_set_from_icon_name(GTK_IMAGE(data->icon_preview), "folder", GTK_ICON_SIZE_DIALOG);
    
    std::string base = directory_path;
    size_t s = directory_path.find_last_of('/');
    if (s != std::string::npos && s + 1 < directory_path.length()) base = directory_path.substr(s + 1);
    if (base.empty()) base = "/";

    gtk_label_set_text(GTK_LABEL(data->title_label), base.c_str());
    gtk_label_set_text(GTK_LABEL(data->type_label), "Folder");
    gtk_label_set_text(GTK_LABEL(data->loc_val), directory_path.c_str());
    gtk_widget_set_visible(data->dim_row, FALSE);
    gtk_widget_set_visible(data->checksum_box, FALSE);

    struct stat st;
    if (stat(directory_path.c_str(), &st) == 0) {
        gtk_label_set_text(GTK_LABEL(data->perm_val), format_perm_string(st.st_mode).c_str());
        struct passwd* pw = getpwuid(st.st_uid);
        struct group* gr = getgrgid(st.st_gid);
        std::string own = (pw ? pw->pw_name : std::to_string(st.st_uid)) + ":" + (gr ? gr->gr_name : std::to_string(st.st_gid));
        gtk_label_set_text(GTK_LABEL(data->owner_val), own.c_str());

        char timebuf[64];
        struct tm* tm_info = localtime(&st.st_mtime);
        strftime(timebuf, sizeof(timebuf), "%b %d, %Y %H:%M", tm_info);
        gtk_label_set_text(GTK_LABEL(data->mod_val), timebuf);
    }
}

void InspectorPanel::update_selection(GtkWidget* panel, const std::vector<std::string>& selected_paths) {
    if (!panel) return;
    auto* data = static_cast<InspectorData*>(g_object_get_data(G_OBJECT(panel), "inspector_data"));
    if (!data) return;

    data->calculation_gen++;

    if (selected_paths.empty()) {
        set_current_directory(panel, data->current_path);
        return;
    }

    if (selected_paths.size() > 1) {
        gtk_image_set_from_icon_name(GTK_IMAGE(data->icon_preview), "emblem-documents", GTK_ICON_SIZE_DIALOG);
        gtk_label_set_text(GTK_LABEL(data->title_label), (std::to_string(selected_paths.size()) + " items selected").c_str());
        gtk_label_set_text(GTK_LABEL(data->type_label), "Multiple Selection");

        uint64_t total_size = 0;
        std::error_code ec;
        for (const auto& p : selected_paths) {
            if (fs::is_regular_file(p, ec)) total_size += fs::file_size(p, ec);
        }
        gtk_label_set_text(GTK_LABEL(data->size_val), FileItem::format_size(total_size).c_str());
        gtk_widget_set_visible(data->dim_row, FALSE);
        gtk_widget_set_visible(data->checksum_box, FALSE);
        return;
    }

    // Single item selection
    const std::string& p = selected_paths[0];
    data->current_path = p;

    std::string filename = fs::path(p).filename().string();
    if (filename.empty()) filename = p;

    gtk_label_set_text(GTK_LABEL(data->title_label), filename.c_str());
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

    // Icon / Thumbnail preview
    GdkPixbuf* thumb = FileItem::load_thumbnail(p, "file://" + p, mime, 96);
    if (thumb) {
        gtk_image_set_from_pixbuf(GTK_IMAGE(data->icon_preview), thumb);
        g_object_unref(thumb);
    } else {
        gtk_image_set_from_icon_name(GTK_IMAGE(data->icon_preview), is_dir ? "folder" : "text-x-generic", GTK_ICON_SIZE_DIALOG);
    }

    struct stat st;
    if (stat(p.c_str(), &st) == 0) {
        if (!is_dir) {
            gtk_label_set_text(GTK_LABEL(data->size_val), FileItem::format_size(st.st_size).c_str());
            gtk_widget_set_visible(data->checksum_box, TRUE);
            gtk_label_set_text(GTK_LABEL(data->sha256_val), "—");
            gtk_label_set_text(GTK_LABEL(data->md5_val), "—");
        } else {
            gtk_label_set_text(GTK_LABEL(data->size_val), "Calculating…");
            gtk_widget_set_visible(data->checksum_box, FALSE);

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

                g_idle_add(+[](gpointer p) -> gboolean {
                    auto* r = static_cast<DirCalcResultCtx*>(p);
                    if (r->data->calculation_gen == r->gen) {
                        std::string s = FileItem::format_size(r->bytes) + " (" + std::to_string(r->count) + " items)";
                        gtk_label_set_text(GTK_LABEL(r->data->size_val), s.c_str());
                    }
                    delete r;
                    return G_SOURCE_REMOVE;
                }, new DirCalcResultCtx{data, total, count, my_gen});
            }).detach();
        }

        char timebuf[64];
        struct tm* tm_info = localtime(&st.st_mtime);
        strftime(timebuf, sizeof(timebuf), "%b %d, %Y %H:%M", tm_info);
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
