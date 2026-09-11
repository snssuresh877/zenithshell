#pragma once

#include <gio/gio.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <string>
#include <memory>
#include <ctime>

namespace zenith {

struct FileItem {
    std::string path;
    std::string name;
    std::string display_name;
    std::string uri;
    std::string mime_type;
    uint64_t size{0};
    std::string formatted_size;
    time_t mtime{0};
    std::string formatted_date;
    bool is_directory{false};
    bool is_hidden{false};
    bool is_symlink{false};
    std::string symlink_target;

    GIcon* gicon{nullptr};
    GdkPixbuf* pixbuf_small{nullptr};
    GdkPixbuf* pixbuf_large{nullptr};

    FileItem() = default;
    ~FileItem();

    // Move semantics
    FileItem(FileItem&& other) noexcept;
    FileItem& operator=(FileItem&& other) noexcept;

    // Disallow simple copy to avoid dangling GObject pointers
    FileItem(const FileItem&) = delete;
    FileItem& operator=(const FileItem&) = delete;

    static std::shared_ptr<FileItem> from_file_info(GFile* file, GFileInfo* info, int large_icon_size = 48, int small_icon_size = 20);
    static std::string format_size(uint64_t bytes);
    static std::string format_timestamp(time_t t);
};

} // namespace zenith
