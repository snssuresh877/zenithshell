#pragma once

#include <gtk/gtk.h>
#include <gio/gio.h>
#include <string>
#include <vector>

namespace zenith {

enum class AppCategory {
    ALL,
    WEB,
    MULTIMEDIA,
    DEVELOPMENT,
    GRAPHICS,
    OFFICE,
    SYSTEM,
    SETTINGS,
    FILES
};

struct SearchResult {
    std::string title;
    std::string subtitle;
    GIcon* icon = nullptr;
    std::string icon_name = "application-x-executable";
    std::string action_cmd;
    GAppInfo* app_info = nullptr;
    enum Type { APP, MATH, SYSTEM, WEB } type = APP;
};

class SpotlightSearch {
public:
    static void init(GtkApplication* app);
    static void toggle();
    static void show();
    static void hide();

private:
    static GtkWidget* window;
    static GtkWidget* search_entry;
    static GtkWidget* results_listbox;
    static std::vector<SearchResult> current_results;
    static std::vector<GAppInfo*> installed_apps;
    static AppCategory current_category;
    static std::vector<GtkWidget*> category_buttons;

    static void load_apps();
    static void set_category(AppCategory cat);
    static bool app_matches_category(GAppInfo* app, AppCategory cat);
    static void on_search_changed(GtkEntry* entry, gpointer user_data);
    static void on_row_activated(GtkListBox* listbox, GtkListBoxRow* row, gpointer user_data);
    static gboolean on_key_pressed(GtkWidget* widget, GdkEventKey* event, gpointer user_data);
    static void execute_result(const SearchResult& result);
    static std::string evaluate_math(const std::string& expr);
};

} // namespace zenith
