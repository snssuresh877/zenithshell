#include "shell/tray/system_tray_manager.hpp"
#include "gtk3_compat.hpp"
#include <iostream>
#include <filesystem>
#include <algorithm>
#include <unistd.h>

namespace fs = std::filesystem;

namespace zenith {

GDBusConnection* SystemTrayManager::dbus_conn = nullptr;
std::vector<TrayItem> SystemTrayManager::current_items;
std::function<void()> SystemTrayManager::change_cb = nullptr;
guint SystemTrayManager::reg_sub_id = 0;
guint SystemTrayManager::unreg_sub_id = 0;

static GDBusNodeInfo* watcher_introspection = nullptr;
static guint watcher_owner_id = 0;
static std::vector<std::string> registered_services;

static const gchar watcher_xml[] =
    "<node>"
    "  <interface name='org.kde.StatusNotifierWatcher'>"
    "    <method name='RegisterStatusNotifierItem'>"
    "      <arg type='s' name='service' direction='in'/>"
    "    </method>"
    "    <method name='RegisterStatusNotifierHost'>"
    "      <arg type='s' name='service' direction='in'/>"
    "    </method>"
    "    <property name='RegisteredStatusNotifierItems' type='as' access='read'/>"
    "    <property name='IsStatusNotifierHostRegistered' type='b' access='read'/>"
    "    <property name='ProtocolVersion' type='i' access='read'/>"
    "    <signal name='StatusNotifierItemRegistered'>"
    "      <arg type='s' name='service'/>"
    "    </signal>"
    "    <signal name='StatusNotifierItemUnregistered'>"
    "      <arg type='s' name='service'/>"
    "    </signal>"
    "    <signal name='StatusNotifierHostRegistered'/>"
    "  </interface>"
    "</node>";

static void watcher_method_call(GDBusConnection*, const gchar*, const gchar*,
                                const gchar*, const gchar* method_name,
                                GVariant* parameters, GDBusMethodInvocation* invocation, gpointer) {
    std::string method(method_name);
    if (method == "RegisterStatusNotifierItem") {
        const char* s = nullptr;
        g_variant_get(parameters, "(&s)", &s);
        if (s) {
            std::string service_str = s;
            g_idle_add([](gpointer data) -> gboolean {
                char* str = static_cast<char*>(data);
                SystemTrayManager::init(); // Triggers refresh
                free(str);
                return FALSE;
            }, strdup(service_str.c_str()));
        }
        g_dbus_method_invocation_return_value(invocation, g_variant_new("()"));
    } else if (method == "RegisterStatusNotifierHost") {
        g_dbus_method_invocation_return_value(invocation, g_variant_new("()"));
    } else {
        g_dbus_method_invocation_return_value(invocation, nullptr);
    }
}

static GVariant* watcher_get_property(GDBusConnection*, const gchar*, const gchar*,
                                      const gchar*, const gchar* prop_name,
                                      GError**, gpointer) {
    std::string prop(prop_name);
    if (prop == "RegisteredStatusNotifierItems") {
        GVariantBuilder b;
        g_variant_builder_init(&b, G_VARIANT_TYPE("as"));
        for (const auto& item : SystemTrayManager::get_tray_items()) {
            g_variant_builder_add(&b, "s", (item.service + item.path).c_str());
        }
        return g_variant_builder_end(&b);
    } else if (prop == "IsStatusNotifierHostRegistered") {
        return g_variant_new_boolean(TRUE);
    } else if (prop == "ProtocolVersion") {
        return g_variant_new_int32(0);
    }
    return nullptr;
}

void SystemTrayManager::init() {
    GError* error = nullptr;
    if (!dbus_conn) {
        dbus_conn = g_bus_get_sync(G_BUS_TYPE_SESSION, nullptr, &error);
        if (!dbus_conn) {
            std::cerr << "[SystemTrayManager] Failed to connect to session bus: "
                      << (error ? error->message : "unknown") << std::endl;
            if (error) g_error_free(error);
            return;
        }
    }

    // Host org.kde.StatusNotifierWatcher if not already hosted
    if (!watcher_introspection) {
        watcher_introspection = g_dbus_node_info_new_for_xml(watcher_xml, nullptr);
        if (watcher_introspection) {
            static const GDBusInterfaceVTable vtable = {
                watcher_method_call,
                watcher_get_property,
                nullptr,
                { nullptr }
            };

            g_dbus_connection_register_object(
                dbus_conn,
                "/StatusNotifierWatcher",
                watcher_introspection->interfaces[0],
                &vtable,
                nullptr,
                nullptr,
                nullptr
            );

            watcher_owner_id = g_bus_own_name_on_connection(
                dbus_conn,
                "org.kde.StatusNotifierWatcher",
                G_BUS_NAME_OWNER_FLAGS_REPLACE,
                nullptr,
                nullptr,
                nullptr,
                nullptr
            );

            std::string host_name = "org.kde.StatusNotifierHost-" + std::to_string(getpid()) + "-0";
            g_bus_own_name_on_connection(
                dbus_conn,
                host_name.c_str(),
                G_BUS_NAME_OWNER_FLAGS_NONE,
                nullptr,
                nullptr,
                nullptr,
                nullptr
            );
        }

        // Subscribe to NameOwnerChanged to detect SNI apps launching/closing
        g_dbus_connection_signal_subscribe(
            dbus_conn,
            "org.freedesktop.DBus",
            "org.freedesktop.DBus",
            "NameOwnerChanged",
            "/org/freedesktop/DBus",
            nullptr,
            G_DBUS_SIGNAL_FLAGS_NONE,
            [](GDBusConnection*, const gchar*, const gchar*, const gchar*,
               const gchar*, GVariant* params, gpointer) {
                const char *name = "", *old_owner = "", *new_owner = "";
                g_variant_get(params, "(&s&s&s)", &name, &old_owner, &new_owner);
                std::string sname(name);
                if (sname.find("StatusNotifierItem") != std::string::npos ||
                    sname.find("proton") != std::string::npos ||
                    sname.find("wechat") != std::string::npos) {
                    g_idle_add([](gpointer) -> gboolean {
                        SystemTrayManager::fetch_all_items();
                        return FALSE;
                    }, nullptr);
                }
            },
            nullptr,
            nullptr
        );
    }

    fetch_all_items();
}

void SystemTrayManager::fetch_all_items() {
    if (!dbus_conn) return;

    GError* error = nullptr;
    GVariant* res = g_dbus_connection_call_sync(
        dbus_conn,
        "org.freedesktop.DBus",
        "/org/freedesktop/DBus",
        "org.freedesktop.DBus",
        "ListNames",
        nullptr,
        G_VARIANT_TYPE("(as)"),
        G_DBUS_CALL_FLAGS_NONE,
        500,
        nullptr,
        &error
    );

    if (!res) {
        if (error) g_error_free(error);
        return;
    }

    GVariant* val = nullptr;
    g_variant_get(res, "(@as)", &val);

    std::vector<std::string> sni_services;
    GVariantIter iter;
    g_variant_iter_init(&iter, val);
    const gchar* item_str = nullptr;

    while (g_variant_iter_loop(&iter, "&s", &item_str)) {
        std::string name(item_str);
        if (name.find("org.kde.StatusNotifierItem-") == 0) {
            sni_services.push_back(name);
        }
    }

    g_variant_unref(val);
    g_variant_unref(res);

    current_items.clear();
    for (const auto& s : sni_services) {
        TrayItem item = query_item_properties(s, "/StatusNotifierItem");
        if (!item.service.empty()) {
            current_items.push_back(item);
            std::cout << "[SystemTrayManager] Found SNI Tray item: " << item.title
                      << " (" << item.id << " / " << item.service << ")" << std::endl;
        }
    }

    if (change_cb) {
        change_cb();
    }
}

TrayItem SystemTrayManager::query_item_properties(const std::string& service, const std::string& path) {
    TrayItem item;
    item.service = service;
    item.path = path;
    item.id = service;

    if (!dbus_conn) return item;

    GError* error = nullptr;
    GVariant* res = g_dbus_connection_call_sync(
        dbus_conn,
        service.c_str(),
        path.c_str(),
        "org.freedesktop.DBus.Properties",
        "GetAll",
        g_variant_new("(s)", "org.kde.StatusNotifierItem"),
        G_VARIANT_TYPE("(a{sv})"),
        G_DBUS_CALL_FLAGS_NONE,
        500,
        nullptr,
        &error
    );

    if (!res) {
        if (error) g_error_free(error);
        return item;
    }

    GVariant* dict = nullptr;
    g_variant_get(res, "(@a{sv})", &dict);

    GVariantIter iter;
    g_variant_iter_init(&iter, dict);
    const gchar* key = nullptr;
    GVariant* val = nullptr;

    while (g_variant_iter_loop(&iter, "{&sv}", &key, &val)) {
        std::string k(key);
        if (k == "Id" && g_variant_is_of_type(val, G_VARIANT_TYPE_STRING)) {
            item.id = g_variant_get_string(val, nullptr);
        } else if (k == "Title" && g_variant_is_of_type(val, G_VARIANT_TYPE_STRING)) {
            item.title = g_variant_get_string(val, nullptr);
        } else if (k == "IconName" && g_variant_is_of_type(val, G_VARIANT_TYPE_STRING)) {
            item.icon_name = g_variant_get_string(val, nullptr);
        } else if (k == "IconAccessibleDesc" && g_variant_is_of_type(val, G_VARIANT_TYPE_STRING)) {
            item.tooltip = g_variant_get_string(val, nullptr);
        }
    }

    if (item.title.empty()) item.title = item.id;
    if (item.tooltip.empty()) item.tooltip = item.title;

    g_variant_unref(dict);
    g_variant_unref(res);
    return item;
}

void SystemTrayManager::activate_item(const std::string& service, const std::string& path) {
    if (!dbus_conn || service.empty()) return;

    // 1. Call standard SNI Activate(0, 0)
    g_dbus_connection_call(
        dbus_conn,
        service.c_str(),
        path.empty() ? "/StatusNotifierItem" : path.c_str(),
        "org.kde.StatusNotifierItem",
        "Activate",
        g_variant_new("(ii)", 0, 0),
        nullptr,
        G_DBUS_CALL_FLAGS_NONE,
        -1,
        nullptr,
        nullptr,
        nullptr
    );

    // 2. Also try org.freedesktop.Application.Activate for Gtk/Qt desktop apps
    std::string app_service = service;
    if (app_service.find("org.kde.StatusNotifierItem-") == 0) {
        size_t dash1 = app_service.find('-');
        size_t dash2 = app_service.rfind('-');
        if (dash1 != std::string::npos && dash2 != std::string::npos && dash2 > dash1) {
            app_service = app_service.substr(dash1 + 1, dash2 - dash1 - 1);
        }
    }

    if (!app_service.empty() && app_service.find('.') != std::string::npos) {
        std::string obj_path = "/";
        for (char c : app_service) {
            obj_path += (c == '.') ? '/' : c;
        }

        GVariantBuilder builder;
        g_variant_builder_init(&builder, G_VARIANT_TYPE("a{sv}"));
        g_dbus_connection_call(
            dbus_conn,
            app_service.c_str(),
            obj_path.c_str(),
            "org.freedesktop.Application",
            "Activate",
            g_variant_new("(a{sv})", &builder),
            nullptr,
            G_DBUS_CALL_FLAGS_NONE,
            -1,
            nullptr,
            nullptr,
            nullptr
        );
    }

    // 3. Focus corresponding window in Hyprland if it appears
    std::string focus_cmd = "hyprctl dispatch 'hl.dsp.focus({window=\"class:" + app_service + "\"})' 2>/dev/null &";
    system(focus_cmd.c_str());

    std::cout << "[SystemTrayManager] Activated tray item & application: " << service << " (" << app_service << ")" << std::endl;
}

std::vector<TrayItem> SystemTrayManager::get_tray_items() {
    return current_items;
}

void SystemTrayManager::set_changed_callback(std::function<void()> cb) {
    change_cb = cb;
}

GtkWidget* SystemTrayManager::create_tray_box() {
    GtkWidget* tray_container = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
    gtk_widget_add_css_class(tray_container, "pill-widget");
    gtk_widget_add_css_class(tray_container, "tray-drawer-pill");
    gtk_widget_set_size_request(tray_container, -1, 32);

    GtkWidget* icons_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
    gtk_widget_add_css_class(icons_box, "tray-icons-box");
    gtk_box_pack_start(GTK_BOX(tray_container), icons_box, TRUE, TRUE, 0);

    auto render_icons = [icons_box, tray_container]() {
        // Clear old icon children
        GList* children = gtk_container_get_children(GTK_CONTAINER(icons_box));
        for (GList* l = children; l != nullptr; l = l->next) {
            gtk_widget_destroy(GTK_WIDGET(l->data));
        }
        g_list_free(children);

        const auto& items = SystemTrayManager::get_tray_items();
        if (items.empty()) {
            gtk_widget_set_visible(tray_container, FALSE);
            return;
        }

        gtk_widget_set_visible(tray_container, TRUE);

        for (const auto& item : items) {
            GtkWidget* item_btn = gtk_button_new();
            gtk_widget_add_css_class(item_btn, "tray-item-btn");
            gtk_widget_set_tooltip_text(item_btn, item.tooltip.c_str());

            GtkWidget* icon_w = nullptr;
            if (!item.icon_name.empty() && g_file_test(item.icon_name.c_str(), G_FILE_TEST_EXISTS)) {
                // Direct file path icon (e.g. Proton VPN state-connected.svg)
                icon_w = gtk_image_new_from_file(item.icon_name.c_str());
            } else {
                std::string icon_key = item.icon_name.empty() ? item.id : item.icon_name;
                if (icon_key == "wechat") icon_key = "wechat";
                else if (icon_key.find("proton") != std::string::npos) icon_key = "protonvpn-gui";

                GtkIconTheme* theme = gtk_icon_theme_get_default();
                if (gtk_icon_theme_has_icon(theme, icon_key.c_str())) {
                    icon_w = gtk_image_new_from_icon_name(icon_key.c_str(), GTK_ICON_SIZE_MENU);
                } else if (icon_key == "wechat" || item.title == "wechat" || item.id == "wechat") {
                    icon_w = gtk_label_new("󰘑");
                    gtk_widget_add_css_class(icon_w, "tray-fallback-icon");
                } else if (icon_key.find("proton") != std::string::npos || icon_key.find("vpn") != std::string::npos) {
                    icon_w = gtk_label_new("󰖟");
                    gtk_widget_add_css_class(icon_w, "tray-fallback-icon");
                } else {
                    icon_w = gtk_label_new("󰍜");
                    gtk_widget_add_css_class(icon_w, "tray-fallback-icon");
                }
            }

            if (GTK_IS_IMAGE(icon_w)) {
                gtk_image_set_pixel_size(GTK_IMAGE(icon_w), 16);
            }

            gtk_container_add(GTK_CONTAINER(item_btn), icon_w);

            using TrayPair = std::pair<std::string, std::string>;
            auto* tp = new TrayPair(item.service, item.path);
            g_signal_connect(item_btn, "clicked", G_CALLBACK(+[](GtkButton*, gpointer data) {
                auto* pair = static_cast<TrayPair*>(data);
                SystemTrayManager::activate_item(pair->first, pair->second);
            }), tp);

            gtk_box_pack_start(GTK_BOX(icons_box), item_btn, FALSE, FALSE, 0);
        }

        gtk_widget_show_all(icons_box);
    };

    SystemTrayManager::set_changed_callback([render_icons]() {
        g_idle_add([](gpointer data) -> gboolean {
            auto* fn = static_cast<std::function<void()>*>(data);
            (*fn)();
            return FALSE;
        }, new std::function<void()>(render_icons));
    });

    render_icons();

    return tray_container;
}

} // namespace zenith
