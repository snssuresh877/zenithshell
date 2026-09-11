#include "shell/files/file_item.hpp"
#include <gtk/gtk.h>
#include <iomanip>
#include <sstream>
#include <cmath>
#include <filesystem>
#include <cstring>

namespace fs = std::filesystem;

namespace zenith {

FileItem::~FileItem() {
    if (gicon) {
        g_object_unref(gicon);
        gicon = nullptr;
    }
    if (pixbuf_small) {
        g_object_unref(pixbuf_small);
        pixbuf_small = nullptr;
    }
    if (pixbuf_large) {
        g_object_unref(pixbuf_large);
        pixbuf_large = nullptr;
    }
}

FileItem::FileItem(FileItem&& other) noexcept
    : path(std::move(other.path)),
      name(std::move(other.name)),
      display_name(std::move(other.display_name)),
      uri(std::move(other.uri)),
      mime_type(std::move(other.mime_type)),
      size(other.size),
      formatted_size(std::move(other.formatted_size)),
      mtime(other.mtime),
      formatted_date(std::move(other.formatted_date)),
      is_directory(other.is_directory),
      is_hidden(other.is_hidden),
      is_symlink(other.is_symlink),
      symlink_target(std::move(other.symlink_target)),
      gicon(other.gicon),
      pixbuf_small(other.pixbuf_small),
      pixbuf_large(other.pixbuf_large) {
    other.gicon = nullptr;
    other.pixbuf_small = nullptr;
    other.pixbuf_large = nullptr;
}

FileItem& FileItem::operator=(FileItem&& other) noexcept {
    if (this != &other) {
        if (gicon) g_object_unref(gicon);
        if (pixbuf_small) g_object_unref(pixbuf_small);
        if (pixbuf_large) g_object_unref(pixbuf_large);

        path = std::move(other.path);
        name = std::move(other.name);
        display_name = std::move(other.display_name);
        uri = std::move(other.uri);
        mime_type = std::move(other.mime_type);
        size = other.size;
        formatted_size = std::move(other.formatted_size);
        mtime = other.mtime;
        formatted_date = std::move(other.formatted_date);
        is_directory = other.is_directory;
        is_hidden = other.is_hidden;
        is_symlink = other.is_symlink;
        symlink_target = std::move(other.symlink_target);
        gicon = other.gicon;
        pixbuf_small = other.pixbuf_small;
        pixbuf_large = other.pixbuf_large;

        other.gicon = nullptr;
        other.pixbuf_small = nullptr;
        other.pixbuf_large = nullptr;
    }
    return *this;
}

std::string FileItem::format_size(uint64_t bytes) {
    if (bytes == 0) return "0 B";
    const char* units[] = {"B", "KB", "MB", "GB", "TB"};
    int unit_idx = 0;
    double d_size = static_cast<double>(bytes);
    while (d_size >= 1024.0 && unit_idx < 4) {
        d_size /= 1024.0;
        unit_idx++;
    }
    char buf[64];
    if (unit_idx == 0) {
        snprintf(buf, sizeof(buf), "%lu B", (unsigned long)bytes);
    } else {
        snprintf(buf, sizeof(buf), "%.1f %s", d_size, units[unit_idx]);
    }
    return std::string(buf);
}

std::string FileItem::format_timestamp(time_t t) {
    if (t == 0) return "-";
    struct tm tm_info;
    localtime_r(&t, &tm_info);
    char buf[64];
    strftime(buf, sizeof(buf), "%b %d, %Y  %H:%M", &tm_info);
    return std::string(buf);
}

std::shared_ptr<FileItem> FileItem::from_file_info(GFile* file, GFileInfo* info, int large_icon_size, int small_icon_size) {
    auto item = std::make_shared<FileItem>();

    char* p = g_file_get_path(file);
    if (p) {
        item->path = p;
        g_free(p);
    }
    char* u = g_file_get_uri(file);
    if (u) {
        item->uri = u;
        g_free(u);
    }

    const char* n = g_file_info_get_name(info);
    item->name = n ? n : "";

    const char* dn = g_file_info_get_display_name(info);
    item->display_name = dn ? dn : item->name;

    GFileType ftype = g_file_info_get_file_type(info);
    item->is_directory = (ftype == G_FILE_TYPE_DIRECTORY);
    item->is_symlink = (ftype == G_FILE_TYPE_SYMBOLIC_LINK);
    if (item->is_symlink) {
        const char* target = g_file_info_get_symlink_target(info);
        if (target) item->symlink_target = target;
    }

    item->is_hidden = g_file_info_get_is_hidden(info) || (!item->name.empty() && item->name[0] == '.');

    const char* ctype = g_file_info_get_content_type(info);
    item->mime_type = ctype ? ctype : "application/octet-stream";

    if (item->is_directory) {
        item->size = 0;
        item->formatted_size = "Folder";
    } else {
        item->size = g_file_info_get_size(info);
        item->formatted_size = format_size(item->size);
    }

    GDateTime* dt = g_file_info_get_modification_date_time(info);
    if (dt) {
        item->mtime = g_date_time_to_unix(dt);
        g_date_time_unref(dt);
    } else {
        item->mtime = 0;
    }
    item->formatted_date = format_timestamp(item->mtime);

    // Resolve icons
    GIcon* icon = g_file_info_get_icon(info);
    if (icon) {
        item->gicon = G_ICON(g_object_ref(icon));
    }

    GtkIconTheme* theme = gtk_icon_theme_get_default();
    if (item->gicon && theme) {
        GtkIconInfo* info_lg = gtk_icon_theme_lookup_by_gicon(
            theme, item->gicon, large_icon_size,
            static_cast<GtkIconLookupFlags>(GTK_ICON_LOOKUP_USE_BUILTIN | GTK_ICON_LOOKUP_FORCE_SIZE)
        );
        if (info_lg) {
            item->pixbuf_large = gtk_icon_info_load_icon(info_lg, nullptr);
            g_object_unref(info_lg);
        }

        GtkIconInfo* info_sm = gtk_icon_theme_lookup_by_gicon(
            theme, item->gicon, small_icon_size,
            static_cast<GtkIconLookupFlags>(GTK_ICON_LOOKUP_USE_BUILTIN | GTK_ICON_LOOKUP_FORCE_SIZE)
        );
        if (info_sm) {
            item->pixbuf_small = gtk_icon_info_load_icon(info_sm, nullptr);
            g_object_unref(info_sm);
        }
    }

    // Fallbacks if icon lookup returned null
    if (!item->pixbuf_large && theme) {
        const char* fallback = item->is_directory ? "folder" : "text-x-generic";
        item->pixbuf_large = gtk_icon_theme_load_icon(theme, fallback, large_icon_size, GTK_ICON_LOOKUP_FORCE_SIZE, nullptr);
    }
    if (!item->pixbuf_small && theme) {
        const char* fallback = item->is_directory ? "folder" : "text-x-generic";
        item->pixbuf_small = gtk_icon_theme_load_icon(theme, fallback, small_icon_size, GTK_ICON_LOOKUP_FORCE_SIZE, nullptr);
    }

    // NOTE: Image/video thumbnails are generated asynchronously by the
    // ThumbnailLoader in file_view_widget.cpp after the directory is shown.
    // This keeps load_directory() fast and non-blocking for any folder size.

    return item;
}

// ── Async thumbnail loading ───────────────────────────────────────────────────
// Called from a GThreadPool worker thread (NOT the GTK main thread).
// Returns a pixbuf or nullptr — caller g_idle_adds the result to main thread.
GdkPixbuf* FileItem::load_thumbnail(const std::string& path, const std::string& uri,
                                     const std::string& mime_type, int size) {
    // Step 1: Check Freedesktop thumbnail cache (md5 of the file:// URI)
    const char* home_dir = g_get_home_dir();
    if (home_dir && !uri.empty()) {
        gchar* md5 = g_compute_checksum_for_string(G_CHECKSUM_MD5, uri.c_str(), -1);
        if (md5) {
            for (const char* sz : {"large", "normal"}) {
                std::string cache = std::string(home_dir) +
                                    "/.cache/thumbnails/" + sz + "/" + md5 + ".png";
                std::error_code ec;
                if (fs::exists(cache, ec)) {
                    GdkPixbuf* pb = gdk_pixbuf_new_from_file_at_scale(
                        cache.c_str(), size, size, TRUE, nullptr);
                    if (pb) { g_free(md5); return pb; }
                }
            }
            g_free(md5);
        }
    }

    // Step 2: For raster images, decode directly (safe — runs on worker thread)
    bool is_raster = (mime_type == "image/jpeg" || mime_type == "image/png"  ||
                      mime_type == "image/gif"  || mime_type == "image/webp"  ||
                      mime_type == "image/bmp"  || mime_type == "image/tiff"  ||
                      mime_type == "image/x-bmp" || mime_type == "image/svg+xml");
    if (is_raster && !path.empty()) {
        GError* err = nullptr;
        GdkPixbuf* pb = gdk_pixbuf_new_from_file_at_scale(
            path.c_str(), size, size, TRUE, &err);
        if (err) g_error_free(err);
        return pb; // nullptr if decode failed
    }

    return nullptr;
}

} // namespace zenith
