#pragma once

#include <string>
#include <vector>
#include <functional>
#include <gtk/gtk.h>

namespace zenith {

class FileOperations {
public:
    using OperationCallback = std::function<void(bool success, const std::string& message)>;

    static bool launch_file(const std::string& path);
    static bool open_terminal(const std::string& directory_path);
    static bool create_folder(const std::string& parent_path, const std::string& folder_name, std::string* out_created_path = nullptr);
    static bool create_file(const std::string& parent_path, const std::string& file_name, std::string* out_created_path = nullptr);
    static bool rename_item(const std::string& path, const std::string& new_name, std::string* out_new_path = nullptr);
    static bool move_to_trash(const std::vector<std::string>& paths);
    static bool delete_permanently(const std::vector<std::string>& paths);

    // Standard Desktop Clipboard Integration
    static void copy_to_clipboard(const std::vector<std::string>& paths, bool is_cut);
    static bool paste_from_clipboard(const std::string& target_dir);
    // Paste with GTK progress dialog (shows when total > 1 MB)
    static bool paste_from_clipboard_with_progress(const std::string& target_dir, GtkWindow* parent = nullptr);
    static bool has_clipboard_files();

    // Archive operations (via file-roller)
    static bool is_archive(const std::string& mime_type);
    static void compress_files(const std::vector<std::string>& paths);
    static void extract_archive(const std::string& archive_path, const std::string& target_dir);

    // Open With dialog
    static void open_with_dialog(const std::string& path, GtkWindow* parent = nullptr);
};

} // namespace zenith
