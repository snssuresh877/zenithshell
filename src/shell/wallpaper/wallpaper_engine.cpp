#include "shell/wallpaper/wallpaper_engine.hpp"
#include "gtk3_compat.hpp"
#include <gtk-layer-shell/gtk-layer-shell.h>
#include <gdk/gdk.h>
#include <cairo.h>
#include <vector>
#include <algorithm>
#include <cmath>
#include <iostream>
#include <filesystem>

namespace fs = std::filesystem;

namespace zenith {

namespace {

struct MonitorView {
    GdkMonitor* monitor = nullptr;
    GtkWidget* window = nullptr;
    GtkWidget* drawing_area = nullptr;
};

GtkApplication* s_app = nullptr;
std::vector<MonitorView> s_views;
std::string s_current_path;
GdkPixbuf* s_current_pixbuf = nullptr;
GdkPixbuf* s_previous_pixbuf = nullptr;

double s_transition_progress = 1.0;
gint64 s_transition_start_time = 0;
int s_transition_duration_ms = 350;
guint s_tick_id = 0;

void draw_scaled_pixbuf(cairo_t* cr, GdkPixbuf* pixbuf, int widget_w, int widget_h, double alpha) {
    if (!pixbuf || widget_w <= 0 || widget_h <= 0) return;

    int img_w = gdk_pixbuf_get_width(pixbuf);
    int img_h = gdk_pixbuf_get_height(pixbuf);
    if (img_w <= 0 || img_h <= 0) return;

    // Fill / Cover mode
    double scale_x = static_cast<double>(widget_w) / img_w;
    double scale_y = static_cast<double>(widget_h) / img_h;
    double scale = std::max(scale_x, scale_y);

    double rendered_w = img_w * scale;
    double rendered_h = img_h * scale;
    double offset_x = (widget_w - rendered_w) / 2.0;
    double offset_y = (widget_h - rendered_h) / 2.0;

    cairo_save(cr);
    cairo_translate(cr, offset_x, offset_y);
    cairo_scale(cr, scale, scale);
    gdk_cairo_set_source_pixbuf(cr, pixbuf, 0, 0);
    cairo_paint_with_alpha(cr, std::clamp(alpha, 0.0, 1.0));
    cairo_restore(cr);
}

gboolean on_drawing_area_draw(GtkWidget* widget, cairo_t* cr, gpointer) {
    int w = gtk_widget_get_allocated_width(widget);
    int h = gtk_widget_get_allocated_height(widget);

    // Deep background fallback
    cairo_set_source_rgb(cr, 0.05, 0.05, 0.06);
    cairo_paint(cr);

    if (s_transition_progress < 1.0 && s_previous_pixbuf) {
        draw_scaled_pixbuf(cr, s_previous_pixbuf, w, h, 1.0 - s_transition_progress);
    }

    if (s_current_pixbuf) {
        double current_alpha = (s_transition_progress < 1.0) ? s_transition_progress : 1.0;
        draw_scaled_pixbuf(cr, s_current_pixbuf, w, h, current_alpha);
    }

    return FALSE;
}

void queue_draw_all() {
    for (const auto& v : s_views) {
        if (v.drawing_area && GTK_IS_WIDGET(v.drawing_area)) {
            gtk_widget_queue_draw(v.drawing_area);
        }
    }
}

gboolean on_tick(GtkWidget*, GdkFrameClock*, gpointer) {
    gint64 now = g_get_monotonic_time();
    double elapsed_ms = (now - s_transition_start_time) / 1000.0;
    s_transition_progress = elapsed_ms / s_transition_duration_ms;

    if (s_transition_progress >= 1.0) {
        s_transition_progress = 1.0;
        if (s_previous_pixbuf) {
            g_object_unref(s_previous_pixbuf);
            s_previous_pixbuf = nullptr;
        }
        s_tick_id = 0;
        queue_draw_all();
        return G_SOURCE_REMOVE;
    }

    queue_draw_all();
    return G_SOURCE_CONTINUE;
}

void setup_input_passthrough(GtkWidget* window) {
    g_signal_connect(window, "realize", G_CALLBACK(+[](GtkWidget* widget, gpointer) {
        GdkWindow* gdk_win = gtk_widget_get_window(widget);
        if (gdk_win) {
            cairo_region_t* empty = cairo_region_create();
            gdk_window_input_shape_combine_region(gdk_win, empty, 0, 0);
            cairo_region_destroy(empty);
        }
    }), nullptr);
}

MonitorView create_monitor_view(GtkApplication* app, GdkMonitor* monitor) {
    MonitorView view;
    view.monitor = monitor;
    view.window = gtk_application_window_new(app);

    gtk_widget_add_css_class(view.window, "zenith-wallpaper");

    // Wayland Layer Shell configuration for full desktop wallpaper
    gtk_layer_init_for_window(GTK_WINDOW(view.window));
    gtk_layer_set_layer(GTK_WINDOW(view.window), GTK_LAYER_SHELL_LAYER_BACKGROUND);
    gtk_layer_set_monitor(GTK_WINDOW(view.window), monitor);
    gtk_layer_set_exclusive_zone(GTK_WINDOW(view.window), -1);
    gtk_layer_set_keyboard_mode(GTK_WINDOW(view.window), GTK_LAYER_SHELL_KEYBOARD_MODE_NONE);

    // Anchor to all 4 edges to span full monitor resolution
    gtk_layer_set_anchor(GTK_WINDOW(view.window), GTK_LAYER_SHELL_EDGE_TOP, TRUE);
    gtk_layer_set_anchor(GTK_WINDOW(view.window), GTK_LAYER_SHELL_EDGE_BOTTOM, TRUE);
    gtk_layer_set_anchor(GTK_WINDOW(view.window), GTK_LAYER_SHELL_EDGE_LEFT, TRUE);
    gtk_layer_set_anchor(GTK_WINDOW(view.window), GTK_LAYER_SHELL_EDGE_RIGHT, TRUE);

    // Make window completely click-through to Hyprland
    setup_input_passthrough(view.window);

    // Canvas drawing area
    view.drawing_area = gtk_drawing_area_new();
    gtk_widget_set_hexpand(view.drawing_area, TRUE);
    gtk_widget_set_vexpand(view.drawing_area, TRUE);
    g_signal_connect(view.drawing_area, "draw", G_CALLBACK(on_drawing_area_draw), nullptr);

    gtk_container_add(GTK_CONTAINER(view.window), view.drawing_area);
    gtk_widget_show_all(view.window);

    return view;
}

} // namespace

void WallpaperEngine::terminate_external_daemons() {
    // Gracefully terminate external rust daemons so ZenithShell has clean background ownership
    g_spawn_command_line_async("pkill -x awww-daemon", nullptr);
    g_spawn_command_line_async("pkill -x swww-daemon", nullptr);
    g_spawn_command_line_async("pkill -x hyprpaper", nullptr);
}

void WallpaperEngine::init(GtkApplication* app) {
    if (!app) return;
    s_app = app;

    terminate_external_daemons();

    reload_monitors();

    // Listen for monitor hotplug events
    GdkDisplay* display = gdk_display_get_default();
    if (display) {
        g_signal_connect(display, "monitor-added", G_CALLBACK(+[](GdkDisplay*, GdkMonitor*, gpointer) {
            std::cout << "[WallpaperEngine] Monitor connected. Reloading wallpaper views." << std::endl;
            WallpaperEngine::reload_monitors();
        }), nullptr);

        g_signal_connect(display, "monitor-removed", G_CALLBACK(+[](GdkDisplay*, GdkMonitor*, gpointer) {
            std::cout << "[WallpaperEngine] Monitor disconnected. Reloading wallpaper views." << std::endl;
            WallpaperEngine::reload_monitors();
        }), nullptr);
    }

    std::cout << "[WallpaperEngine] Initialized Wayland layer-shell wallpaper engine across "
              << s_views.size() << " display(s)" << std::endl;
}

void WallpaperEngine::reload_monitors() {
    if (!s_app) return;

    // Destroy existing views
    for (auto& v : s_views) {
        if (v.window && GTK_IS_WIDGET(v.window)) {
            gtk_widget_destroy(v.window);
        }
    }
    s_views.clear();

    GdkDisplay* display = gdk_display_get_default();
    if (!display) return;

    int n_monitors = gdk_display_get_n_monitors(display);
    for (int i = 0; i < n_monitors; ++i) {
        GdkMonitor* monitor = gdk_display_get_monitor(display, i);
        if (monitor) {
            s_views.push_back(create_monitor_view(s_app, monitor));
        }
    }

    queue_draw_all();
}

void WallpaperEngine::set_wallpaper(const std::string& path) {
    if (path.empty() || !fs::exists(path)) {
        std::cerr << "[WallpaperEngine] Cannot load nonexistent wallpaper: " << path << std::endl;
        return;
    }

    GError* error = nullptr;
    GdkPixbuf* new_pixbuf = gdk_pixbuf_new_from_file(path.c_str(), &error);
    if (!new_pixbuf) {
        if (error) {
            std::cerr << "[WallpaperEngine] Failed to load wallpaper: " << error->message << std::endl;
            g_error_free(error);
        }
        return;
    }

    // Advance previous pixbuf for crossfade
    if (s_current_pixbuf) {
        if (s_previous_pixbuf) {
            g_object_unref(s_previous_pixbuf);
        }
        s_previous_pixbuf = s_current_pixbuf;
    }

    s_current_pixbuf = new_pixbuf;
    s_current_path = path;

    // Start 60fps frame tick animation
    s_transition_progress = (s_previous_pixbuf != nullptr) ? 0.0 : 1.0;
    s_transition_start_time = g_get_monotonic_time();

    if (s_transition_progress < 1.0 && s_tick_id == 0 && !s_views.empty()) {
        s_tick_id = gtk_widget_add_tick_callback(s_views[0].drawing_area, on_tick, nullptr, nullptr);
    }

    queue_draw_all();
}

void WallpaperEngine::set_transition_duration(int ms) {
    s_transition_duration_ms = std::max(50, ms);
}

std::string WallpaperEngine::get_current_wallpaper() {
    return s_current_path;
}

} // namespace zenith
