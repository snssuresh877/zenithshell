#include "shell/launcher/spotlight_search.hpp"
#include "gtk3_compat.hpp"
#include <gtk-layer-shell/gtk-layer-shell.h>
#include <gio/gdesktopappinfo.h>
#include <gdk/gdk.h>
#include <gdk/gdkkeysyms.h>
#include <cairo.h>
#include <algorithm>
#include <cctype>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <sstream>
#include <unordered_set>

namespace fs = std::filesystem;

namespace zenith {

GtkWidget* SpotlightSearch::window = nullptr;
GtkWidget* SpotlightSearch::search_entry = nullptr;
GtkWidget* SpotlightSearch::results_listbox = nullptr;
std::vector<SearchResult> SpotlightSearch::current_results;
std::vector<GAppInfo*> SpotlightSearch::installed_apps;
AppCategory SpotlightSearch::current_category = AppCategory::ALL;
std::vector<GtkWidget*> SpotlightSearch::category_buttons;

void SpotlightSearch::init(GtkApplication* app) {
    load_apps();

    window = gtk_application_window_new(app);
    gtk_widget_add_css_class(window, "spotlight-window");

    // Enable true RGBA transparency so rounded corners don't render grey/black box artifacts
    GdkScreen* screen = gtk_widget_get_screen(window);
    GdkVisual* visual = gdk_screen_get_rgba_visual(screen);
    if (visual) {
        gtk_widget_set_visual(window, visual);
    }
    gtk_widget_set_app_paintable(window, TRUE);

    g_signal_connect(window, "draw", G_CALLBACK(+[](GtkWidget*, cairo_t* cr, gpointer) -> gboolean {
        cairo_save(cr);
        cairo_set_operator(cr, CAIRO_OPERATOR_CLEAR);
        cairo_paint(cr);
        cairo_restore(cr);
        return FALSE;
    }), nullptr);

    // Full-screen overlay layer shell
    gtk_layer_init_for_window(GTK_WINDOW(window));
    gtk_layer_set_layer(GTK_WINDOW(window), GTK_LAYER_SHELL_LAYER_OVERLAY);
    gtk_layer_set_keyboard_mode(GTK_WINDOW(window), GTK_LAYER_SHELL_KEYBOARD_MODE_NONE);

    gtk_layer_set_anchor(GTK_WINDOW(window), GTK_LAYER_SHELL_EDGE_TOP, TRUE);
    gtk_layer_set_anchor(GTK_WINDOW(window), GTK_LAYER_SHELL_EDGE_BOTTOM, TRUE);
    gtk_layer_set_anchor(GTK_WINDOW(window), GTK_LAYER_SHELL_EDGE_LEFT, TRUE);
    gtk_layer_set_anchor(GTK_WINDOW(window), GTK_LAYER_SHELL_EDGE_RIGHT, TRUE);

    // Clickable transparent backdrop
    GtkWidget* backdrop = gtk_event_box_new();
    gtk_event_box_set_visible_window(GTK_EVENT_BOX(backdrop), FALSE);
    gtk_widget_add_css_class(backdrop, "spotlight-backdrop");

    // Outer layout to center the card at top: 90px
    GtkWidget* outer_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_halign(outer_box, GTK_ALIGN_CENTER);
    gtk_widget_set_valign(outer_box, GTK_ALIGN_START);
    gtk_widget_set_margin_top(outer_box, 90);

    // Spotlight Card (640px Width, 490px Height, 24px curvy radius)
    GtkWidget* card = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
    gtk_widget_add_css_class(card, "spotlight-card");
    gtk_widget_set_size_request(card, 640, 490);

    // 1. Search Bar Input Container (Search icon + GtkEntry)
    GtkWidget* search_container = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_widget_add_css_class(search_container, "spotlight-input-container");

    GtkWidget* search_icon = gtk_label_new("󰍉");
    gtk_widget_add_css_class(search_icon, "spotlight-search-icon");

    search_entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(search_entry), "Search applications, math, system commands...");
    gtk_widget_add_css_class(search_entry, "spotlight-input-field");
    gtk_widget_set_hexpand(search_entry, TRUE);

    gtk_box_pack_start(GTK_BOX(search_container), search_icon, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(search_container), search_entry, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(card), search_container, FALSE, FALSE, 0);

    // 2. Category Filter Pill Strip
    GtkWidget* cat_scroll = gtk_scrolled_window_new(nullptr, nullptr);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(cat_scroll), GTK_POLICY_AUTOMATIC, GTK_POLICY_NEVER);
    gtk_widget_add_css_class(cat_scroll, "spotlight-cat-scroll");

    GtkWidget* cat_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    gtk_widget_add_css_class(cat_box, "spotlight-cat-bar");

    struct CatDef {
        AppCategory cat;
        const char* icon;
        const char* label;
    };

    const std::vector<CatDef> cat_defs = {
        { AppCategory::ALL,         "󰄬", "All" },
        { AppCategory::WEB,         "󰖟", "Internet" },
        { AppCategory::MULTIMEDIA,  "󰎈", "Media" },
        { AppCategory::DEVELOPMENT, "󰅩", "Dev" },
        { AppCategory::GRAPHICS,    "󰏘", "Graphics" },
        { AppCategory::OFFICE,      "󰏫", "Office" },
        { AppCategory::SYSTEM,      "󰒋", "System" },
        { AppCategory::SETTINGS,    "󰒓", "Settings" },
        { AppCategory::FILES,       "󰉋", "Files" }
    };

    category_buttons.clear();
    for (const auto& cd : cat_defs) {
        std::string full_text = std::string(cd.icon) + " " + cd.label;
        GtkWidget* cat_btn = gtk_button_new_with_label(full_text.c_str());
        gtk_widget_add_css_class(cat_btn, "spotlight-cat-btn");
        if (cd.cat == AppCategory::ALL) {
            gtk_widget_add_css_class(cat_btn, "active");
        }

        AppCategory c = cd.cat;
        g_signal_connect(cat_btn, "clicked", G_CALLBACK(+[](GtkButton*, gpointer user_data) {
            AppCategory selected_cat = static_cast<AppCategory>(GPOINTER_TO_INT(user_data));
            SpotlightSearch::set_category(selected_cat);
        }), GINT_TO_POINTER(static_cast<int>(c)));

        category_buttons.push_back(cat_btn);
        gtk_box_pack_start(GTK_BOX(cat_box), cat_btn, FALSE, FALSE, 0);
    }

    gtk_container_add(GTK_CONTAINER(cat_scroll), cat_box);
    gtk_box_pack_start(GTK_BOX(card), cat_scroll, FALSE, FALSE, 0);

    g_signal_connect(search_entry, "changed", G_CALLBACK(on_search_changed), nullptr);
    g_signal_connect(window, "key-press-event", G_CALLBACK(on_key_pressed), nullptr);

    // Backdrop click dismisses Spotlight
    g_signal_connect(backdrop, "button-press-event", G_CALLBACK(+[](GtkWidget*, GdkEventButton*, gpointer) -> gboolean {
        SpotlightSearch::hide();
        return TRUE;
    }), nullptr);

    // 3. Results Scrollable List
    GtkWidget* scroll = gtk_scrolled_window_new(nullptr, nullptr);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_widget_set_vexpand(scroll, TRUE);

    results_listbox = gtk_list_box_new();
    gtk_widget_add_css_class(results_listbox, "spotlight-list");
    gtk_container_add(GTK_CONTAINER(scroll), results_listbox);

    g_signal_connect(results_listbox, "row-activated", G_CALLBACK(on_row_activated), nullptr);
    gtk_box_pack_start(GTK_BOX(card), scroll, TRUE, TRUE, 0);

    // 4. Footer with sleek navigation hints
    GtkWidget* footer_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 16);
    gtk_widget_add_css_class(footer_box, "spotlight-footer");

    GtkWidget* hint_nav = gtk_label_new("󰁌 ↑↓ Navigate");
    gtk_widget_add_css_class(hint_nav, "spotlight-footer-hint");

    GtkWidget* hint_launch = gtk_label_new("󰘳 ↵ Launch");
    gtk_widget_add_css_class(hint_launch, "spotlight-footer-hint");

    GtkWidget* hint_close = gtk_label_new("󱊷 Esc Close");
    gtk_widget_add_css_class(hint_close, "spotlight-footer-hint");

    gtk_box_pack_start(GTK_BOX(footer_box), hint_nav, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(footer_box), hint_launch, FALSE, FALSE, 0);
    gtk_box_pack_end(GTK_BOX(footer_box), hint_close, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(card), footer_box, FALSE, FALSE, 0);

    gtk_box_pack_start(GTK_BOX(outer_box), card, FALSE, FALSE, 0);

    // Pack into backdrop event box
    gtk_container_add(GTK_CONTAINER(backdrop), outer_box);
    gtk_container_add(GTK_CONTAINER(window), backdrop);
}

void SpotlightSearch::set_category(AppCategory cat) {
    current_category = cat;
    for (size_t i = 0; i < category_buttons.size(); ++i) {
        if (i == static_cast<size_t>(cat)) {
            gtk_widget_add_css_class(category_buttons[i], "active");
        } else {
            gtk_widget_remove_css_class(category_buttons[i], "active");
        }
    }
    if (search_entry) {
        on_search_changed(GTK_ENTRY(search_entry), nullptr);
    }
}

bool SpotlightSearch::app_matches_category(GAppInfo* app, AppCategory cat) {
    if (cat == AppCategory::ALL) return true;

    std::string categories_str = "";
    std::string id_str = g_app_info_get_id(app) ? g_app_info_get_id(app) : "";
    std::string name_str = g_app_info_get_name(app) ? g_app_info_get_name(app) : "";
    std::string exec_str = g_app_info_get_executable(app) ? g_app_info_get_executable(app) : "";

    std::transform(id_str.begin(), id_str.end(), id_str.begin(), ::tolower);
    std::transform(name_str.begin(), name_str.end(), name_str.begin(), ::tolower);
    std::transform(exec_str.begin(), exec_str.end(), exec_str.begin(), ::tolower);

    if (G_IS_DESKTOP_APP_INFO(app)) {
        GDesktopAppInfo* dinfo = G_DESKTOP_APP_INFO(app);
        const char* cats = g_desktop_app_info_get_categories(dinfo);
        if (cats) {
            categories_str = cats;
            std::transform(categories_str.begin(), categories_str.end(), categories_str.begin(), ::tolower);
        }
    }

    auto has = [&](const std::string& key) {
        return categories_str.find(key) != std::string::npos ||
               id_str.find(key) != std::string::npos ||
               name_str.find(key) != std::string::npos ||
               exec_str.find(key) != std::string::npos;
    };

    switch (cat) {
        case AppCategory::WEB:
            return has("network") || has("webbrowser") || has("browser") || has("firefox") || has("zen") || has("qutebrowser") || has("chat") || has("telegram") || has("wechat") || has("email") || has("vpn") || has("proton") || has("localsend") || has("kdeconnect");
        case AppCategory::MULTIMEDIA:
            return has("audiovideo") || has("audio") || has("video") || has("player") || has("music") || has("media") || has("mpv") || has("handbrake") || has("ghb") || has("sound") || has("volume") || has("pavucontrol");
        case AppCategory::DEVELOPMENT:
            return has("development") || has("ide") || has("texteditor") || has("editor") || has("zed") || has("nvim") || has("vim") || has("neovim") || has("code") || has("cmake") || has("debugger") || has("antigravity");
        case AppCategory::GRAPHICS:
            return has("graphics") || has("2dgraphics") || has("rastergraphics") || has("vectorgraphics") || has("photography") || has("gimp") || has("krita") || has("inkscape") || has("loupe") || has("image") || has("draw") || has("scan");
        case AppCategory::OFFICE:
            return has("office") || has("spreadsheet") || has("wordprocessor") || has("presentation") || has("viewer") || has("calc") || has("writer") || has("impress") || has("libreoffice") || has("onlyoffice") || has("obsidian") || has("planify") || has("evince") || has("document") || has("pdf");
        case AppCategory::SYSTEM:
            return has("system") || has("monitor") || has("terminalemulator") || has("terminal") || has("foot") || has("kitty") || has("btop") || has("htop") || has("nvtop") || has("timeshift") || has("yazi") || has("systemmonitor");
        case AppCategory::SETTINGS:
            return has("settings") || has("desktopsettings") || has("hardwaresettings") || has("preferences") || has("configuration") || has("nwg-look") || has("nwg-displays") || has("qt5ct") || has("qt6ct") || has("blueman") || has("kvantum") || has("cups") || has("controlpanel") || has("solaar");
        case AppCategory::FILES:
            return has("filemanager") || has("files") || has("archiving") || has("compression") || has("file-roller") || has("thunar") || has("cosmic-files") || has("yazi") || has("bulk-rename");
        default:
            return true;
    }
}

void SpotlightSearch::load_apps() {
    installed_apps.clear();
    std::unordered_set<std::string> seen_ids;

    // 1. Standard Gio AppInfo
    GList* list = g_app_info_get_all();
    for (GList* l = list; l != nullptr; l = l->next) {
        GAppInfo* info = G_APP_INFO(l->data);
        if (g_app_info_should_show(info)) {
            const char* id = g_app_info_get_id(info);
            if (id && seen_ids.find(id) == seen_ids.end()) {
                seen_ids.insert(id);
                installed_apps.push_back(info);
            }
        }
    }

    // 2. Extra Desktop Dirs (Flatpak, Local)
    const std::vector<std::string> app_dirs = {
        "/usr/share/applications",
        "/usr/local/share/applications",
        std::string(g_get_user_data_dir()) + "/applications",
        "/var/lib/flatpak/exports/share/applications",
        std::string(g_get_home_dir()) + "/.local/share/flatpak/exports/share/applications",
        "/var/lib/snapd/desktop/applications"
    };

    for (const auto& dir : app_dirs) {
        if (!fs::exists(dir)) continue;
        try {
            for (const auto& entry : fs::directory_iterator(dir)) {
                if (entry.is_regular_file() && entry.path().extension() == ".desktop") {
                    std::string path_str = entry.path().string();
                    std::string filename = entry.path().filename().string();
                    if (seen_ids.find(filename) == seen_ids.end()) {
                        GDesktopAppInfo* dinfo = g_desktop_app_info_new_from_filename(path_str.c_str());
                        if (dinfo) {
                            GAppInfo* app_info = G_APP_INFO(dinfo);
                            if (g_app_info_should_show(app_info)) {
                                seen_ids.insert(filename);
                                installed_apps.push_back(app_info);
                            } else {
                                g_object_unref(dinfo);
                            }
                        }
                    }
                }
            }
        } catch (...) {}
    }
}

void SpotlightSearch::toggle() {
    if (!window) return;
    if (gtk_widget_get_visible(window)) {
        hide();
    } else {
        show();
    }
}

void SpotlightSearch::show() {
    if (!window) return;
    load_apps();
    set_category(AppCategory::ALL);
    gtk_layer_set_keyboard_mode(GTK_WINDOW(window), GTK_LAYER_SHELL_KEYBOARD_MODE_EXCLUSIVE);
    gtk_entry_set_text(GTK_ENTRY(search_entry), "");
    on_search_changed(GTK_ENTRY(search_entry), nullptr);
    gtk_widget_show_all(window);
    gtk_widget_grab_focus(search_entry);
}

void SpotlightSearch::hide() {
    if (!window) return;
    gtk_layer_set_keyboard_mode(GTK_WINDOW(window), GTK_LAYER_SHELL_KEYBOARD_MODE_NONE);
    gtk_widget_hide(window);
}

void SpotlightSearch::on_search_changed(GtkEntry*, gpointer) {
    const char* text_ptr = gtk_entry_get_text(GTK_ENTRY(search_entry));
    std::string query = text_ptr ? text_ptr : "";
    std::transform(query.begin(), query.end(), query.begin(), ::tolower);

    // Clear listbox
    GList* children = gtk_container_get_children(GTK_CONTAINER(results_listbox));
    for (GList* l = children; l != nullptr; l = l->next) {
        gtk_widget_destroy(GTK_WIDGET(l->data));
    }
    g_list_free(children);
    current_results.clear();

    if (query.empty()) {
        int count = 0;
        for (GAppInfo* app : installed_apps) {
            if (!app_matches_category(app, current_category)) continue;
            if (count++ >= 10) break;
            const char* name = g_app_info_get_name(app);
            const char* desc = g_app_info_get_description(app);

            SearchResult res;
            res.title = name ? name : "Application";
            res.subtitle = desc ? desc : "Application";
            res.icon = g_app_info_get_icon(app);
            res.type = SearchResult::APP;
            res.app_info = app;
            current_results.push_back(res);
        }
    } else {
        // 1. Math Calculation (Only in ALL)
        if (current_category == AppCategory::ALL) {
            std::string math_res = evaluate_math(query);
            if (!math_res.empty()) {
                SearchResult res;
                res.title = math_res;
                res.subtitle = "Math calculation • Press Enter to copy to clipboard";
                res.icon_name = "accessories-calculator";
                res.type = SearchResult::MATH;
                current_results.push_back(res);
            }
        }

        // 2. System Commands (In ALL or SYSTEM)
        if (current_category == AppCategory::ALL || current_category == AppCategory::SYSTEM) {
            if (query == "lock" || query == "reboot" || query == "restart" || query == "shutdown" || query == "poweroff" || query == "logout" || query == "exit" || query == "suspend" || query == "sleep") {
                SearchResult res;
                res.title = "System: " + query;
                res.type = SearchResult::SYSTEM;
                if (query == "lock") {
                    res.subtitle = "Lock current session with hyprlock";
                    res.icon_name = "system-lock-screen";
                    res.action_cmd = "hyprlock || swaylock";
                } else if (query == "reboot" || query == "restart") {
                    res.subtitle = "Restart system computer";
                    res.icon_name = "system-reboot";
                    res.action_cmd = "systemctl reboot";
                } else if (query == "suspend" || query == "sleep") {
                    res.subtitle = "Suspend computer to RAM";
                    res.icon_name = "system-suspend";
                    res.action_cmd = "systemctl suspend";
                } else if (query == "logout" || query == "exit") {
                    res.subtitle = "Exit current Hyprland graphical session";
                    res.icon_name = "system-log-out";
                    res.action_cmd = "hyprctl dispatch exit";
                } else {
                    res.subtitle = "Power off system computer";
                    res.icon_name = "system-shutdown";
                    res.action_cmd = "systemctl poweroff";
                }
                current_results.push_back(res);
            }
        }

        // 3. Applications Matching with Multi-Field Scoring
        struct ScoredApp {
            SearchResult item;
            int score = 0;
        };
        std::vector<ScoredApp> scored_list;

        for (GAppInfo* app : installed_apps) {
            if (!app_matches_category(app, current_category)) continue;

            const char* name_cstr = g_app_info_get_name(app);
            const char* desc_cstr = g_app_info_get_description(app);
            const char* exec_cstr = g_app_info_get_executable(app);
            const char* id_cstr = g_app_info_get_id(app);

            std::string name = name_cstr ? name_cstr : "";
            std::string desc = desc_cstr ? desc_cstr : "";
            std::string exec = exec_cstr ? exec_cstr : "";
            std::string id = id_cstr ? id_cstr : "";

            std::string name_lower = name;
            std::string desc_lower = desc;
            std::string exec_lower = exec;
            std::string id_lower = id;

            std::transform(name_lower.begin(), name_lower.end(), name_lower.begin(), ::tolower);
            std::transform(desc_lower.begin(), desc_lower.end(), desc_lower.begin(), ::tolower);
            std::transform(exec_lower.begin(), exec_lower.end(), exec_lower.begin(), ::tolower);
            std::transform(id_lower.begin(), id_lower.end(), id_lower.begin(), ::tolower);

            // Also check keywords & generic name & categories if desktop app
            std::string keywords_str = "";
            std::string generic_name_lower = "";
            std::string categories_lower = "";
            if (G_IS_DESKTOP_APP_INFO(app)) {
                GDesktopAppInfo* dinfo = G_DESKTOP_APP_INFO(app);
                const char* gen = g_desktop_app_info_get_generic_name(dinfo);
                if (gen) {
                    generic_name_lower = gen;
                    std::transform(generic_name_lower.begin(), generic_name_lower.end(), generic_name_lower.begin(), ::tolower);
                }
                const char* cats = g_desktop_app_info_get_categories(dinfo);
                if (cats) {
                    categories_lower = cats;
                    std::transform(categories_lower.begin(), categories_lower.end(), categories_lower.begin(), ::tolower);
                }
                const char* const* kw = g_desktop_app_info_get_keywords(dinfo);
                if (kw) {
                    for (int i = 0; kw[i] != nullptr; ++i) {
                        keywords_str += kw[i];
                        keywords_str += " ";
                    }
                    std::transform(keywords_str.begin(), keywords_str.end(), keywords_str.begin(), ::tolower);
                }
            }

            int score = 0;

            // Executable exact/prefix match (e.g. "gimp" -> "gimp-3.2", "zen" -> "zen-bin")
            if (exec_lower == query) score += 130;
            else if (exec_lower.rfind(query, 0) == 0) score += 110;
            else if (exec_lower.find(query) != std::string::npos) score += 85;

            // Name exact/prefix match (e.g. "Firefox", "Zed", "Foot")
            if (name_lower == query) score += 120;
            else if (name_lower.rfind(query, 0) == 0) score += 100;
            else if (name_lower.find(query) != std::string::npos) score += 75;

            // Desktop ID match (e.g. "gimp.desktop", "zen.desktop")
            if (id_lower.rfind(query, 0) == 0) score += 90;
            else if (id_lower.find(query) != std::string::npos) score += 70;

            // Keywords match (e.g. "GIMP", "photo", "drawing", "pdf")
            if (keywords_str.find(query) != std::string::npos) score += 65;

            // Generic name match (e.g. "Image Editor", "Web Browser")
            if (generic_name_lower.find(query) != std::string::npos) score += 55;

            // Categories match (e.g. "Graphics", "Office", "AudioVideo")
            if (categories_lower.find(query) != std::string::npos) score += 40;

            // Description match
            if (desc_lower.find(query) != std::string::npos) score += 30;

            // Penalize keybind helper desktop entries slightly unless explicitly typed
            if (name_lower.rfind("super +", 0) == 0 && query.rfind("super", 0) != 0) {
                score -= 30;
            }

            if (score > 0) {
                SearchResult res;
                res.title = name;
                res.subtitle = desc.empty() ? (generic_name_lower.empty() ? "Application" : generic_name_lower) : desc;
                res.icon = g_app_info_get_icon(app);
                res.type = SearchResult::APP;
                res.app_info = app;
                scored_list.push_back({res, score});
            }
        }

        // Sort by score descending
        std::sort(scored_list.begin(), scored_list.end(), [](const ScoredApp& a, const ScoredApp& b) {
            return a.score > b.score;
        });

        for (size_t i = 0; i < scored_list.size() && current_results.size() < 12; ++i) {
            current_results.push_back(scored_list[i].item);
        }

        // 4. Fallback: Run in terminal / Web Search
        if (current_results.empty() && current_category == AppCategory::ALL) {
            SearchResult term_res;
            term_res.title = "Run '" + query + "' in terminal";
            term_res.subtitle = "Execute command in Foot terminal";
            term_res.icon_name = "utilities-terminal";
            term_res.type = SearchResult::SYSTEM;
            term_res.action_cmd = "foot -e " + query;
            current_results.push_back(term_res);

            SearchResult web_res;
            web_res.title = "Search Google for \"" + query + "\"";
            web_res.subtitle = "Open search query in default web browser";
            web_res.icon_name = "web-browser";
            web_res.type = SearchResult::SYSTEM;
            web_res.action_cmd = "xdg-open 'https://www.google.com/search?q=" + query + "'";
            current_results.push_back(web_res);
        }
    }

    for (const auto& item : current_results) {
        GtkWidget* row_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
        gtk_widget_add_css_class(row_box, "spotlight-item");
        gtk_widget_set_size_request(row_box, -1, 48);

        // Icon Container (32x32px centered)
        GtkWidget* icon_container = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
        gtk_widget_add_css_class(icon_container, "spotlight-icon-box");
        gtk_widget_set_size_request(icon_container, 34, 34);

        GtkWidget* icon_w = nullptr;
        if (item.icon) {
            icon_w = gtk_image_new_from_gicon(item.icon, GTK_ICON_SIZE_DND);
            gtk_image_set_pixel_size(GTK_IMAGE(icon_w), 28);
        } else {
            icon_w = gtk_image_new_from_icon_name(item.icon_name.c_str(), GTK_ICON_SIZE_DND);
            gtk_image_set_pixel_size(GTK_IMAGE(icon_w), 28);
        }
        gtk_box_pack_start(GTK_BOX(icon_container), icon_w, TRUE, TRUE, 0);
        gtk_box_pack_start(GTK_BOX(row_box), icon_container, FALSE, FALSE, 0);

        // Text Box (Title + Subtitle)
        GtkWidget* txt_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
        GtkWidget* title = gtk_label_new(item.title.c_str());
        gtk_widget_add_css_class(title, "spotlight-item-title");
        gtk_widget_set_halign(title, GTK_ALIGN_START);

        GtkWidget* sub = gtk_label_new(item.subtitle.c_str());
        gtk_widget_add_css_class(sub, "spotlight-item-sub");
        gtk_widget_set_halign(sub, GTK_ALIGN_START);
        gtk_label_set_ellipsize(GTK_LABEL(sub), PANGO_ELLIPSIZE_END);

        gtk_box_pack_start(GTK_BOX(txt_box), title, FALSE, FALSE, 0);
        gtk_box_pack_start(GTK_BOX(txt_box), sub, FALSE, FALSE, 0);
        gtk_box_pack_start(GTK_BOX(row_box), txt_box, TRUE, TRUE, 0);

        // Action Tag on Right
        const char* badge_text = (item.type == SearchResult::MATH) ? "Math" : ((item.type == SearchResult::SYSTEM) ? "Command" : "App");
        GtkWidget* badge = gtk_label_new(badge_text);
        gtk_widget_add_css_class(badge, "spotlight-item-badge");
        gtk_box_pack_end(GTK_BOX(row_box), badge, FALSE, FALSE, 4);

        gtk_container_add(GTK_CONTAINER(results_listbox), row_box);
    }
    gtk_widget_show_all(results_listbox);

    if (!current_results.empty()) {
        GtkListBoxRow* first_row = gtk_list_box_get_row_at_index(GTK_LIST_BOX(results_listbox), 0);
        if (first_row) gtk_list_box_select_row(GTK_LIST_BOX(results_listbox), first_row);
    }
}

void SpotlightSearch::on_row_activated(GtkListBox*, GtkListBoxRow* row, gpointer) {
    int index = gtk_list_box_row_get_index(row);
    if (index >= 0 && static_cast<size_t>(index) < current_results.size()) {
        execute_result(current_results[index]);
    }
}

gboolean SpotlightSearch::on_key_pressed(GtkWidget*, GdkEventKey* event, gpointer) {
    if (event->keyval == GDK_KEY_Escape) {
        hide();
        return TRUE;
    }
    if (event->keyval == GDK_KEY_Return || event->keyval == GDK_KEY_KP_Enter) {
        GtkListBoxRow* selected = gtk_list_box_get_selected_row(GTK_LIST_BOX(results_listbox));
        if (!selected) selected = gtk_list_box_get_row_at_index(GTK_LIST_BOX(results_listbox), 0);
        if (selected) {
            on_row_activated(GTK_LIST_BOX(results_listbox), selected, nullptr);
            return TRUE;
        }
    }
    if (event->keyval == GDK_KEY_Down) {
        GtkListBoxRow* selected = gtk_list_box_get_selected_row(GTK_LIST_BOX(results_listbox));
        int cur_idx = selected ? gtk_list_box_row_get_index(selected) : -1;
        GtkListBoxRow* next_row = gtk_list_box_get_row_at_index(GTK_LIST_BOX(results_listbox), cur_idx + 1);
        if (next_row) {
            gtk_list_box_select_row(GTK_LIST_BOX(results_listbox), next_row);
            gtk_widget_grab_focus(GTK_WIDGET(next_row));
            gtk_widget_grab_focus(search_entry);
        }
        return TRUE;
    }
    if (event->keyval == GDK_KEY_Up) {
        GtkListBoxRow* selected = gtk_list_box_get_selected_row(GTK_LIST_BOX(results_listbox));
        int cur_idx = selected ? gtk_list_box_row_get_index(selected) : 1;
        if (cur_idx > 0) {
            GtkListBoxRow* prev_row = gtk_list_box_get_row_at_index(GTK_LIST_BOX(results_listbox), cur_idx - 1);
            if (prev_row) {
                gtk_list_box_select_row(GTK_LIST_BOX(results_listbox), prev_row);
                gtk_widget_grab_focus(GTK_WIDGET(prev_row));
                gtk_widget_grab_focus(search_entry);
            }
        }
        return TRUE;
    }
    return FALSE;
}

void SpotlightSearch::execute_result(const SearchResult& result) {
    hide();
    if (result.type == SearchResult::APP) {
        if (result.app_info) {
            GError* err = nullptr;
            g_app_info_launch(result.app_info, nullptr, nullptr, &err);
            if (err) {
                std::cerr << "[SpotlightSearch] App launch error: " << err->message << "\n";
                g_error_free(err);
            }
        }
    } else if (result.type == SearchResult::SYSTEM) {
        if (!result.action_cmd.empty()) {
            std::string cmd = result.action_cmd + " &";
            system(cmd.c_str());
        }
    } else if (result.type == SearchResult::MATH) {
        std::string cmd = "wl-copy '" + result.title + "' 2>/dev/null";
        system(cmd.c_str());
    }
}

std::string SpotlightSearch::evaluate_math(const std::string& expr) {
    if (expr.empty()) return "";
    bool has_math = false;
    for (char c : expr) {
        if (c == '+' || c == '-' || c == '*' || c == '/' || c == '^' || c == '%') {
            has_math = true;
            break;
        }
    }
    if (!has_math) return "";

    double a = 0, b = 0;
    char op = 0;
    if (sscanf(expr.c_str(), "%lf %c %lf", &a, &op, &b) == 3 || sscanf(expr.c_str(), "%lf%c%lf", &a, &op, &b) == 3) {
        double res = 0;
        if (op == '+') res = a + b;
        else if (op == '-') res = a - b;
        else if (op == '*') res = a * b;
        else if (op == '/') res = (b != 0) ? a / b : 0;
        else if (op == '^') res = pow(a, b);
        else if (op == '%') res = (a * b) / 100.0;
        
        char buf[64];
        snprintf(buf, sizeof(buf), "= %.4g", res);
        return std::string(buf);
    }
    return "";
}

} // namespace zenith
