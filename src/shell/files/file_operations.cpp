#include "shell/files/file_operations.hpp"
#include <gio/gio.h>
#include <gtk/gtk.h>
#include <filesystem>
#include <fstream>
#include <iostream>

namespace fs = std::filesystem;

namespace zenith {

static std::vector<std::string> s_clipboard_paths;
static bool s_clipboard_is_cut = false;

bool FileOperations::launch_file(const std::string& path) {
    if (path.empty()) return false;

    GFile* file = g_file_new_for_path(path.c_str());
    if (!file) return false;

    char* uri = g_file_get_uri(file);
    g_object_unref(file);
    if (!uri) return false;

    GError* error = nullptr;
    gboolean success = g_app_info_launch_default_for_uri(uri, nullptr, &error);
    g_free(uri);

    if (error) {
        std::cerr << "[ZenithFiles] Error launching " << path << ": " << error->message << std::endl;
        g_error_free(error);
        return false;
    }
    return success;
}

bool FileOperations::open_terminal(const std::string& directory_path) {
    std::string dir = directory_path.empty() ? g_get_home_dir() : directory_path;
    std::string cmd = "foot --working-directory=\"" + dir + "\" 2>/dev/null &";
    int ret = system(cmd.c_str());
    (void)ret;
    return true;
}

bool FileOperations::create_folder(const std::string& parent_path, const std::string& folder_name, std::string* out_created_path) {
    if (parent_path.empty() || folder_name.empty()) return false;

    std::error_code ec;
    fs::path p = fs::path(parent_path) / folder_name;

    // Handle existing name collision with (1), (2), etc.
    int count = 1;
    while (fs::exists(p, ec)) {
        p = fs::path(parent_path) / (folder_name + " (" + std::to_string(count++) + ")");
    }

    if (fs::create_directory(p, ec)) {
        if (out_created_path) *out_created_path = p.string();
        return true;
    }
    return false;
}

bool FileOperations::create_file(const std::string& parent_path, const std::string& file_name, std::string* out_created_path) {
    if (parent_path.empty() || file_name.empty()) return false;

    std::error_code ec;
    fs::path p = fs::path(parent_path) / file_name;

    int count = 1;
    std::string base = fs::path(file_name).stem().string();
    std::string ext = fs::path(file_name).extension().string();

    while (fs::exists(p, ec)) {
        p = fs::path(parent_path) / (base + " (" + std::to_string(count++) + ")" + ext);
    }

    std::ofstream out(p);
    if (out.is_open()) {
        out.close();
        if (out_created_path) *out_created_path = p.string();
        return true;
    }
    return false;
}

bool FileOperations::rename_item(const std::string& path, const std::string& new_name, std::string* out_new_path) {
    if (path.empty() || new_name.empty()) return false;

    GFile* src = g_file_new_for_path(path.c_str());
    if (!src) return false;

    GError* error = nullptr;
    GFile* dst = g_file_set_display_name(src, new_name.c_str(), nullptr, &error);
    g_object_unref(src);

    if (error) {
        std::cerr << "[ZenithFiles] Rename error: " << error->message << std::endl;
        g_error_free(error);
        if (dst) g_object_unref(dst);
        return false;
    }

    if (dst) {
        char* new_p = g_file_get_path(dst);
        if (new_p) {
            if (out_new_path) *out_new_path = new_p;
            g_free(new_p);
        }
        g_object_unref(dst);
        return true;
    }
    return false;
}

bool FileOperations::move_to_trash(const std::vector<std::string>& paths) {
    bool all_ok = true;
    for (const auto& p : paths) {
        if (p.empty()) continue;
        GFile* f = g_file_new_for_path(p.c_str());
        if (f) {
            GError* err = nullptr;
            if (!g_file_trash(f, nullptr, &err)) {
                if (err) {
                    std::cerr << "[ZenithFiles] Trash error on " << p << ": " << err->message << std::endl;
                    g_error_free(err);
                }
                all_ok = false;
            }
            g_object_unref(f);
        }
    }
    return all_ok;
}

bool FileOperations::delete_permanently(const std::vector<std::string>& paths) {
    bool all_ok = true;
    std::error_code ec;
    for (const auto& p : paths) {
        if (p.empty()) continue;
        if (!fs::remove_all(p, ec)) {
            all_ok = false;
        }
    }
    return all_ok;
}

void FileOperations::copy_to_clipboard(const std::vector<std::string>& paths, bool is_cut) {
    s_clipboard_paths = paths;
    s_clipboard_is_cut = is_cut;

    // Set URI list in standard GtkClipboard
    GtkClipboard* clip = gtk_clipboard_get(GDK_SELECTION_CLIPBOARD);
    if (clip && !paths.empty()) {
        std::string uris;
        for (const auto& p : paths) {
            GFile* f = g_file_new_for_path(p.c_str());
            if (f) {
                char* u = g_file_get_uri(f);
                if (u) {
                    uris += u;
                    uris += "\r\n";
                    g_free(u);
                }
                g_object_unref(f);
            }
        }
        gtk_clipboard_set_text(clip, uris.c_str(), -1);
    }
}

bool FileOperations::has_clipboard_files() {
    return !s_clipboard_paths.empty();
}

bool FileOperations::paste_from_clipboard(const std::string& target_dir) {
    if (s_clipboard_paths.empty() || target_dir.empty()) return false;

    GFile* target_folder = g_file_new_for_path(target_dir.c_str());
    if (!target_folder) return false;

    bool all_ok = true;
    std::error_code ec;

    for (const auto& src_path : s_clipboard_paths) {
        if (!fs::exists(src_path, ec)) continue;

        fs::path src_f(src_path);
        std::string filename = src_f.filename().string();
        fs::path dst_f = fs::path(target_dir) / filename;

        // Collision avoidance
        int count = 1;
        while (fs::exists(dst_f, ec)) {
            std::string stem = src_f.stem().string();
            std::string ext = src_f.extension().string();
            dst_f = fs::path(target_dir) / (stem + " (" + std::to_string(count++) + ")" + ext);
        }

        GFile* src = g_file_new_for_path(src_path.c_str());
        GFile* dst = g_file_new_for_path(dst_f.string().c_str());

        if (src && dst) {
            GError* err = nullptr;
            if (s_clipboard_is_cut) {
                if (!g_file_move(src, dst, G_FILE_COPY_NONE, nullptr, nullptr, nullptr, &err)) {
                    all_ok = false;
                    if (err) { g_error_free(err); err = nullptr; }
                }
            } else {
                if (fs::is_directory(src_path, ec)) {
                    fs::copy(src_f, dst_f, fs::copy_options::recursive, ec);
                    if (ec) all_ok = false;
                } else {
                    if (!g_file_copy(src, dst, G_FILE_COPY_NONE, nullptr, nullptr, nullptr, &err)) {
                        all_ok = false;
                        if (err) { g_error_free(err); err = nullptr; }
                    }
                }
            }
            g_object_unref(dst);
            g_object_unref(src);
        }
    }

    g_object_unref(target_folder);

    if (s_clipboard_is_cut) {
        s_clipboard_paths.clear();
        s_clipboard_is_cut = false;
    }

    return all_ok;
}

} // namespace zenith
