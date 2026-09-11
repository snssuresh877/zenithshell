#pragma once

#include <gtk/gtk.h>
#include <string>
#include <vector>
#include <functional>
#include <memory>

namespace zenith {

enum class ViewMode {
    GRID,
    LIST
};

enum class SortField {
    NAME,
    SIZE,
    TYPE,
    DATE
};

class FileViewWidget {
public:
    using NavigateCallback = std::function<void(const std::string& path)>;
    using StatusCallback = std::function<void(int total_items, int selected_items, uint64_t selected_bytes)>;

    static GtkWidget* create(NavigateCallback on_navigate, StatusCallback on_status);
    static void load_directory(GtkWidget* widget, const std::string& path);
    static std::string get_current_directory(GtkWidget* widget);
    static void refresh(GtkWidget* widget);

    static void set_view_mode(GtkWidget* widget, ViewMode mode);
    static ViewMode get_view_mode(GtkWidget* widget);

    static void set_show_hidden(GtkWidget* widget, bool show);
    static bool get_show_hidden(GtkWidget* widget);

    static void set_search_query(GtkWidget* widget, const std::string& query);

    static void set_sort(GtkWidget* widget, SortField field, bool ascending);

    static std::vector<std::string> get_selected_paths(GtkWidget* widget);
    static void select_all(GtkWidget* widget);
    static void clear_selection(GtkWidget* widget);

    // Operations on current selection or directory
    static void action_open_selected(GtkWidget* widget);
    static void action_cut(GtkWidget* widget);
    static void action_copy(GtkWidget* widget);
    static void action_paste(GtkWidget* widget);
    static void action_rename_selected(GtkWidget* widget);
    static void action_trash_selected(GtkWidget* widget);
    static void action_delete_selected(GtkWidget* widget);
    static void action_new_folder(GtkWidget* widget);
    static void action_new_file(GtkWidget* widget);
    static void action_open_terminal(GtkWidget* widget);
    static void action_properties(GtkWidget* widget);
};

} // namespace zenith
