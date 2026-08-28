#include "dbus/notification_manager.hpp"
#include "gtk3_compat.hpp"
#include <gtk-layer-shell/gtk-layer-shell.h>
#include <cairo.h>
#include <iostream>
#include <ctime>

namespace zenith {

GtkApplication* NotificationManager::gtk_app = nullptr;
GDBusNodeInfo* NotificationManager::introspection_data = nullptr;
guint NotificationManager::owner_id = 0;
uint32_t NotificationManager::next_id = 1;
std::vector<NotificationItem> NotificationManager::history;
std::function<void()> NotificationManager::history_cb = nullptr;

static const gchar introspection_xml[] =
    "<node>"
    "  <interface name='org.freedesktop.Notifications'>"
    "    <method name='GetCapabilities'>"
    "      <arg type='as' name='capabilities' direction='out'/>"
    "    </method>"
    "    <method name='Notify'>"
    "      <arg type='s' name='app_name' direction='in'/>"
    "      <arg type='u' name='replaces_id' direction='in'/>"
    "      <arg type='s' name='app_icon' direction='in'/>"
    "      <arg type='s' name='summary' direction='in'/>"
    "      <arg type='s' name='body' direction='in'/>"
    "      <arg type='as' name='actions' direction='in'/>"
    "      <arg type='a{sv}' name='hints' direction='in'/>"
    "      <arg type='i' name='expire_timeout' direction='in'/>"
    "      <arg type='u' name='id' direction='out'/>"
    "    </method>"
    "    <method name='CloseNotification'>"
    "      <arg type='u' name='id' direction='in'/>"
    "    </method>"
    "    <method name='GetServerInformation'>"
    "      <arg type='s' name='name' direction='out'/>"
    "      <arg type='s' name='vendor' direction='out'/>"
    "      <arg type='s' name='version' direction='out'/>"
    "      <arg type='s' name='spec_version' direction='out'/>"
    "    </method>"
    "    <signal name='NotificationClosed'>"
    "      <arg type='u' name='id'/>"
    "      <arg type='u' name='reason'/>"
    "    </signal>"
    "    <signal name='ActionInvoked'>"
    "      <arg type='u' name='id'/>"
    "      <arg type='s' name='action_key'/>"
    "    </signal>"
    "  </interface>"
    "</node>";

void NotificationManager::init(GtkApplication* app) {
    gtk_app = app;

    GError* error = nullptr;
    introspection_data = g_dbus_node_info_new_for_xml(introspection_xml, &error);
    if (!introspection_data) {
        std::cerr << "[NotificationManager] Introspection XML parse failed: "
                  << (error ? error->message : "unknown") << std::endl;
        if (error) g_error_free(error);
        return;
    }

    owner_id = g_bus_own_name(G_BUS_TYPE_SESSION,
                              "org.freedesktop.Notifications",
                              G_BUS_NAME_OWNER_FLAGS_REPLACE,
                              on_bus_acquired,
                              on_name_acquired,
                              on_name_lost,
                              nullptr,
                              nullptr);
}

void NotificationManager::cleanup() {
    if (owner_id > 0) {
        g_bus_unown_name(owner_id);
        owner_id = 0;
    }
    if (introspection_data) {
        g_dbus_node_info_unref(introspection_data);
        introspection_data = nullptr;
    }
}

void NotificationManager::show_toast(const NotificationItem& item, int timeout_ms) {
    if (!gtk_app) return;

    GtkWidget* win = gtk_application_window_new(gtk_app);
    gtk_widget_add_css_class(win, "toast-window");

    // Enable true RGBA transparency so rounded corners don't render grey/black box artifacts
    GdkScreen* screen = gtk_widget_get_screen(win);
    GdkVisual* visual = gdk_screen_get_rgba_visual(screen);
    if (visual) {
        gtk_widget_set_visual(win, visual);
    }
    gtk_widget_set_app_paintable(win, TRUE);

    g_signal_connect(win, "draw", G_CALLBACK(+[](GtkWidget*, cairo_t* cr, gpointer) -> gboolean {
        cairo_save(cr);
        cairo_set_operator(cr, CAIRO_OPERATOR_CLEAR);
        cairo_paint(cr);
        cairo_restore(cr);
        return FALSE;
    }), nullptr);

    gtk_layer_init_for_window(GTK_WINDOW(win));
    gtk_layer_set_layer(GTK_WINDOW(win), GTK_LAYER_SHELL_LAYER_OVERLAY);
    gtk_layer_set_keyboard_mode(GTK_WINDOW(win), GTK_LAYER_SHELL_KEYBOARD_MODE_NONE);

    // Anchor Top Right
    gtk_layer_set_anchor(GTK_WINDOW(win), GTK_LAYER_SHELL_EDGE_TOP, TRUE);
    gtk_layer_set_anchor(GTK_WINDOW(win), GTK_LAYER_SHELL_EDGE_RIGHT, TRUE);
    gtk_layer_set_anchor(GTK_WINDOW(win), GTK_LAYER_SHELL_EDGE_BOTTOM, FALSE);
    gtk_layer_set_anchor(GTK_WINDOW(win), GTK_LAYER_SHELL_EDGE_LEFT, FALSE);
    gtk_layer_set_margin(GTK_WINDOW(win), GTK_LAYER_SHELL_EDGE_TOP, 48);
    gtk_layer_set_margin(GTK_WINDOW(win), GTK_LAYER_SHELL_EDGE_RIGHT, 14);

    GtkWidget* card = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    gtk_widget_add_css_class(card, "notification-toast");
    gtk_widget_set_size_request(card, 360, -1);

    // Header: [󰂚 Icon] [App Name] ------- [Timestamp] [󰅖]
    GtkWidget* header = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    GtkWidget* icon_w = gtk_label_new(item.app_icon.empty() ? "󰂚" : item.app_icon.c_str());
    gtk_widget_add_css_class(icon_w, "toast-icon");

    GtkWidget* app_lbl = gtk_label_new(item.app_name.empty() ? "Notification" : item.app_name.c_str());
    gtk_widget_add_css_class(app_lbl, "toast-app-name");
    gtk_widget_set_halign(app_lbl, GTK_ALIGN_START);

    GtkWidget* time_lbl = gtk_label_new(item.timestamp.c_str());
    gtk_widget_add_css_class(time_lbl, "toast-time");

    GtkWidget* close_btn = gtk_button_new_with_label("󰅖");
    gtk_widget_add_css_class(close_btn, "toast-close-btn");
    g_signal_connect_swapped(close_btn, "clicked", G_CALLBACK(gtk_widget_destroy), win);

    gtk_box_pack_start(GTK_BOX(header), icon_w, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(header), app_lbl, FALSE, FALSE, 0);
    gtk_box_pack_end(GTK_BOX(header), close_btn, FALSE, FALSE, 0);
    gtk_box_pack_end(GTK_BOX(header), time_lbl, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(card), header, FALSE, FALSE, 0);

    // Summary Title
    if (!item.summary.empty()) {
        GtkWidget* sum_lbl = gtk_label_new(item.summary.c_str());
        gtk_widget_add_css_class(sum_lbl, "toast-summary");
        gtk_widget_set_halign(sum_lbl, GTK_ALIGN_START);
        gtk_label_set_line_wrap(GTK_LABEL(sum_lbl), TRUE);
        gtk_box_pack_start(GTK_BOX(card), sum_lbl, FALSE, FALSE, 0);
    }

    // Body Text
    if (!item.body.empty()) {
        GtkWidget* body_lbl = gtk_label_new(item.body.c_str());
        gtk_widget_add_css_class(body_lbl, "toast-body");
        gtk_widget_set_halign(body_lbl, GTK_ALIGN_START);
        gtk_label_set_line_wrap(GTK_LABEL(body_lbl), TRUE);
        gtk_label_set_max_width_chars(GTK_LABEL(body_lbl), 36);
        gtk_box_pack_start(GTK_BOX(card), body_lbl, FALSE, FALSE, 0);
    }

    gtk_container_add(GTK_CONTAINER(win), card);
    gtk_widget_show_all(win);

    int timeout = (timeout_ms > 0) ? timeout_ms : 5000;
    g_timeout_add(timeout, [](gpointer data) -> gboolean {
        GtkWidget* w = GTK_WIDGET(data);
        if (GTK_IS_WIDGET(w)) {
            gtk_widget_destroy(w);
        }
        return FALSE;
    }, win);
}

void NotificationManager::on_bus_acquired(GDBusConnection* connection, const gchar*, gpointer) {
    static const GDBusInterfaceVTable interface_vtable = {
        NotificationManager::handle_method_call,
        nullptr,
        nullptr
    };

    GError* error = nullptr;
    guint registration_id = g_dbus_connection_register_object(
        connection,
        "/org/freedesktop/Notifications",
        introspection_data->interfaces[0],
        &interface_vtable,
        nullptr,
        nullptr,
        &error
    );

    if (registration_id == 0) {
        std::cerr << "[NotificationManager] Object registration failed: "
                  << (error ? error->message : "unknown") << std::endl;
        if (error) g_error_free(error);
    } else {
        std::cout << "[NotificationManager] Claimed DBus org.freedesktop.Notifications service\n";
    }
}

void NotificationManager::on_name_acquired(GDBusConnection*, const gchar* name, gpointer) {
    std::cout << "[NotificationManager] Acquired bus name: " << name << std::endl;
}

void NotificationManager::on_name_lost(GDBusConnection*, const gchar* name, gpointer) {
    std::cout << "[NotificationManager] Lost bus name: " << name << std::endl;
}

void NotificationManager::handle_method_call(GDBusConnection*,
                                             const gchar*,
                                             const gchar*,
                                             const gchar*,
                                             const gchar* method_name,
                                             GVariant* parameters,
                                             GDBusMethodInvocation* invocation,
                                             gpointer) {
    std::string method(method_name);

    if (method == "GetCapabilities") {
        GVariantBuilder builder;
        g_variant_builder_init(&builder, G_VARIANT_TYPE("as"));
        g_variant_builder_add(&builder, "s", "body");
        g_variant_builder_add(&builder, "s", "body-markup");
        g_variant_builder_add(&builder, "s", "actions");
        g_variant_builder_add(&builder, "s", "icon-static");
        g_variant_builder_add(&builder, "s", "persistence");
        g_dbus_method_invocation_return_value(invocation, g_variant_new("(as)", &builder));
    } else if (method == "GetServerInformation") {
        g_dbus_method_invocation_return_value(
            invocation,
            g_variant_new("(ssss)", "ZenithShell Notifications", "Zenith", "1.0.0", "1.2")
        );
    } else if (method == "Notify") {
        const char *app_name = "", *app_icon = "", *summary = "", *body = "";
        uint32_t replaces_id = 0;
        int32_t expire_timeout = -1;
        GVariant* actions = nullptr;
        GVariant* hints = nullptr;

        g_variant_get(parameters, "(&su&s&s&s@as@a{sv}i)",
                      &app_name, &replaces_id, &app_icon, &summary, &body,
                      &actions, &hints, &expire_timeout);

        uint32_t id = (replaces_id != 0) ? replaces_id : next_id++;

        auto now = std::time(nullptr);
        char tbuf[16];
        std::strftime(tbuf, sizeof(tbuf), "%H:%M", std::localtime(&now));

        NotificationItem item{
            id,
            std::string(app_name),
            std::string(app_icon),
            std::string(summary),
            std::string(body),
            std::string(tbuf)
        };

        history.insert(history.begin(), item);
        if (history.size() > 50) history.pop_back();

        int timeout = (expire_timeout > 0) ? expire_timeout : 5000;
        g_idle_add([](gpointer data) -> gboolean {
            auto* p = static_cast<std::pair<NotificationItem, int>*>(data);
            NotificationManager::show_toast(p->first, p->second);
            if (NotificationManager::history_cb) {
                NotificationManager::history_cb();
            }
            delete p;
            return FALSE;
        }, new std::pair<NotificationItem, int>(item, timeout));

        g_dbus_method_invocation_return_value(invocation, g_variant_new("(u)", id));
    } else if (method == "CloseNotification") {
        g_dbus_method_invocation_return_value(invocation, nullptr);
    } else {
        g_dbus_method_invocation_return_error(invocation, G_DBUS_ERROR, G_DBUS_ERROR_UNKNOWN_METHOD, "Unknown method");
    }
}

int NotificationManager::get_count() {
    return static_cast<int>(history.size());
}

const std::vector<NotificationItem>& NotificationManager::get_history() {
    return history;
}

const NotificationItem* NotificationManager::get_latest() {
    if (history.empty()) return nullptr;
    return &history.front();
}

void NotificationManager::clear_history() {
    history.clear();
    if (history_cb) history_cb();
}

void NotificationManager::remove_notification(uint32_t id) {
    for (auto it = history.begin(); it != history.end(); ++it) {
        if (it->id == id) {
            history.erase(it);
            break;
        }
    }
    if (history_cb) history_cb();
}

void NotificationManager::set_history_changed_callback(std::function<void()> cb) {
    history_cb = cb;
}

} // namespace zenith
