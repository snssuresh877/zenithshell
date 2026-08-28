#include "shell/control_center/wifi_manager.hpp"
#include "gtk3_compat.hpp"
#include <gtk-layer-shell/gtk-layer-shell.h>
#include <gdk/gdk.h>
#include <gdk/gdkkeysyms.h>
#include <iostream>
#include <array>
#include <memory>
#include <sstream>
#include <algorithm>
#include <map>
#include <unordered_set>
#include <thread>
#include <cairo.h>

namespace zenith {

GtkWidget* WifiManager::window = nullptr;
GtkWidget* WifiManager::main_card = nullptr;
GtkWidget* WifiManager::list_container = nullptr;
GtkWidget* WifiManager::listbox = nullptr;
GtkWidget* WifiManager::active_box = nullptr;
GtkWidget* WifiManager::wifi_switch = nullptr;
GtkWidget* WifiManager::scan_spinner = nullptr;
GtkWidget* WifiManager::status_msg_lbl = nullptr;
GtkWidget* WifiManager::count_lbl = nullptr;
GtkWidget* WifiManager::ping_lbl = nullptr;
GtkWidget* WifiManager::vpn_btn = nullptr;
GtkWidget* WifiManager::vpn_sub_lbl = nullptr;
GtkWidget* WifiManager::hotspot_btn = nullptr;
GtkWidget* WifiManager::hotspot_sub_lbl = nullptr;
GtkWidget* WifiManager::qr_view_box = nullptr;
GtkWidget* WifiManager::qr_image = nullptr;
GtkWidget* WifiManager::qr_ssid_lbl = nullptr;
GtkWidget* WifiManager::qr_pass_lbl = nullptr;
GtkWidget* WifiManager::qr_copy_btn = nullptr;
std::string WifiManager::pending_connect_ssid = "";
std::string WifiManager::current_wifi_password = "";
std::vector<WifiNetwork> WifiManager::networks;
ActiveNetInfo WifiManager::current_info;

void WifiManager::init(GtkApplication* app) {
    if (window) return;

    if (app) {
        window = gtk_application_window_new(app);
    } else {
        window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    }

    gtk_widget_add_css_class(window, "network-window");
    gtk_window_set_default_size(GTK_WINDOW(window), 450, 560);

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
    gtk_widget_add_css_class(backdrop, "network-backdrop");

    GtkWidget* outer_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_halign(outer_box, GTK_ALIGN_END);
    gtk_widget_set_valign(outer_box, GTK_ALIGN_START);
    gtk_widget_set_margin_top(outer_box, 42);
    gtk_widget_set_margin_end(outer_box, 130);

    main_card = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_widget_add_css_class(main_card, "network-card");

    auto on_backdrop_clicked = +[](GtkWidget* widget, GdkEventButton* event, gpointer user_data) -> gboolean {
        GtkWidget* card = static_cast<GtkWidget*>(user_data);
        if (!card) return FALSE;
        GtkAllocation alloc;
        gtk_widget_get_allocation(card, &alloc);
        int wx = 0, wy = 0;
        gtk_widget_translate_coordinates(widget, card, static_cast<int>(event->x), static_cast<int>(event->y), &wx, &wy);
        if (wx >= 0 && wx < alloc.width && wy >= 0 && wy < alloc.height) {
            return FALSE;
        }
        WifiManager::hide_panel();
        return TRUE;
    };
    g_signal_connect(backdrop, "button-press-event", G_CALLBACK(on_backdrop_clicked), main_card);

    // 1. Header with Title, Wi-Fi Switch, Spinner, Rescan & Close
    GtkWidget* header = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_widget_add_css_class(header, "network-header");

    GtkWidget* icon = gtk_label_new("");
    gtk_widget_add_css_class(icon, "network-header-icon");

    GtkWidget* title = gtk_label_new("Network & Wi-Fi");
    gtk_widget_add_css_class(title, "network-header-title");
    gtk_widget_set_halign(title, GTK_ALIGN_START);

    scan_spinner = gtk_spinner_new();
    gtk_widget_set_no_show_all(scan_spinner, TRUE);

    wifi_switch = gtk_switch_new();
    gtk_widget_add_css_class(wifi_switch, "network-wifi-switch");
    gtk_widget_set_valign(wifi_switch, GTK_ALIGN_CENTER);
    gtk_switch_set_active(GTK_SWITCH(wifi_switch), TRUE);

    g_signal_connect(wifi_switch, "state-set", G_CALLBACK(+[](GtkSwitch*, gboolean state, gpointer) -> gboolean {
        WifiManager::toggle_wifi_radio(state);
        return FALSE;
    }), nullptr);

    GtkWidget* scan_btn = gtk_button_new_with_label("");
    gtk_widget_add_css_class(scan_btn, "btn-icon-action");
    gtk_widget_set_tooltip_text(scan_btn, "Rescan Wi-Fi Networks");
    g_signal_connect(scan_btn, "clicked", G_CALLBACK(+[](GtkButton*, gpointer) {
        if (status_msg_lbl) {
            gtk_label_set_text(GTK_LABEL(status_msg_lbl), "Rescanning Wi-Fi networks...");
            gtk_widget_show(status_msg_lbl);
        }
        WifiManager::fetch_active_connection();
        WifiManager::scan_networks(true);
    }), nullptr);

    GtkWidget* close_btn = gtk_button_new_with_label("");
    gtk_widget_add_css_class(close_btn, "btn-close-action");
    g_signal_connect(close_btn, "clicked", G_CALLBACK(+[](GtkButton*, gpointer) {
        WifiManager::hide_panel();
    }), nullptr);

    gtk_box_pack_start(GTK_BOX(header), icon, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(header), title, TRUE, TRUE, 0);
    gtk_box_pack_end(GTK_BOX(header), close_btn, FALSE, FALSE, 0);
    gtk_box_pack_end(GTK_BOX(header), scan_btn, FALSE, FALSE, 0);
    gtk_box_pack_end(GTK_BOX(header), scan_spinner, FALSE, FALSE, 4);
    gtk_box_pack_end(GTK_BOX(header), wifi_switch, FALSE, FALSE, 4);

    gtk_box_pack_start(GTK_BOX(main_card), header, FALSE, FALSE, 0);

    // Status Message Label
    status_msg_lbl = gtk_label_new("");
    gtk_widget_add_css_class(status_msg_lbl, "network-status-lbl");
    gtk_widget_set_halign(status_msg_lbl, GTK_ALIGN_START);
    gtk_widget_set_no_show_all(status_msg_lbl, TRUE);
    gtk_box_pack_start(GTK_BOX(main_card), status_msg_lbl, FALSE, FALSE, 0);

    // 2. Active Connection Card Box
    active_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    gtk_widget_add_css_class(active_box, "active-net-card");
    gtk_box_pack_start(GTK_BOX(main_card), active_box, FALSE, FALSE, 0);

    // 2.5 Privacy & Connectivity Quick Tools (VPN & Hotspot)
    GtkWidget* privacy_hdr = gtk_label_new("PRIVACY & SHARING");
    gtk_widget_add_css_class(privacy_hdr, "network-section-header");
    gtk_widget_set_halign(privacy_hdr, GTK_ALIGN_START);
    gtk_box_pack_start(GTK_BOX(main_card), privacy_hdr, FALSE, FALSE, 0);

    GtkWidget* privacy_grid = gtk_grid_new();
    gtk_widget_add_css_class(privacy_grid, "privacy-tools-grid");
    gtk_grid_set_column_spacing(GTK_GRID(privacy_grid), 8);
    gtk_grid_set_column_homogeneous(GTK_GRID(privacy_grid), TRUE);

    // VPN Tool Button
    vpn_btn = gtk_button_new();
    gtk_widget_add_css_class(vpn_btn, "privacy-tool-btn");
    gtk_widget_add_css_class(vpn_btn, "tool-vpn");
    GtkWidget* vpn_content = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
    GtkWidget* vpn_top = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    GtkWidget* vpn_icon = gtk_label_new("󰌾");
    gtk_widget_add_css_class(vpn_icon, "privacy-tool-icon");
    GtkWidget* vpn_title = gtk_label_new("VPN Security");
    gtk_widget_add_css_class(vpn_title, "privacy-tool-title");
    gtk_box_pack_start(GTK_BOX(vpn_top), vpn_icon, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(vpn_top), vpn_title, FALSE, FALSE, 0);

    vpn_sub_lbl = gtk_label_new("Disconnected");
    gtk_widget_add_css_class(vpn_sub_lbl, "privacy-tool-sub");
    gtk_widget_set_halign(vpn_sub_lbl, GTK_ALIGN_START);
    gtk_box_pack_start(GTK_BOX(vpn_content), vpn_top, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(vpn_content), vpn_sub_lbl, FALSE, FALSE, 0);
    gtk_container_add(GTK_CONTAINER(vpn_btn), vpn_content);
    g_signal_connect(vpn_btn, "clicked", G_CALLBACK(+[](GtkButton*, gpointer) {
        WifiManager::toggle_vpn();
    }), nullptr);

    // Hotspot Tool Button
    hotspot_btn = gtk_button_new();
    gtk_widget_add_css_class(hotspot_btn, "privacy-tool-btn");
    gtk_widget_add_css_class(hotspot_btn, "tool-hotspot");
    GtkWidget* hs_content = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
    GtkWidget* hs_top = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    GtkWidget* hs_icon = gtk_label_new("󰤨");
    gtk_widget_add_css_class(hs_icon, "privacy-tool-icon");
    GtkWidget* hs_title = gtk_label_new("Wi-Fi Hotspot");
    gtk_widget_add_css_class(hs_title, "privacy-tool-title");
    gtk_box_pack_start(GTK_BOX(hs_top), hs_icon, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(hs_top), hs_title, FALSE, FALSE, 0);

    hotspot_sub_lbl = gtk_label_new("Disabled");
    gtk_widget_add_css_class(hotspot_sub_lbl, "privacy-tool-sub");
    gtk_widget_set_halign(hotspot_sub_lbl, GTK_ALIGN_START);
    gtk_box_pack_start(GTK_BOX(hs_content), hs_top, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(hs_content), hotspot_sub_lbl, FALSE, FALSE, 0);
    gtk_container_add(GTK_CONTAINER(hotspot_btn), hs_content);
    g_signal_connect(hotspot_btn, "clicked", G_CALLBACK(+[](GtkButton*, gpointer) {
        WifiManager::toggle_hotspot();
    }), nullptr);

    gtk_grid_attach(GTK_GRID(privacy_grid), vpn_btn, 0, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(privacy_grid), hotspot_btn, 1, 0, 1, 1);
    gtk_box_pack_start(GTK_BOX(main_card), privacy_grid, FALSE, FALSE, 0);

    // 3. Normal Networks List Container
    list_container = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);

    GtkWidget* section_hdr = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    GtkWidget* section_title = gtk_label_new("AVAILABLE NETWORKS");
    gtk_widget_add_css_class(section_title, "network-section-header");
    gtk_widget_set_halign(section_title, GTK_ALIGN_START);

    count_lbl = gtk_label_new("");
    gtk_widget_add_css_class(count_lbl, "network-count-lbl");
    gtk_widget_set_halign(count_lbl, GTK_ALIGN_END);

    gtk_box_pack_start(GTK_BOX(section_hdr), section_title, TRUE, TRUE, 0);
    gtk_box_pack_end(GTK_BOX(section_hdr), count_lbl, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(list_container), section_hdr, FALSE, FALSE, 0);

    // Scrollable Wi-Fi Listbox
    GtkWidget* scroll = gtk_scrolled_window_new(nullptr, nullptr);
    gtk_widget_add_css_class(scroll, "network-scroll");
    gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(scroll), GTK_SHADOW_NONE);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_widget_set_size_request(scroll, -1, 260);
    gtk_widget_set_vexpand(scroll, TRUE);

    listbox = gtk_list_box_new();
    gtk_widget_add_css_class(listbox, "wifi-list");
    gtk_list_box_set_selection_mode(GTK_LIST_BOX(listbox), GTK_SELECTION_NONE);

    gtk_container_add(GTK_CONTAINER(scroll), listbox);
    gtk_box_pack_start(GTK_BOX(list_container), scroll, TRUE, TRUE, 0);

    gtk_box_pack_start(GTK_BOX(main_card), list_container, TRUE, TRUE, 0);

    // 4. Native QR Code Card Overlay (Hidden by default)
    qr_view_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_widget_add_css_class(qr_view_box, "qr-view-card");
    gtk_widget_set_no_show_all(qr_view_box, TRUE);

    GtkWidget* qr_hdr = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    GtkWidget* back_btn = gtk_button_new_with_label("    Back  ");
    gtk_widget_add_css_class(back_btn, "btn-qr-back");
    g_signal_connect(back_btn, "clicked", G_CALLBACK(+[](GtkButton*, gpointer) {
        WifiManager::close_qr_view();
    }), nullptr);

    qr_ssid_lbl = gtk_label_new("Wi-Fi Quick Connect");
    gtk_widget_add_css_class(qr_ssid_lbl, "qr-title-lbl");

    gtk_box_pack_start(GTK_BOX(qr_hdr), back_btn, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(qr_hdr), qr_ssid_lbl, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(qr_view_box), qr_hdr, FALSE, FALSE, 0);

    GtkWidget* img_container = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_add_css_class(img_container, "qr-img-frame");
    gtk_widget_set_halign(img_container, GTK_ALIGN_CENTER);

    qr_image = gtk_image_new();
    gtk_box_pack_start(GTK_BOX(img_container), qr_image, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(qr_view_box), img_container, FALSE, FALSE, 4);

    GtkWidget* pass_disp_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_widget_add_css_class(pass_disp_box, "qr-pass-box");
    gtk_widget_set_halign(pass_disp_box, GTK_ALIGN_CENTER);

    GtkWidget* pass_tag = gtk_label_new("Password:");
    gtk_widget_add_css_class(pass_tag, "qr-pass-tag");

    qr_pass_lbl = gtk_label_new("••••••••••");
    gtk_widget_add_css_class(qr_pass_lbl, "qr-pass-text");
    gtk_label_set_selectable(GTK_LABEL(qr_pass_lbl), TRUE);

    qr_copy_btn = gtk_button_new_with_label("󰅍 Copy");
    gtk_widget_add_css_class(qr_copy_btn, "btn-qr-copy");
    g_signal_connect(qr_copy_btn, "clicked", G_CALLBACK(+[](GtkButton*, gpointer) {
        if (!current_wifi_password.empty()) {
            std::string cmd = "printf '%s' '" + current_wifi_password + "' | wl-copy 2>/dev/null";
            system(cmd.c_str());
            if (qr_copy_btn) {
                gtk_button_set_label(GTK_BUTTON(qr_copy_btn), "󰄬 Copied!");
            }
        }
    }), nullptr);

    gtk_box_pack_start(GTK_BOX(pass_disp_box), pass_tag, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(pass_disp_box), qr_pass_lbl, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(pass_disp_box), qr_copy_btn, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(qr_view_box), pass_disp_box, FALSE, FALSE, 0);

    GtkWidget* scan_hint = gtk_label_new("Scan with your phone's camera to connect instantly");
    gtk_widget_add_css_class(scan_hint, "qr-hint-text");
    gtk_box_pack_start(GTK_BOX(qr_view_box), scan_hint, FALSE, FALSE, 0);

    gtk_box_pack_start(GTK_BOX(main_card), qr_view_box, FALSE, FALSE, 0);

    // 5. Bottom Utilities Toolbar
    GtkWidget* footer = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_widget_add_css_class(footer, "network-footer");

    GtkWidget* nm_btn = gtk_button_new_with_label(" Settings");
    gtk_widget_add_css_class(nm_btn, "network-footer-btn");
    gtk_widget_set_tooltip_text(nm_btn, "Open Advanced Network Connections (nm-connection-editor)");
    g_signal_connect(nm_btn, "clicked", G_CALLBACK(+[](GtkButton*, gpointer) {
        system("nm-connection-editor &");
    }), nullptr);

    GtkWidget* ping_btn = gtk_button_new_with_label(" Ping Test");
    gtk_widget_add_css_class(ping_btn, "network-footer-btn");
    gtk_widget_set_tooltip_text(ping_btn, "Test Live Internet Latency (Cloudflare DNS)");
    g_signal_connect(ping_btn, "clicked", G_CALLBACK(+[](GtkButton*, gpointer) {
        WifiManager::run_ping_test();
    }), nullptr);

    ping_lbl = gtk_label_new("Ping: --");
    gtk_widget_add_css_class(ping_lbl, "network-ping-lbl");
    gtk_widget_set_halign(ping_lbl, GTK_ALIGN_END);

    gtk_box_pack_start(GTK_BOX(footer), nm_btn, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(footer), ping_btn, FALSE, FALSE, 0);
    gtk_box_pack_end(GTK_BOX(footer), ping_lbl, FALSE, FALSE, 0);

    gtk_box_pack_end(GTK_BOX(main_card), footer, FALSE, FALSE, 0);

    gtk_box_pack_start(GTK_BOX(outer_box), main_card, FALSE, FALSE, 0);
    gtk_container_add(GTK_CONTAINER(backdrop), outer_box);
    gtk_container_add(GTK_CONTAINER(window), backdrop);

    g_signal_connect(window, "key-press-event", G_CALLBACK(+[](GtkWidget*, GdkEventKey* event, gpointer) -> gboolean {
        if (event->keyval == GDK_KEY_Escape) {
            if (qr_view_box && gtk_widget_get_visible(qr_view_box)) {
                WifiManager::close_qr_view();
                return TRUE;
            }
            WifiManager::hide_panel();
            return TRUE;
        }
        return FALSE;
    }), nullptr);

    gtk_widget_show_all(window);
    gtk_widget_hide(window);
}

static int64_t last_wifi_hide_time_ms = 0;

void WifiManager::toggle_panel() {
    init();
    if (!window) return;

    int64_t now = g_get_monotonic_time() / 1000;
    if (now - last_wifi_hide_time_ms < 250) {
        return;
    }

    if (gtk_widget_get_visible(window)) {
        hide_panel();
    } else {
        show_panel();
    }
}

void WifiManager::show_panel() {
    init();
    if (!window) return;
    close_qr_view();
    if (status_msg_lbl) gtk_widget_hide(status_msg_lbl);
    refresh_privacy_tools_ui();
    fetch_active_connection();
    scan_networks(false);
    gtk_widget_show(window);
    gtk_window_present(GTK_WINDOW(window));
    if (qr_view_box) gtk_widget_hide(qr_view_box);
}

void WifiManager::hide_panel() {
    if (!window) return;
    last_wifi_hide_time_ms = g_get_monotonic_time() / 1000;
    close_qr_view();
    if (status_msg_lbl) gtk_widget_hide(status_msg_lbl);
    gtk_widget_hide(window);
}

void WifiManager::close_qr_view() {
    if (qr_view_box) gtk_widget_hide(qr_view_box);
    if (list_container) gtk_widget_show_all(list_container);
}

void WifiManager::fetch_active_connection() {
    std::thread([]() {
        ActiveNetInfo info;
        info.is_connected = false;
        info.iface = "wlan0";
        info.type = "wifi";

        // 1. Get default route device & gateway
        FILE* fp = popen("ip route get 1.1.1.1 2>/dev/null", "r");
        if (fp) {
            char buf[256];
            if (fgets(buf, sizeof(buf), fp)) {
                std::string line(buf);
                std::stringstream ss(line);
                std::string token;
                while (ss >> token) {
                    if (token == "dev" && ss >> info.iface) {}
                    if (token == "via" && ss >> info.gateway) {}
                    if (token == "src" && ss >> info.ip_addr) {}
                }
                if (!info.iface.empty()) {
                    info.is_connected = true;
                }
            }
            pclose(fp);
        }

        if (info.is_connected) {
            if (info.iface.rfind("wl", 0) == 0) {
                info.type = "wifi";
            } else if (info.iface.rfind("eth", 0) == 0 || info.iface.rfind("en", 0) == 0) {
                info.type = "ethernet";
            }

            // 2. Get connection name and IP from nmcli dev show
            std::string cmd = "nmcli -t -f GENERAL.CONNECTION,IP4.ADDRESS,IP4.GATEWAY dev show " + info.iface + " 2>/dev/null";
            fp = popen(cmd.c_str(), "r");
            if (fp) {
                char buf[256];
                while (fgets(buf, sizeof(buf), fp)) {
                    std::string l(buf);
                    if (l.find("GENERAL.CONNECTION:") == 0) {
                        info.ssid = l.substr(19);
                        info.ssid.erase(info.ssid.find_last_not_of(" \n\r\t") + 1);
                    } else if (l.find("IP4.ADDRESS") == 0 && info.ip_addr.empty()) {
                        size_t colon = l.find(':');
                        if (colon != std::string::npos) {
                            info.ip_addr = l.substr(colon + 1);
                            info.ip_addr.erase(info.ip_addr.find_last_not_of(" \n\r\t") + 1);
                        }
                    } else if (l.find("IP4.GATEWAY:") == 0 && info.gateway.empty()) {
                        info.gateway = l.substr(12);
                        info.gateway.erase(info.gateway.find_last_not_of(" \n\r\t") + 1);
                    }
                }
                pclose(fp);
            }

            // 3. Wi-Fi details (Signal, Channel, Frequency)
            if (info.type == "wifi") {
                fp = popen("nmcli -t -f IN-USE,SSID,SIGNAL,CHAN,FREQ dev wifi list 2>/dev/null | grep '^\\*'", "r");
                if (fp) {
                    char buf[256];
                    if (fgets(buf, sizeof(buf), fp)) {
                        std::string l(buf);
                        std::stringstream ss(l);
                        std::string in_use, ssid, sig_str, chan_str, freq_str;
                        std::getline(ss, in_use, ':');
                        std::getline(ss, ssid, ':');
                        std::getline(ss, sig_str, ':');
                        std::getline(ss, chan_str, ':');
                        std::getline(ss, freq_str, ':');

                        if (!ssid.empty() && info.ssid.empty()) {
                            info.ssid = ssid;
                        }
                        if (!sig_str.empty()) {
                            try { info.signal_strength = std::stoi(sig_str); } catch (...) {}
                        }
                        info.channel = chan_str;
                        if (!freq_str.empty()) {
                            try {
                                int f = std::stoi(freq_str);
                                info.freq_band = (f > 4000) ? "5 GHz" : "2.4 GHz";
                            } catch (...) {
                                info.freq_band = freq_str;
                            }
                        }
                    }
                    pclose(fp);
                }
            }
        }

        // 4. Check Wi-Fi radio status
        bool wifi_enabled = true;
        FILE* fp_radio = popen("nmcli radio wifi 2>/dev/null", "r");
        if (fp_radio) {
            char buf[32];
            if (fgets(buf, sizeof(buf), fp_radio)) {
                std::string r(buf);
                if (r.find("disabled") != std::string::npos) {
                    wifi_enabled = false;
                }
            }
            pclose(fp_radio);
        }

        struct FetchData {
            ActiveNetInfo info;
            bool wifi_enabled;
        };
        auto* data = new FetchData{info, wifi_enabled};

        g_idle_add([](gpointer user_data) -> gboolean {
            auto* d = static_cast<FetchData*>(user_data);
            current_info = d->info;

            if (wifi_switch) {
                gtk_switch_set_active(GTK_SWITCH(wifi_switch), d->wifi_enabled);
            }

            update_active_connection_ui();
            delete d;
            return G_SOURCE_REMOVE;
        }, data);
    }).detach();
}

void WifiManager::update_active_connection_ui() {
    if (!active_box) return;

    GList* children = gtk_container_get_children(GTK_CONTAINER(active_box));
    for (GList* l = children; l != nullptr; l = l->next) {
        gtk_widget_destroy(GTK_WIDGET(l->data));
    }
    g_list_free(children);

    if (current_info.is_connected && !current_info.ssid.empty() && current_info.ssid != "--") {
        GtkWidget* top_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);

        GtkWidget* dot_icon = gtk_label_new("●");
        gtk_widget_add_css_class(dot_icon, current_info.type == "ethernet" ? "active-connected-eth-dot" : "active-connected-dot");

        GtkWidget* ssid_lbl = gtk_label_new(current_info.ssid.c_str());
        gtk_widget_add_css_class(ssid_lbl, "active-ssid-title");

        std::string badge_text = (current_info.type == "ethernet") ? "Wired Ethernet" : current_info.freq_band;
        if (current_info.type == "wifi" && !current_info.channel.empty()) {
            badge_text += " • Ch " + current_info.channel;
        }
        GtkWidget* band_badge = gtk_label_new(badge_text.empty() ? "Connected" : badge_text.c_str());
        gtk_widget_add_css_class(band_badge, "active-band-badge");

        gtk_box_pack_start(GTK_BOX(top_row), dot_icon, FALSE, FALSE, 0);
        gtk_box_pack_start(GTK_BOX(top_row), ssid_lbl, FALSE, FALSE, 0);
        gtk_box_pack_end(GTK_BOX(top_row), band_badge, FALSE, FALSE, 0);

        // Details sub-grid
        GtkWidget* details_grid = gtk_grid_new();
        gtk_grid_set_column_spacing(GTK_GRID(details_grid), 16);
        gtk_grid_set_row_spacing(GTK_GRID(details_grid), 4);
        gtk_widget_add_css_class(details_grid, "active-details-grid");

        std::string ip_text = "󰩟 " + (current_info.ip_addr.empty() ? "--" : current_info.ip_addr);
        GtkWidget* ip_lbl = gtk_label_new(ip_text.c_str());
        gtk_widget_add_css_class(ip_lbl, "active-detail-item");

        std::string gw_text = "󰌘 " + (current_info.gateway.empty() ? "--" : current_info.gateway);
        GtkWidget* gw_lbl = gtk_label_new(gw_text.c_str());
        gtk_widget_add_css_class(gw_lbl, "active-detail-item");

        std::string sig_text;
        if (current_info.type == "ethernet") {
            sig_text = "󰈀 " + current_info.iface;
        } else {
            sig_text = " " + std::to_string(current_info.signal_strength) + "%";
        }
        GtkWidget* sig_lbl = gtk_label_new(sig_text.c_str());
        gtk_widget_add_css_class(sig_lbl, "active-detail-item");

        gtk_grid_attach(GTK_GRID(details_grid), ip_lbl, 0, 0, 1, 1);
        gtk_grid_attach(GTK_GRID(details_grid), gw_lbl, 1, 0, 1, 1);
        gtk_grid_attach(GTK_GRID(details_grid), sig_lbl, 2, 0, 1, 1);

        // Action Buttons Row
        GtkWidget* actions_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
        gtk_widget_add_css_class(actions_row, "active-actions-row");

        if (current_info.type == "wifi") {
            GtkWidget* qr_btn = gtk_button_new_with_label("󰌆 Share Pass & QR");
            gtk_widget_add_css_class(qr_btn, "btn-active-action");
            g_signal_connect(qr_btn, "clicked", G_CALLBACK(+[](GtkButton*, gpointer) {
                WifiManager::share_wifi_qr();
            }), nullptr);
            gtk_box_pack_start(GTK_BOX(actions_row), qr_btn, FALSE, FALSE, 0);
        }

        GtkWidget* disc_btn = gtk_button_new_with_label(" Disconnect");
        gtk_widget_add_css_class(disc_btn, "btn-active-disconnect");
        g_signal_connect(disc_btn, "clicked", G_CALLBACK(+[](GtkButton*, gpointer) {
            WifiManager::disconnect_active();
        }), nullptr);

        gtk_box_pack_end(GTK_BOX(actions_row), disc_btn, FALSE, FALSE, 0);

        gtk_box_pack_start(GTK_BOX(active_box), top_row, FALSE, FALSE, 0);
        gtk_box_pack_start(GTK_BOX(active_box), details_grid, FALSE, FALSE, 0);
        gtk_box_pack_start(GTK_BOX(active_box), actions_row, FALSE, FALSE, 0);
    } else {
        GtkWidget* dis_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
        GtkWidget* dis_icon = gtk_label_new("󰖪");
        gtk_widget_add_css_class(dis_icon, "active-disconnected-icon");

        GtkWidget* dis_lbl = gtk_label_new("Not Connected to any Network");
        gtk_widget_add_css_class(dis_lbl, "active-disconnected-title");

        gtk_box_pack_start(GTK_BOX(dis_row), dis_icon, FALSE, FALSE, 0);
        gtk_box_pack_start(GTK_BOX(dis_row), dis_lbl, FALSE, FALSE, 0);
        gtk_box_pack_start(GTK_BOX(active_box), dis_row, FALSE, FALSE, 0);
    }

    gtk_widget_show_all(active_box);
}

void WifiManager::scan_networks(bool force_rescan) {
    if (scan_spinner) {
        gtk_spinner_start(GTK_SPINNER(scan_spinner));
        gtk_widget_show(scan_spinner);
    }

    std::thread([force_rescan]() {
        if (force_rescan) {
            system("nmcli dev wifi rescan 2>/dev/null");
        }

        // 1. Fetch saved connection names
        std::unordered_set<std::string> saved_ssids;
        FILE* fp_saved = popen("nmcli -t -f NAME,TYPE connection show 2>/dev/null | grep ':802-11-wireless' | cut -d: -f1", "r");
        if (fp_saved) {
            char buf[256];
            while (fgets(buf, sizeof(buf), fp_saved)) {
                std::string s(buf);
                s.erase(s.find_last_not_of(" \n\r\t") + 1);
                if (!s.empty()) saved_ssids.insert(s);
            }
            pclose(fp_saved);
        }

        // 2. Query nmcli for Wi-Fi access points
        std::array<char, 512> buffer;
        std::string result;
        FILE* fp = popen("nmcli -t -f IN-USE,BSSID,SSID,MODE,CHAN,FREQ,RATE,SIGNAL,SECURITY device wifi list 2>/dev/null", "r");
        if (fp) {
            while (fgets(buffer.data(), buffer.size(), fp) != nullptr) {
                result += buffer.data();
            }
            pclose(fp);
        }

        std::map<std::string, WifiNetwork> unique_nets;
        std::istringstream stream(result);
        std::string line;

        while (std::getline(stream, line)) {
            if (line.empty()) continue;

            std::vector<std::string> parts;
            std::string cur;
            bool escaped = false;
            for (char c : line) {
                if (escaped) {
                    cur += c;
                    escaped = false;
                } else if (c == '\\') {
                    escaped = true;
                } else if (c == ':') {
                    parts.push_back(cur);
                    cur.clear();
                } else {
                    cur += c;
                }
            }
            parts.push_back(cur);

            if (parts.size() >= 8) {
                std::string in_use = parts[0];
                std::string bssid = parts[1];
                std::string ssid = parts[2];
                std::string chan = parts[4];
                std::string freq = parts[5];
                std::string sig_str = parts[7];
                std::string sec = (parts.size() >= 9) ? parts[8] : "";

                if (ssid.empty() || ssid == "--") continue;

                int sig = 50;
                try { sig = std::stoi(sig_str); } catch (...) {}

                bool is_conn = (in_use == "*");
                bool is_sec = (!sec.empty() && sec != "--");
                bool is_saved = (saved_ssids.find(ssid) != saved_ssids.end());

                if (unique_nets.find(ssid) == unique_nets.end() || unique_nets[ssid].signal_strength < sig) {
                    unique_nets[ssid] = {ssid, bssid, is_conn, is_saved, sig, chan, freq, sec, is_sec};
                }
            }
        }

        auto* nets = new std::vector<WifiNetwork>();
        for (const auto& [_, net] : unique_nets) {
            nets->push_back(net);
        }

        std::sort(nets->begin(), nets->end(), [](const WifiNetwork& a, const WifiNetwork& b) {
            if (a.is_connected != b.is_connected) return a.is_connected > b.is_connected;
            return a.signal_strength > b.signal_strength;
        });

        g_idle_add([](gpointer data) -> gboolean {
            auto* parsed_nets = static_cast<std::vector<WifiNetwork>*>(data);
            networks = *parsed_nets;
            delete parsed_nets;

            if (scan_spinner) {
                gtk_spinner_stop(GTK_SPINNER(scan_spinner));
                gtk_widget_hide(scan_spinner);
            }

            if (count_lbl) {
                std::string ctxt = std::to_string(networks.size()) + " found";
                gtk_label_set_text(GTK_LABEL(count_lbl), ctxt.c_str());
            }

            if (status_msg_lbl && gtk_widget_get_visible(status_msg_lbl)) {
                g_timeout_add(1500, [](gpointer) -> gboolean {
                    if (status_msg_lbl) gtk_widget_hide(status_msg_lbl);
                    return G_SOURCE_REMOVE;
                }, nullptr);
            }

            update_networks_list_ui();
            return G_SOURCE_REMOVE;
        }, nets);
    }).detach();
}

void WifiManager::update_networks_list_ui() {
    if (!listbox) return;

    GList* children = gtk_container_get_children(GTK_CONTAINER(listbox));
    for (GList* l = children; l != nullptr; l = l->next) {
        gtk_widget_destroy(GTK_WIDGET(l->data));
    }
    g_list_free(children);

    for (const auto& net : networks) {
        // Container holding the network row and its inline expandable password form below
        GtkWidget* item_container = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
        gtk_widget_add_css_class(item_container, "wifi-item-container");

        // Top Row inside an EventBox for full card clickability
        GtkWidget* row_event_box = gtk_event_box_new();
        gtk_event_box_set_visible_window(GTK_EVENT_BOX(row_event_box), FALSE);

        GtkWidget* row_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
        gtk_widget_add_css_class(row_box, "wifi-item-row");

        // Signal icon with level
        const char* sig_icon = net.signal_strength > 75 ? "󰤨" : (net.signal_strength > 50 ? "󰤥" : (net.signal_strength > 25 ? "󰤢" : "󰤟"));
        GtkWidget* icon = gtk_label_new(sig_icon);
        gtk_widget_add_css_class(icon, "wifi-signal-icon");

        GtkWidget* name = gtk_label_new(net.ssid.c_str());
        gtk_widget_add_css_class(name, "wifi-name-lbl");
        gtk_label_set_ellipsize(GTK_LABEL(name), PANGO_ELLIPSIZE_END);
        gtk_label_set_max_width_chars(GTK_LABEL(name), 22);
        gtk_widget_set_halign(name, GTK_ALIGN_START);

        gtk_box_pack_start(GTK_BOX(row_box), icon, FALSE, FALSE, 0);
        gtk_box_pack_start(GTK_BOX(row_box), name, FALSE, FALSE, 0);

        if (net.is_secure) {
            GtkWidget* lock = gtk_label_new("");
            gtk_widget_add_css_class(lock, "wifi-lock-badge");
            gtk_box_pack_start(GTK_BOX(row_box), lock, FALSE, FALSE, 4);
        }

        // Frequency band hint badge (5G / 2.4G)
        if (!net.frequency.empty()) {
            std::string fhint = "2.4G";
            try {
                int f = std::stoi(net.frequency);
                if (f > 4000) fhint = "5G";
            } catch (...) {}
            GtkWidget* freq_badge = gtk_label_new(fhint.c_str());
            gtk_widget_add_css_class(freq_badge, "wifi-freq-badge");
            gtk_box_pack_start(GTK_BOX(row_box), freq_badge, FALSE, FALSE, 2);
        }

        // Inline Expandable Form placed DIRECTLY BELOW this row
        GtkWidget* inline_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
        gtk_widget_add_css_class(inline_box, "wifi-inline-connect-box");

        std::string title_text = "󰌆  Enter Password for " + net.ssid + ":";
        GtkWidget* inline_title = gtk_label_new(title_text.c_str());
        gtk_widget_add_css_class(inline_title, "wifi-inline-title");
        gtk_widget_set_halign(inline_title, GTK_ALIGN_START);
        gtk_box_pack_start(GTK_BOX(inline_box), inline_title, FALSE, FALSE, 0);

        GtkWidget* entry_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
        GtkWidget* entry = gtk_entry_new();
        gtk_entry_set_visibility(GTK_ENTRY(entry), FALSE);
        gtk_entry_set_placeholder_text(GTK_ENTRY(entry), "Passphrase...");
        gtk_widget_add_css_class(entry, "wifi-inline-entry");
        gtk_widget_set_hexpand(entry, TRUE);

        GtkWidget* eye_btn = gtk_button_new_with_label("󰈈");
        gtk_widget_add_css_class(eye_btn, "btn-inline-eye");
        g_signal_connect(eye_btn, "clicked", G_CALLBACK(+[](GtkButton* btn, gpointer data) {
            GtkEntry* e = GTK_ENTRY(data);
            gboolean vis = gtk_entry_get_visibility(e);
            gtk_entry_set_visibility(e, !vis);
            gtk_button_set_label(btn, !vis ? "󰈉" : "󰈈");
        }), entry);

        GtkWidget* conn_btn = gtk_button_new_with_label("Connect");
        gtk_widget_add_css_class(conn_btn, "btn-inline-connect");

        GtkWidget* cancel_btn = gtk_button_new_with_label(" Cancel");
        gtk_widget_add_css_class(cancel_btn, "btn-inline-cancel");

        gtk_box_pack_start(GTK_BOX(entry_row), entry, TRUE, TRUE, 0);
        gtk_box_pack_start(GTK_BOX(entry_row), eye_btn, FALSE, FALSE, 0);
        gtk_box_pack_start(GTK_BOX(entry_row), conn_btn, FALSE, FALSE, 0);
        gtk_box_pack_start(GTK_BOX(entry_row), cancel_btn, FALSE, FALSE, 0);
        gtk_box_pack_start(GTK_BOX(inline_box), entry_row, FALSE, FALSE, 0);

        GtkWidget* inline_status_lbl = gtk_label_new("");
        gtk_widget_add_css_class(inline_status_lbl, "wifi-inline-status-lbl");
        gtk_widget_set_halign(inline_status_lbl, GTK_ALIGN_START);
        gtk_widget_set_no_show_all(inline_status_lbl, TRUE);
        gtk_box_pack_start(GTK_BOX(inline_box), inline_status_lbl, FALSE, FALSE, 0);

        // Cancel Button Action -> collapse
        using WidgetsPair = std::pair<GtkWidget*, GtkWidget*>;
        auto* wp = new WidgetsPair(inline_box, item_container);
        g_signal_connect(cancel_btn, "clicked", G_CALLBACK(+[](GtkButton*, gpointer data) {
            auto* p = static_cast<WidgetsPair*>(data);
            gtk_widget_hide(p->first);
            gtk_widget_remove_css_class(p->second, "expanded");
        }), wp);

        // Submit Handler for Password
        struct InlineConnectData {
            std::string ssid;
            GtkWidget* entry;
            GtkWidget* status_lbl;
            GtkWidget* conn_btn;
            GtkWidget* inline_box;
            GtkWidget* item_container;
        };

        auto do_inline_connect = +[](gpointer user_data) {
            auto* d = static_cast<InlineConnectData*>(user_data);
            const char* pass_text = gtk_entry_get_text(GTK_ENTRY(d->entry));
            std::string pass = pass_text ? pass_text : "";

            gtk_label_set_text(GTK_LABEL(d->status_lbl), "Connecting to network...");
            gtk_widget_remove_css_class(d->status_lbl, "error");
            gtk_widget_add_css_class(d->status_lbl, "info");
            gtk_widget_show(d->status_lbl);
            gtk_widget_set_sensitive(d->conn_btn, FALSE);

            std::string target_ssid = d->ssid;
            GtkWidget* stat_lbl = d->status_lbl;
            GtkWidget* cbtn = d->conn_btn;
            GtkWidget* ibox = d->inline_box;
            GtkWidget* icontainer = d->item_container;

            WifiManager::connect_to_with_feedback(target_ssid, pass, [stat_lbl, cbtn, ibox, icontainer](bool success, const std::string& err_msg) {
                gtk_widget_set_sensitive(cbtn, TRUE);
                if (success) {
                    gtk_widget_hide(ibox);
                    gtk_widget_remove_css_class(icontainer, "expanded");
                } else {
                    gtk_widget_remove_css_class(stat_lbl, "info");
                    gtk_widget_add_css_class(stat_lbl, "error");
                    std::string err = "Failed: ";
                    if (err_msg.find("Secrets were required") != std::string::npos ||
                        err_msg.find("password") != std::string::npos) {
                        err += "Incorrect password";
                    } else if (err_msg.size() > 36) {
                        err += err_msg.substr(0, 34) + "..";
                    } else {
                        err += err_msg;
                    }
                    gtk_label_set_text(GTK_LABEL(stat_lbl), err.c_str());
                    gtk_widget_show(stat_lbl);
                }
            });
        };

        auto* cdata = new InlineConnectData{net.ssid, entry, inline_status_lbl, conn_btn, inline_box, item_container};

        g_signal_connect_swapped(conn_btn, "clicked", G_CALLBACK(do_inline_connect), cdata);
        g_signal_connect_swapped(entry, "activate", G_CALLBACK(do_inline_connect), cdata);

        // Buttons on the right of row_box
        if (net.is_connected) {
            gtk_widget_add_css_class(item_container, "connected");
            GtkWidget* conn_badge = gtk_label_new("Connected");
            gtk_widget_add_css_class(conn_badge, "wifi-connected-pill");
            gtk_box_pack_end(GTK_BOX(row_box), conn_badge, FALSE, FALSE, 0);
        } else {
            GtkWidget* btn_container = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);

            GtkWidget* connect_btn = gtk_button_new_with_label(net.is_saved ? "󰤨 Connect" : "Connect");
            gtk_widget_add_css_class(connect_btn, "btn-wifi-connect");

            struct ActionBtnData {
                std::string ssid;
                bool is_sec;
                bool is_saved;
                GtkWidget* inline_box;
                GtkWidget* item_container;
                GtkWidget* entry;
            };

            auto* abdata = new ActionBtnData{net.ssid, net.is_secure, net.is_saved, inline_box, item_container, entry};

            g_signal_connect(connect_btn, "clicked", G_CALLBACK(+[](GtkButton*, gpointer user_data) {
                auto* ad = static_cast<ActionBtnData*>(user_data);
                if (ad->is_saved) {
                    WifiManager::connect_to(ad->ssid, "");
                } else if (ad->is_sec) {
                    gboolean is_vis = gtk_widget_get_visible(ad->inline_box);
                    if (is_vis) {
                        gtk_widget_hide(ad->inline_box);
                        gtk_widget_remove_css_class(ad->item_container, "expanded");
                    } else {
                        if (listbox) {
                            GList* rows = gtk_container_get_children(GTK_CONTAINER(listbox));
                            for (GList* l = rows; l != nullptr; l = l->next) {
                                GtkWidget* row_w = GTK_WIDGET(l->data);
                                GtkWidget* item = GTK_IS_BIN(row_w) ? gtk_bin_get_child(GTK_BIN(row_w)) : row_w;
                                if (item && GTK_IS_BOX(item)) {
                                    GList* inner = gtk_container_get_children(GTK_CONTAINER(item));
                                    for (GList* in = inner; in != nullptr; in = in->next) {
                                        GtkWidget* w = GTK_WIDGET(in->data);
                                        if (gtk_widget_has_css_class(w, "wifi-inline-connect-box")) {
                                            gtk_widget_hide(w);
                                        }
                                    }
                                    g_list_free(inner);
                                    gtk_widget_remove_css_class(item, "expanded");
                                }
                            }
                            g_list_free(rows);
                        }

                        gtk_widget_show_all(ad->inline_box);
                        gtk_widget_add_css_class(ad->item_container, "expanded");
                        gtk_widget_grab_focus(ad->entry);
                    }
                } else {
                    WifiManager::connect_to(ad->ssid, "");
                }
            }), abdata);

            g_signal_connect(row_event_box, "button-press-event", G_CALLBACK(+[](GtkWidget*, GdkEventButton* event, gpointer user_data) -> gboolean {
                if (event->type == GDK_BUTTON_PRESS && event->button == 1) {
                    auto* ad = static_cast<ActionBtnData*>(user_data);
                    if (ad->is_saved) {
                        WifiManager::connect_to(ad->ssid, "");
                        return TRUE;
                    } else if (ad->is_sec) {
                        gboolean is_vis = gtk_widget_get_visible(ad->inline_box);
                        if (is_vis) {
                            gtk_widget_hide(ad->inline_box);
                            gtk_widget_remove_css_class(ad->item_container, "expanded");
                        } else {
                            if (listbox) {
                                GList* rows = gtk_container_get_children(GTK_CONTAINER(listbox));
                                for (GList* l = rows; l != nullptr; l = l->next) {
                                    GtkWidget* row_w = GTK_WIDGET(l->data);
                                    GtkWidget* item = GTK_IS_BIN(row_w) ? gtk_bin_get_child(GTK_BIN(row_w)) : row_w;
                                    if (item && GTK_IS_BOX(item)) {
                                        GList* inner = gtk_container_get_children(GTK_CONTAINER(item));
                                        for (GList* in = inner; in != nullptr; in = in->next) {
                                            GtkWidget* w = GTK_WIDGET(in->data);
                                            if (gtk_widget_has_css_class(w, "wifi-inline-connect-box")) {
                                                gtk_widget_hide(w);
                                            }
                                        }
                                        g_list_free(inner);
                                        gtk_widget_remove_css_class(item, "expanded");
                                    }
                                }
                                g_list_free(rows);
                            }

                            gtk_widget_show_all(ad->inline_box);
                            gtk_widget_add_css_class(ad->item_container, "expanded");
                            gtk_widget_grab_focus(ad->entry);
                        }
                        return TRUE;
                    } else {
                        WifiManager::connect_to(ad->ssid, "");
                        return TRUE;
                    }
                }
                return FALSE;
            }), abdata);

            gtk_box_pack_start(GTK_BOX(btn_container), connect_btn, FALSE, FALSE, 0);

            if (net.is_saved) {
                GtkWidget* forget_btn = gtk_button_new_with_label("󰆴");
                gtk_widget_add_css_class(forget_btn, "btn-wifi-forget");
                gtk_widget_set_tooltip_text(forget_btn, "Forget Saved Network");
                g_object_set_data_full(G_OBJECT(forget_btn), "ssid", strdup(net.ssid.c_str()), free);
                g_signal_connect(forget_btn, "clicked", G_CALLBACK(+[](GtkButton* btn, gpointer) {
                    const char* ssid = static_cast<const char*>(g_object_get_data(G_OBJECT(btn), "ssid"));
                    if (ssid) {
                        WifiManager::forget_network(ssid);
                    }
                }), nullptr);
                gtk_box_pack_start(GTK_BOX(btn_container), forget_btn, FALSE, FALSE, 0);
            }

            gtk_box_pack_end(GTK_BOX(row_box), btn_container, FALSE, FALSE, 0);
        }

        gtk_container_add(GTK_CONTAINER(row_event_box), row_box);
        gtk_box_pack_start(GTK_BOX(item_container), row_event_box, FALSE, FALSE, 0);
        gtk_box_pack_start(GTK_BOX(item_container), inline_box, FALSE, FALSE, 0);

        gtk_container_add(GTK_CONTAINER(listbox), item_container);
    }

    gtk_widget_show_all(listbox);

    // Hide all inline_box widgets by default (open only on user click/interaction)
    GList* rows = gtk_container_get_children(GTK_CONTAINER(listbox));
    for (GList* l = rows; l != nullptr; l = l->next) {
        GtkWidget* row_w = GTK_WIDGET(l->data);
        GtkWidget* item = GTK_IS_BIN(row_w) ? gtk_bin_get_child(GTK_BIN(row_w)) : row_w;
        if (item && GTK_IS_BOX(item)) {
            GList* children = gtk_container_get_children(GTK_CONTAINER(item));
            for (GList* c = children; c != nullptr; c = c->next) {
                GtkWidget* w = GTK_WIDGET(c->data);
                if (gtk_widget_has_css_class(w, "wifi-inline-connect-box")) {
                    gtk_widget_hide(w);
                }
            }
            g_list_free(children);
        }
    }
    g_list_free(rows);
}

void WifiManager::connect_to(const std::string& ssid, const std::string& password) {
    connect_to_with_feedback(ssid, password, nullptr);
}

void WifiManager::connect_to_with_feedback(const std::string& ssid, const std::string& password, std::function<void(bool, const std::string&)> on_complete) {
    if (ssid.empty()) return;

    if (status_msg_lbl) {
        std::string msg = "Connecting to " + ssid + "...";
        gtk_label_set_text(GTK_LABEL(status_msg_lbl), msg.c_str());
        gtk_widget_show(status_msg_lbl);
    }

    std::thread([ssid, password, on_complete]() {
        std::string cmd;
        if (!password.empty()) {
            cmd = "nmcli device wifi connect \"" + ssid + "\" password \"" + password + "\" 2>&1";
        } else {
            cmd = "nmcli device wifi connect \"" + ssid + "\" 2>&1";
        }

        FILE* fp = popen(cmd.c_str(), "r");
        std::string output;
        int exit_code = 1;
        if (fp) {
            char buf[256];
            while (fgets(buf, sizeof(buf), fp)) {
                output += buf;
            }
            exit_code = pclose(fp);
        }

        struct ConnResult {
            bool success;
            std::string msg;
            std::string ssid;
            std::function<void(bool, const std::string&)> callback;
        };
        auto* res = new ConnResult{exit_code == 0, output, ssid, on_complete};

        g_idle_add([](gpointer data) -> gboolean {
            auto* r = static_cast<ConnResult*>(data);
            if (r->callback) {
                r->callback(r->success, r->msg);
            }

            if (status_msg_lbl) {
                if (r->success) {
                    std::string smsg = "󰄬 Successfully connected to " + r->ssid;
                    gtk_label_set_text(GTK_LABEL(status_msg_lbl), smsg.c_str());
                } else {
                    std::string err = "Failed: ";
                    if (r->msg.find("Secrets were required") != std::string::npos ||
                        r->msg.find("password") != std::string::npos) {
                        err += "Incorrect password";
                    } else if (r->msg.size() > 40) {
                        err += r->msg.substr(0, 38) + "..";
                    } else {
                        err += r->msg;
                    }
                    gtk_label_set_text(GTK_LABEL(status_msg_lbl), err.c_str());
                }
                gtk_widget_show(status_msg_lbl);
            }

            WifiManager::fetch_active_connection();
            WifiManager::scan_networks(false);

            delete r;
            return G_SOURCE_REMOVE;
        }, res);
    }).detach();
}

void WifiManager::forget_network(const std::string& ssid) {
    if (ssid.empty()) return;

    if (status_msg_lbl) {
        std::string msg = "Forgetting " + ssid + "...";
        gtk_label_set_text(GTK_LABEL(status_msg_lbl), msg.c_str());
        gtk_widget_show(status_msg_lbl);
    }

    std::thread([ssid]() {
        std::string cmd = "nmcli connection delete id \"" + ssid + "\" 2>/dev/null";
        system(cmd.c_str());

        g_idle_add([](gpointer) -> gboolean {
            WifiManager::scan_networks(false);
            return G_SOURCE_REMOVE;
        }, nullptr);
    }).detach();
}

void WifiManager::disconnect_active() {
    if (current_info.iface.empty()) return;

    if (status_msg_lbl) {
        gtk_label_set_text(GTK_LABEL(status_msg_lbl), "Disconnecting active network...");
        gtk_widget_show(status_msg_lbl);
    }

    std::thread([]() {
        std::string iface = current_info.iface;
        std::string cmd = "nmcli device disconnect " + iface + " 2>/dev/null";
        system(cmd.c_str());

        g_idle_add([](gpointer) -> gboolean {
            WifiManager::fetch_active_connection();
            WifiManager::scan_networks(false);
            return G_SOURCE_REMOVE;
        }, nullptr);
    }).detach();
}

void WifiManager::toggle_wifi_radio(bool enable) {
    if (status_msg_lbl) {
        std::string msg = enable ? "Enabling Wi-Fi..." : "Disabling Wi-Fi...";
        gtk_label_set_text(GTK_LABEL(status_msg_lbl), msg.c_str());
        gtk_widget_show(status_msg_lbl);
    }

    std::thread([enable]() {
        std::string cmd = enable ? "nmcli radio wifi on" : "nmcli radio wifi off";
        system(cmd.c_str());

        g_idle_add([](gpointer) -> gboolean {
            WifiManager::fetch_active_connection();
            WifiManager::scan_networks(false);
            return G_SOURCE_REMOVE;
        }, nullptr);
    }).detach();
}

void WifiManager::share_wifi_qr() {
    if (current_info.iface.empty() || current_info.ssid.empty()) return;

    if (list_container) gtk_widget_hide(list_container);
    if (qr_ssid_lbl) {
        std::string title = "󰤨  " + current_info.ssid;
        gtk_label_set_text(GTK_LABEL(qr_ssid_lbl), title.c_str());
    }
    if (qr_pass_lbl) {
        gtk_label_set_text(GTK_LABEL(qr_pass_lbl), "Reading password...");
    }
    if (qr_copy_btn) {
        gtk_button_set_label(GTK_BUTTON(qr_copy_btn), "󰅍 Copy");
    }
    if (qr_view_box) {
        gtk_widget_show_all(qr_view_box);
    }

    std::string iface = current_info.iface;
    std::string ssid = current_info.ssid;

    std::thread([iface, ssid]() {
        std::string uuid_cmd = "nmcli --get-values GENERAL.CON-UUID device show " + iface + " 2>/dev/null | head -n 1";
        FILE* fp = popen(uuid_cmd.c_str(), "r");
        std::string uuid;
        if (fp) {
            char buf[128];
            if (fgets(buf, sizeof(buf), fp)) {
                uuid = buf;
                uuid.erase(uuid.find_last_not_of(" \n\r\t") + 1);
            }
            pclose(fp);
        }

        std::string pass;
        if (!uuid.empty()) {
            std::string pass_cmd = "nmcli --show-secrets --escape no --get-values 802-11-wireless-security.psk connection show uuid \"" + uuid + "\" 2>/dev/null";
            fp = popen(pass_cmd.c_str(), "r");
            if (fp) {
                char buf[256];
                if (fgets(buf, sizeof(buf), fp)) {
                    pass = buf;
                    pass.erase(pass.find_last_not_of(" \n\r\t") + 1);
                }
                pclose(fp);
            }
        }

        std::string qr_payload = "WIFI:T:WPA;S:" + ssid + ";P:" + pass + ";;";
        std::string qr_cmd = "qrencode -s 6 -m 2 -o /tmp/wifi_qr.png \"" + qr_payload + "\" 2>/dev/null";
        system(qr_cmd.c_str());

        if (!pass.empty()) {
            std::string copy_cmd = "printf '%s' '" + pass + "' | wl-copy 2>/dev/null";
            system(copy_cmd.c_str());
        }

        struct QRData {
            std::string pass;
        };
        auto* data = new QRData{pass};

        g_idle_add([](gpointer user_data) -> gboolean {
            auto* d = static_cast<QRData*>(user_data);
            current_wifi_password = d->pass;

            if (qr_image) {
                gtk_image_set_from_file(GTK_IMAGE(qr_image), "/tmp/wifi_qr.png");
            }
            if (qr_pass_lbl) {
                std::string ptext = d->pass.empty() ? "(No password stored)" : d->pass;
                gtk_label_set_text(GTK_LABEL(qr_pass_lbl), ptext.c_str());
            }
            if (qr_copy_btn && !d->pass.empty()) {
                gtk_button_set_label(GTK_BUTTON(qr_copy_btn), "󰄬 Copied!");
            }

            delete d;
            return G_SOURCE_REMOVE;
        }, data);
    }).detach();
}

void WifiManager::run_ping_test() {
    if (ping_lbl) {
        gtk_label_set_text(GTK_LABEL(ping_lbl), "Pinging 1.1.1.1...");
    }

    std::thread([]() {
        FILE* fp = popen("LC_ALL=C ping -n -c 1 -W 1 1.1.1.1 2>/dev/null | awk -F'time[=<]' '/time[=<]/ { split($2, parts, \" \"); print parts[1]; exit }'", "r");
        std::string ms = "--";
        if (fp) {
            char buf[64];
            if (fgets(buf, sizeof(buf), fp)) {
                ms = buf;
                ms.erase(ms.find_last_not_of(" \n\r\t") + 1);
            }
            pclose(fp);
        }

        auto* res = new std::string(ms);
        g_idle_add([](gpointer data) -> gboolean {
            auto* str = static_cast<std::string*>(data);
            if (ping_lbl) {
                if (*str == "--" || str->empty()) {
                    gtk_label_set_text(GTK_LABEL(ping_lbl), "Ping: Offline");
                } else {
                    std::string txt = "Ping: " + *str + " ms";
                    gtk_label_set_text(GTK_LABEL(ping_lbl), txt.c_str());
                }
            }
            delete str;
            return G_SOURCE_REMOVE;
        }, res);
    }).detach();
}

bool WifiManager::is_vpn_active() {
    FILE* fp = popen("nmcli -t -f TYPE,STATE con show --active 2>/dev/null | grep -E '^(vpn|wireguard|tun):activated' || ip link show proton0 2>/dev/null || ip link show type wireguard 2>/dev/null", "r");
    if (!fp) return false;
    char buf[128];
    bool active = (fgets(buf, sizeof(buf), fp) != nullptr && buf[0] != '\0');
    pclose(fp);
    return active;
}

void WifiManager::toggle_vpn() {
    if (is_vpn_active()) {
        if (status_msg_lbl) {
            gtk_label_set_text(GTK_LABEL(status_msg_lbl), "Disconnecting VPN...");
            gtk_widget_show(status_msg_lbl);
        }
        std::thread([]() {
            system("for c in $(nmcli -t -f NAME,TYPE con show --active | grep -E ':vpn|:wireguard' | cut -d: -f1); do nmcli con down \"$c\" 2>/dev/null; done; protonvpn-app --disconnect 2>/dev/null || true");
            g_idle_add([](gpointer) -> gboolean {
                refresh_privacy_tools_ui();
                if (status_msg_lbl) gtk_widget_hide(status_msg_lbl);
                return G_SOURCE_REMOVE;
            }, nullptr);
        }).detach();
    } else {
        if (status_msg_lbl) {
            gtk_label_set_text(GTK_LABEL(status_msg_lbl), "Connecting VPN Security...");
            gtk_widget_show(status_msg_lbl);
        }
        std::thread([]() {
            FILE* fp = popen("nmcli -t -f NAME,TYPE con show | grep -E ':vpn|:wireguard' | head -n 1 | cut -d: -f1", "r");
            std::string vpn_name;
            if (fp) {
                char buf[128];
                if (fgets(buf, sizeof(buf), fp)) {
                    vpn_name = buf;
                    while (!vpn_name.empty() && (vpn_name.back() == '\n' || vpn_name.back() == '\r')) vpn_name.pop_back();
                }
                pclose(fp);
            }
            if (!vpn_name.empty()) {
                std::string cmd = "nmcli con up \"" + vpn_name + "\" 2>/dev/null";
                system(cmd.c_str());
            } else {
                system("protonvpn-app 2>/dev/null &");
            }
            g_idle_add([](gpointer) -> gboolean {
                refresh_privacy_tools_ui();
                if (status_msg_lbl) gtk_widget_hide(status_msg_lbl);
                return G_SOURCE_REMOVE;
            }, nullptr);
        }).detach();
    }
}

bool WifiManager::is_hotspot_active() {
    FILE* fp = popen("nmcli -t -f TYPE,STATE con show --active 2>/dev/null | grep -i 'hotspot' || iw dev wlan0 info 2>/dev/null | grep -i 'type AP'", "r");
    if (!fp) return false;
    char buf[128];
    bool active = (fgets(buf, sizeof(buf), fp) != nullptr && buf[0] != '\0');
    pclose(fp);
    return active;
}

void WifiManager::toggle_hotspot() {
    // 1. Check if dnsmasq is available
    if (access("/usr/bin/dnsmasq", X_OK) != 0 && access("/usr/sbin/dnsmasq", X_OK) != 0) {
        if (status_msg_lbl) {
            gtk_label_set_text(GTK_LABEL(status_msg_lbl), "⚠ Hotspot requires dnsmasq (Run: sudo pacman -S dnsmasq)");
            gtk_widget_show(status_msg_lbl);
        }
        return;
    }

    if (is_hotspot_active()) {
        if (status_msg_lbl) {
            gtk_label_set_text(GTK_LABEL(status_msg_lbl), "Stopping Wi-Fi Hotspot...");
            gtk_widget_show(status_msg_lbl);
        }
        std::thread([]() {
            system("nmcli con down Hotspot 2>/dev/null || nmcli con down ZenithHotspot 2>/dev/null || nmcli con down Zenith-Hotspot 2>/dev/null || true");
            g_idle_add([](gpointer) -> gboolean {
                refresh_privacy_tools_ui();
                fetch_active_connection();
                if (status_msg_lbl) gtk_widget_hide(status_msg_lbl);
                return G_SOURCE_REMOVE;
            }, nullptr);
        }).detach();
    } else {
        if (status_msg_lbl) {
            gtk_label_set_text(GTK_LABEL(status_msg_lbl), "Starting 'Zenith-Hotspot' (Pass: zenith1234)...");
            gtk_widget_show(status_msg_lbl);
        }
        std::thread([]() {
            system("nmcli dev wifi hotspot ifname wlan0 ssid 'Zenith-Hotspot' password 'zenith1234' 2>/dev/null");
            g_idle_add([](gpointer) -> gboolean {
                refresh_privacy_tools_ui();
                fetch_active_connection();
                if (status_msg_lbl) gtk_widget_hide(status_msg_lbl);
                return G_SOURCE_REMOVE;
            }, nullptr);
        }).detach();
    }
}

void WifiManager::refresh_privacy_tools_ui() {
    if (vpn_sub_lbl && vpn_btn) {
        bool v = is_vpn_active();
        gtk_label_set_text(GTK_LABEL(vpn_sub_lbl), v ? "Protected" : "Disconnected");
        if (v) gtk_widget_add_css_class(vpn_btn, "active");
        else gtk_widget_remove_css_class(vpn_btn, "active");
    }
    if (hotspot_sub_lbl && hotspot_btn) {
        bool h = is_hotspot_active();
        gtk_label_set_text(GTK_LABEL(hotspot_sub_lbl), h ? "Sharing ON" : "Disabled");
        if (h) gtk_widget_add_css_class(hotspot_btn, "active");
        else gtk_widget_remove_css_class(hotspot_btn, "active");
    }
}

} // namespace zenith
