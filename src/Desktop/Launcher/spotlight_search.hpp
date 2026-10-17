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

struct IndexedApp {
    GAppInfo* app = nullptr;
    std::string name;
    std::string desc;
    std::string exec;
    std::string id;
    std::string generic_name;
    std::string categories;
    std::string keywords;

    std::string name_lower;
    std::string desc_lower;
    std::string exec_lower;
    std::string id_lower;
    std::string generic_name_lower;
    std::string categories_lower;
    std::string keywords_lower;

    uint16_t category_mask = 0;
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
    static std::vector<IndexedApp> indexed_apps;
    static AppCategory current_category;
    static std::vector<GtkWidget*> category_buttons;
    static guint search_debounce_id;

    static void load_apps();
    static void set_category(AppCategory cat);
    static uint16_t compute_category_mask(const IndexedApp& app);
    static bool app_matches_category(const IndexedApp& app, AppCategory cat);
    static void on_search_changed(GtkEntry* entry, gpointer user_data);
    static void perform_search();
    static void flush_search();
    static void on_row_activated(GtkListBox* listbox, GtkListBoxRow* row, gpointer user_data);
    static gboolean on_key_pressed(GtkWidget* widget, GdkEventKey* event, gpointer user_data);
    static void execute_result(const SearchResult& result);
    static std::string evaluate_math(const std::string& expr);
};

} // namespace zenith
