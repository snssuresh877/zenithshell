#pragma once

#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>
#include <string>
#include <vector>
#include <unordered_map>

namespace zenith {

struct ShortcutDef {
    std::string id;
    std::string label;
    std::string category;
    guint default_keyval{0};
    guint default_modifiers{0};
    guint custom_keyval{0};
    guint custom_modifiers{0};

    guint current_keyval() const {
        return custom_keyval != 0 ? custom_keyval : default_keyval;
    }

    guint current_modifiers() const {
        return custom_keyval != 0 ? custom_modifiers : default_modifiers;
    }
};

class FileShortcuts {
public:
    static void init();
    static bool matches(const std::string& action_id, guint keyval, guint state);
    static std::string get_display_string(const std::string& action_id);
    static std::string get_badge_markup(const std::string& action_id);
    static const std::vector<ShortcutDef>& get_all();
    static bool update_shortcut(const std::string& action_id, guint keyval, guint modifiers);
    static void reset_to_defaults();
    static void save();
    static void load();

    // Helper: format keyval and modifier mask to human readable string (e.g. "Ctrl+Alt+T")
    static std::string format_shortcut(guint keyval, guint modifiers);
    static std::string format_badge_markup(guint keyval, guint modifiers);

private:
    static std::vector<ShortcutDef> s_shortcuts;
    static std::unordered_map<std::string, size_t> s_index_map;
    static bool s_initialized;
};

} // namespace zenith
