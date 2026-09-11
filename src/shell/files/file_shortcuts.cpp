#include "shell/files/file_shortcuts.hpp"
#include <nlohmann/json.hpp>
#include <filesystem>
#include <fstream>
#include <iostream>

namespace fs = std::filesystem;
using json = nlohmann::json;

namespace zenith {

std::vector<ShortcutDef> FileShortcuts::s_shortcuts;
std::unordered_map<std::string, size_t> FileShortcuts::s_index_map;
bool FileShortcuts::s_initialized = false;

static std::string get_config_path() {
    const char* cfg = g_get_user_config_dir();
    std::string dir = std::string(cfg ? cfg : "~/.config") + "/zenithshell";
    fs::create_directories(dir);
    return dir + "/files_shortcuts.json";
}

void FileShortcuts::init() {
    if (s_initialized) return;
    s_initialized = true;

    s_shortcuts.clear();
    s_index_map.clear();

    auto add = [](const std::string& id, const std::string& label, const std::string& category,
                  guint keyval, guint mods) {
        ShortcutDef def;
        def.id = id;
        def.label = label;
        def.category = category;
        def.default_keyval = keyval;
        def.default_modifiers = mods;
        def.custom_keyval = 0;
        def.custom_modifiers = 0;

        size_t idx = s_shortcuts.size();
        s_shortcuts.push_back(def);
        s_index_map[id] = idx;
    };

    // ── 1. Navigation & Tabs ──────────────────────────────────────────────────
    add("nav_back", "Go Back in History", "Navigation", GDK_KEY_Left, GDK_MOD1_MASK);
    add("nav_forward", "Go Forward in History", "Navigation", GDK_KEY_Right, GDK_MOD1_MASK);
    add("nav_up", "Go to Parent Directory", "Navigation", GDK_KEY_Up, GDK_MOD1_MASK);
    add("nav_home", "Go to Home Folder", "Navigation", GDK_KEY_Home, GDK_MOD1_MASK);
    add("edit_path", "Direct Path Entry", "Navigation", GDK_KEY_l, GDK_CONTROL_MASK);
    add("new_tab", "Open New Tab", "Navigation", GDK_KEY_t, GDK_CONTROL_MASK);
    add("close_tab", "Close Current Tab", "Navigation", GDK_KEY_w, GDK_CONTROL_MASK);
    add("split_view", "Split View (Dual Pane)", "Navigation", GDK_KEY_F3, 0);

    // ── 2. File Operations ────────────────────────────────────────────────────
    add("open_item", "Open Selected File / Folder", "File Operations", GDK_KEY_Return, 0);
    add("rename_item", "Rename / Bulk Rename", "File Operations", GDK_KEY_F2, 0);
    add("copy_items", "Copy to Clipboard", "File Operations", GDK_KEY_c, GDK_CONTROL_MASK);
    add("cut_items", "Cut to Clipboard", "File Operations", GDK_KEY_x, GDK_CONTROL_MASK);
    add("paste_items", "Paste from Clipboard", "File Operations", GDK_KEY_v, GDK_CONTROL_MASK);
    add("copy_path", "Copy Full Path", "File Operations", GDK_KEY_c, GDK_CONTROL_MASK | GDK_SHIFT_MASK);
    add("new_folder", "Create New Folder", "File Operations", GDK_KEY_n, GDK_CONTROL_MASK | GDK_SHIFT_MASK);
    add("trash_items", "Move to Trash", "File Operations", GDK_KEY_Delete, 0);
    add("delete_permanent", "Permanently Delete", "File Operations", GDK_KEY_Delete, GDK_SHIFT_MASK);
    add("select_all", "Select All Items", "File Operations", GDK_KEY_a, GDK_CONTROL_MASK);
    add("refresh_view", "Refresh Current Directory", "File Operations", GDK_KEY_F5, 0);

    // ── 3. View, Tools & Panels ───────────────────────────────────────────────
    add("command_palette", "Command Palette", "View & Tools", GDK_KEY_k, GDK_CONTROL_MASK);
    add("inspector", "Toggle Inspector Panel", "View & Tools", GDK_KEY_i, GDK_CONTROL_MASK);
    add("quick_preview", "Quick Preview (QuickLook)", "View & Tools", GDK_KEY_space, 0);
    add("search_files", "Search in Current Folder", "View & Tools", GDK_KEY_f, GDK_CONTROL_MASK);
    add("open_terminal", "Open Terminal Here", "View & Tools", GDK_KEY_F4, 0);
    add("toggle_hidden", "Toggle Hidden Files", "View & Tools", GDK_KEY_h, GDK_CONTROL_MASK);
    add("view_grid", "Switch to Grid View", "View & Tools", GDK_KEY_1, GDK_CONTROL_MASK);
    add("view_list", "Switch to List View", "View & Tools", GDK_KEY_2, GDK_CONTROL_MASK);
    add("zoom_in", "Zoom In (Increase Size)", "View & Tools", GDK_KEY_equal, GDK_CONTROL_MASK);
    add("zoom_out", "Zoom Out (Decrease Size)", "View & Tools", GDK_KEY_minus, GDK_CONTROL_MASK);
    add("zoom_reset", "Reset Zoom to Standard", "View & Tools", GDK_KEY_0, GDK_CONTROL_MASK);
    add("preferences", "Open Preferences", "View & Tools", GDK_KEY_comma, GDK_CONTROL_MASK);

    load();
}

bool FileShortcuts::matches(const std::string& action_id, guint keyval, guint state) {
    if (!s_initialized) init();

    auto it = s_index_map.find(action_id);
    if (it == s_index_map.end()) return false;

    const auto& def = s_shortcuts[it->second];
    guint target_key = def.current_keyval();
    guint target_mods = def.current_modifiers();

    guint clean_state = state & gtk_accelerator_get_default_mod_mask();

    // Key equality (case insensitive for letters)
    guint lower_input = gdk_keyval_to_lower(keyval);
    guint lower_target = gdk_keyval_to_lower(target_key);

    // Handle special zoom in equal/plus keysym aliases
    if (action_id == "zoom_in") {
        if ((clean_state == target_mods) &&
            (keyval == GDK_KEY_equal || keyval == GDK_KEY_plus || keyval == GDK_KEY_KP_Add)) {
            return true;
        }
    }

    if (action_id == "zoom_out") {
        if ((clean_state == target_mods) &&
            (keyval == GDK_KEY_minus || keyval == GDK_KEY_underscore || keyval == GDK_KEY_KP_Subtract)) {
            return true;
        }
    }

    if (action_id == "zoom_reset") {
        if ((clean_state == target_mods) && (keyval == GDK_KEY_0 || keyval == GDK_KEY_KP_0)) {
            return true;
        }
    }

    return (lower_input == lower_target) && (clean_state == target_mods);
}

std::string FileShortcuts::format_shortcut(guint keyval, guint modifiers) {
    if (keyval == 0) return "None";

    std::string result;
    if (modifiers & GDK_CONTROL_MASK) result += "Ctrl+";
    if (modifiers & GDK_MOD1_MASK) result += "Alt+";
    if (modifiers & GDK_SHIFT_MASK) result += "Shift+";
    if (modifiers & GDK_SUPER_MASK) result += "Super+";

    const char* key_name = gdk_keyval_name(keyval);
    if (!key_name) return result + "Unknown";

    std::string kn = key_name;
    if (kn == "Return" || kn == "KP_Enter") kn = "Enter";
    else if (kn == "space") kn = "Space";
    else if (kn == "equal") kn = "+";
    else if (kn == "minus") kn = "-";
    else if (kn == "comma") kn = ",";
    else if (kn == "period") kn = ".";
    else if (kn.length() == 1 && kn[0] >= 'a' && kn[0] <= 'z') {
        kn[0] = static_cast<char>(toupper(kn[0]));
    }

    result += kn;
    return result;
}

std::string FileShortcuts::format_badge_markup(guint keyval, guint modifiers) {
    if (keyval == 0) return "<tt>None</tt>";

    std::string result;
    if (modifiers & GDK_CONTROL_MASK) result += "<tt>Ctrl</tt> + ";
    if (modifiers & GDK_MOD1_MASK) result += "<tt>Alt</tt> + ";
    if (modifiers & GDK_SHIFT_MASK) result += "<tt>Shift</tt> + ";
    if (modifiers & GDK_SUPER_MASK) result += "<tt>Super</tt> + ";

    const char* key_name = gdk_keyval_name(keyval);
    std::string kn = key_name ? key_name : "?";
    if (kn == "Return" || kn == "KP_Enter") kn = "Enter";
    else if (kn == "space") kn = "Space";
    else if (kn == "equal") kn = "+";
    else if (kn == "minus") kn = "-";
    else if (kn == "comma") kn = ",";
    else if (kn == "period") kn = ".";
    else if (kn.length() == 1 && kn[0] >= 'a' && kn[0] <= 'z') {
        kn[0] = static_cast<char>(toupper(kn[0]));
    }

    result += "<tt>" + kn + "</tt>";
    return result;
}

std::string FileShortcuts::get_display_string(const std::string& action_id) {
    if (!s_initialized) init();
    auto it = s_index_map.find(action_id);
    if (it == s_index_map.end()) return "";
    const auto& def = s_shortcuts[it->second];
    return format_shortcut(def.current_keyval(), def.current_modifiers());
}

std::string FileShortcuts::get_badge_markup(const std::string& action_id) {
    if (!s_initialized) init();
    auto it = s_index_map.find(action_id);
    if (it == s_index_map.end()) return "";
    const auto& def = s_shortcuts[it->second];
    return format_badge_markup(def.current_keyval(), def.current_modifiers());
}

const std::vector<ShortcutDef>& FileShortcuts::get_all() {
    if (!s_initialized) init();
    return s_shortcuts;
}

bool FileShortcuts::update_shortcut(const std::string& action_id, guint keyval, guint modifiers) {
    if (!s_initialized) init();
    auto it = s_index_map.find(action_id);
    if (it == s_index_map.end()) return false;

    s_shortcuts[it->second].custom_keyval = keyval;
    s_shortcuts[it->second].custom_modifiers = modifiers;
    save();
    return true;
}

void FileShortcuts::reset_to_defaults() {
    if (!s_initialized) init();
    for (auto& s : s_shortcuts) {
        s.custom_keyval = 0;
        s.custom_modifiers = 0;
    }
    save();
}

void FileShortcuts::save() {
    std::string path = get_config_path();
    json j = json::object();

    for (const auto& s : s_shortcuts) {
        if (s.custom_keyval != 0) {
            j[s.id] = {
                {"keyval", s.custom_keyval},
                {"modifiers", s.custom_modifiers}
            };
        }
    }

    std::ofstream out(path);
    if (out.is_open()) {
        out << j.dump(4);
    }
}

void FileShortcuts::load() {
    std::string path = get_config_path();
    if (!fs::exists(path)) return;

    try {
        std::ifstream in(path);
        if (!in.is_open()) return;
        json j;
        in >> j;

        for (auto it = j.begin(); it != j.end(); ++it) {
            std::string id = it.key();
            auto map_it = s_index_map.find(id);
            if (map_it != s_index_map.end() && it.value().is_object()) {
                guint kv = it.value().value("keyval", 0u);
                guint mods = it.value().value("modifiers", 0u);
                s_shortcuts[map_it->second].custom_keyval = kv;
                s_shortcuts[map_it->second].custom_modifiers = mods;
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "[FileShortcuts] Failed to load custom shortcuts: " << e.what() << "\n";
    }
}

} // namespace zenith
