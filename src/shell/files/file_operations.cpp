#include "shell/files/file_operations.hpp"
#include "gtk3_compat.hpp"
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

// ── Progress-aware paste ──────────────────────────────────────────────────────

struct PasteProgressCtx {
    GtkWidget*   dialog{nullptr};
    GtkWidget*   bar{nullptr};
    GtkWidget*   label{nullptr};
    GCancellable* cancellable{nullptr};
    bool         cancelled{false};
    int          file_num{0};
    int          file_total{0};
    std::string  current_name;
};

static void on_file_progress(goffset current, goffset total, gpointer user_data) {
    auto* ctx = static_cast<PasteProgressCtx*>(user_data);
    if (!ctx || !GTK_IS_WIDGET(ctx->dialog)) return;

    if (ctx->cancelled) {
        if (ctx->cancellable) g_cancellable_cancel(ctx->cancellable);
        return;
    }

    if (total > 0) {
        double frac = static_cast<double>(current) / static_cast<double>(total);
        gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(ctx->bar), frac);
    } else {
        gtk_progress_bar_pulse(GTK_PROGRESS_BAR(ctx->bar));
    }

    std::string txt = "Copying: " + ctx->current_name +
                      " (" + std::to_string(ctx->file_num) + "/" +
                      std::to_string(ctx->file_total) + ")";
    gtk_label_set_text(GTK_LABEL(ctx->label), txt.c_str());

    // Pump GTK events to keep Cancel button responsive
    while (gtk_events_pending()) gtk_main_iteration_do(FALSE);
}

bool FileOperations::paste_from_clipboard_with_progress(const std::string& target_dir, GtkWindow* parent) {
    if (s_clipboard_paths.empty() || target_dir.empty()) return false;

    // Calculate total size to decide whether to show dialog
    uint64_t total_size = 0;
    std::error_code ec;
    for (const auto& p : s_clipboard_paths) {
        if (fs::is_regular_file(p, ec)) total_size += fs::file_size(p, ec);
    }

    bool show_dialog = (total_size > 1024 * 1024); // show for > 1 MB

    PasteProgressCtx ctx;
    ctx.file_total = static_cast<int>(s_clipboard_paths.size());

    if (show_dialog) {
        ctx.cancellable = g_cancellable_new();

        ctx.dialog = gtk_dialog_new_with_buttons(
            s_clipboard_is_cut ? "Moving Files…" : "Copying Files…",
            parent,
            static_cast<GtkDialogFlags>(GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT),
            "_Cancel", GTK_RESPONSE_CANCEL,
            nullptr
        );
        gtk_window_set_default_size(GTK_WINDOW(ctx.dialog), 400, -1);
        gtk_widget_add_css_class(ctx.dialog, "zenith-files-dialog");

        GtkWidget* content = gtk_dialog_get_content_area(GTK_DIALOG(ctx.dialog));
        gtk_container_set_border_width(GTK_CONTAINER(content), 20);
        gtk_box_set_spacing(GTK_BOX(content), 12);

        ctx.label = gtk_label_new("Preparing…");
        gtk_label_set_xalign(GTK_LABEL(ctx.label), 0.0f);
        gtk_label_set_ellipsize(GTK_LABEL(ctx.label), PANGO_ELLIPSIZE_MIDDLE);
        gtk_box_pack_start(GTK_BOX(content), ctx.label, FALSE, FALSE, 0);

        ctx.bar = gtk_progress_bar_new();
        gtk_widget_add_css_class(ctx.bar, "zenith-osd-progress");
        gtk_box_pack_start(GTK_BOX(content), ctx.bar, FALSE, FALSE, 0);

        g_signal_connect(ctx.dialog, "response", G_CALLBACK(+[](GtkDialog*, gint resp, gpointer ud) {
            if (resp == GTK_RESPONSE_CANCEL) {
                auto* c = static_cast<PasteProgressCtx*>(ud);
                c->cancelled = true;
                if (c->cancellable) g_cancellable_cancel(c->cancellable);
            }
        }), &ctx);

        gtk_widget_show_all(ctx.dialog);
        while (gtk_events_pending()) gtk_main_iteration_do(FALSE);
    }

    bool all_ok = true;

    for (const auto& src_path : s_clipboard_paths) {
        if (ctx.cancelled) { all_ok = false; break; }
        if (!fs::exists(src_path, ec)) continue;

        ctx.file_num++;
        ctx.current_name = fs::path(src_path).filename().string();

        fs::path src_f(src_path);
        std::string filename = src_f.filename().string();
        fs::path dst_f = fs::path(target_dir) / filename;

        int count = 1;
        while (fs::exists(dst_f, ec)) {
            std::string stem = src_f.stem().string();
            std::string ext  = src_f.extension().string();
            dst_f = fs::path(target_dir) / (stem + " (" + std::to_string(count++) + ")" + ext);
        }

        GFile* src = g_file_new_for_path(src_path.c_str());
        GFile* dst = g_file_new_for_path(dst_f.string().c_str());

        if (src && dst) {
            GError* err = nullptr;
            GFileCopyFlags flags = G_FILE_COPY_NONE;

            if (s_clipboard_is_cut) {
                if (!g_file_move(src, dst, flags, ctx.cancellable,
                                 show_dialog ? on_file_progress : nullptr, &ctx, &err)) {
                    all_ok = false;
                    if (err) { g_error_free(err); err = nullptr; }
                }
            } else {
                if (fs::is_directory(src_path, ec)) {
                    fs::copy(src_f, dst_f, fs::copy_options::recursive, ec);
                    if (ec) all_ok = false;
                } else {
                    if (!g_file_copy(src, dst, flags, ctx.cancellable,
                                     show_dialog ? on_file_progress : nullptr, &ctx, &err)) {
                        all_ok = false;
                        if (err) { g_error_free(err); err = nullptr; }
                    }
                }
            }
        }

        if (src) g_object_unref(src);
        if (dst) g_object_unref(dst);
    }

    if (show_dialog) {
        if (ctx.cancellable) g_object_unref(ctx.cancellable);
        if (ctx.dialog && GTK_IS_WIDGET(ctx.dialog)) {
            gtk_widget_destroy(ctx.dialog);
        }
    }

    if (s_clipboard_is_cut && all_ok) {
        s_clipboard_paths.clear();
        s_clipboard_is_cut = false;
    }

    return all_ok;
}

// ── Archive operations ────────────────────────────────────────────────────────

bool FileOperations::is_archive(const std::string& mime_type) {
    static const char* archive_mimes[] = {
        "application/zip", "application/x-tar", "application/x-compressed-tar",
        "application/x-bzip2-compressed-tar", "application/x-xz-compressed-tar",
        "application/x-7z-compressed", "application/x-rar",
        "application/x-rar-compressed", "application/gzip",
        "application/x-bzip2", "application/x-xz",
        "application/x-lzma", "application/zstd",
        "application/vnd.debian.binary-package", "application/x-rpm",
        "application/x-archive", nullptr
    };
    for (int i = 0; archive_mimes[i]; ++i) {
        if (mime_type == archive_mimes[i]) return true;
    }
    return false;
}

void FileOperations::compress_files(const std::vector<std::string>& paths) {
    if (paths.empty()) return;

    // Prefer file-roller GUI; fall back to zip CLI
    std::string cmd;
    bool has_file_roller = (system("which file-roller >/dev/null 2>&1") == 0);

    if (has_file_roller) {
        cmd = "file-roller --add";
        for (const auto& p : paths) {
            cmd += " \"" + p + "\"";
        }
    } else {
        // CLI fallback: zip everything into archive.zip in same dir
        std::string parent = fs::path(paths[0]).parent_path().string();
        cmd = "cd \"" + parent + "\" && zip -r \"archive.zip\"";
        for (const auto& p : paths) {
            cmd += " \"" + fs::path(p).filename().string() + "\"";
        }
    }
    cmd += " >/dev/null 2>&1 &";
    int ret = system(cmd.c_str());
    (void)ret;
}

void FileOperations::extract_archive(const std::string& archive_path, const std::string& target_dir) {
    if (archive_path.empty()) return;

    std::string cmd;
    bool has_file_roller = (system("which file-roller >/dev/null 2>&1") == 0);

    if (has_file_roller) {
        cmd = "file-roller --extract-to=\"" + target_dir + "\" \"" + archive_path + "\"";
    } else {
        // CLI fallback: detect type and use tar/unzip
        if (archive_path.rfind(".zip") == archive_path.size() - 4) {
            cmd = "unzip -o \"" + archive_path + "\" -d \"" + target_dir + "\"";
        } else {
            cmd = "tar -xf \"" + archive_path + "\" -C \"" + target_dir + "\"";
        }
    }
    cmd += " >/dev/null 2>&1 &";
    int ret = system(cmd.c_str());
    (void)ret;
}

// ── Open With dialog ──────────────────────────────────────────────────────────

void FileOperations::open_with_dialog(const std::string& path, GtkWindow* parent) {
    if (path.empty()) return;

    GFile* gf = g_file_new_for_path(path.c_str());
    GFileInfo* fi = g_file_query_info(gf, "standard::content-type", G_FILE_QUERY_INFO_NONE, nullptr, nullptr);

    const char* ctype = fi ? g_file_info_get_content_type(fi) : "application/octet-stream";

    GtkWidget* dialog = gtk_app_chooser_dialog_new_for_content_type(
        parent,
        static_cast<GtkDialogFlags>(GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT),
        ctype
    );
    gtk_widget_add_css_class(dialog, "zenith-files-dialog");

    gint response = gtk_dialog_run(GTK_DIALOG(dialog));

    if (response == GTK_RESPONSE_OK) {
        GAppInfo* app = gtk_app_chooser_get_app_info(GTK_APP_CHOOSER(dialog));
        if (app) {
            GList* files = g_list_append(nullptr, gf);
            GError* err = nullptr;
            g_app_info_launch(app, files, nullptr, &err);
            if (err) { g_error_free(err); }
            g_list_free(files);
            g_object_unref(app);
        }
    }

    gtk_widget_destroy(dialog);
    if (fi) g_object_unref(fi);
    g_object_unref(gf);
}

} // namespace zenith
