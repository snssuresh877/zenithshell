#include "app/app.hpp"
#include <iostream>
#include <string>
#include <gio/gio.h>

int main(int argc, char** argv) {
    if (argc > 1 && std::string(argv[1]) == "--clip-store") {
        std::string input((std::istreambuf_iterator<char>(std::cin)), std::istreambuf_iterator<char>());
        if (input.empty()) return 0;

        GError* error = nullptr;
        GDBusConnection* conn = g_bus_get_sync(G_BUS_TYPE_SESSION, nullptr, &error);
        if (!conn) {
            if (error) g_error_free(error);
            return 1;
        }

        GVariant* val = g_variant_new("(s)", input.c_str());
        GVariant* res = g_dbus_connection_call_sync(
            conn,
            "dev.zenith.Shell",
            "/dev/zenith/Shell",
            "dev.zenith.Shell",
            "StoreClipboard",
            val,
            nullptr,
            G_DBUS_CALL_FLAGS_NONE,
            1000,
            nullptr,
            &error
        );
        if (res) g_variant_unref(res);
        if (error) g_error_free(error);
        g_object_unref(conn);
        return 0;
    }

    zenith::App app(argc, argv);
    return app.run();
}
