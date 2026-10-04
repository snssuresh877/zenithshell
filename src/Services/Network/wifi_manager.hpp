#pragma once

#include <gtk/gtk.h>
#include <string>
#include <vector>
#include <functional>

namespace zenith {

struct WifiNetwork {
    std::string ssid;
    std::string bssid;
    bool is_connected = false;
    bool is_saved = false;
    int signal_strength = 50;
    std::string channel;
    std::string frequency;
    std::string security;
    bool is_secure = false;
};

struct ActiveNetInfo {
    bool is_connected = false;
    std::string type = "wifi"; // "wifi", "ethernet", "none"
    std::string iface = "wlan0";
    std::string ssid = "";
    std::string ip_addr = "";
    std::string gateway = "";
    int signal_strength = 0;
    std::string channel = "";
    std::string freq_band = "";
    std::string ping_latency = "--";
};

class WifiManager {
public:
    static void init(GtkApplication* app = nullptr);
    static void toggle_panel();
    static void show_panel();
    static void hide_panel();
    static void scan_networks(bool force_rescan = false);
    static void connect_to(const std::string& ssid, const std::string& password = "");
    static void connect_to_with_feedback(const std::string& ssid, const std::string& password, std::function<void(bool, const std::string&)> on_complete);
    static void forget_network(const std::string& ssid);
    static void disconnect_active();
    static void toggle_wifi_radio(bool enable);
    static void share_wifi_qr();
    static void close_qr_view();
    static void run_ping_test();
    static void toggle_vpn();
    static void toggle_hotspot();
    static bool is_vpn_active();
    static bool is_hotspot_active();
    static void refresh_privacy_tools_ui();

private:
    static GtkWidget* window;
    static GtkWidget* main_card;
    static GtkWidget* list_container;
    static GtkWidget* listbox;
    static GtkWidget* active_box;
    static GtkWidget* wifi_switch;
    static GtkWidget* scan_spinner;
    static GtkWidget* status_msg_lbl;
    static GtkWidget* count_lbl;
    static GtkWidget* ping_lbl;
    static GtkWidget* vpn_btn;
    static GtkWidget* vpn_sub_lbl;
    static GtkWidget* hotspot_btn;
    static GtkWidget* hotspot_sub_lbl;
    static GtkWidget* qr_view_box;
    static GtkWidget* qr_image;
    static GtkWidget* qr_ssid_lbl;
    static GtkWidget* qr_pass_lbl;
    static GtkWidget* qr_copy_btn;
    static std::string pending_connect_ssid;
    static std::string current_wifi_password;
    static std::vector<WifiNetwork> networks;
    static ActiveNetInfo current_info;

    static void update_active_connection_ui();
    static void update_networks_list_ui();
    static void fetch_active_connection();
};

} // namespace zenith

