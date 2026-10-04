#pragma once

#include <string>
#include <cstdint>
#include <chrono>
#include <glib.h>

namespace zenith {

struct SysStats {
    double cpu_usage = 0.0;
    double ram_usage = 0.0;
    int battery_percent = 100;
    bool battery_charging = false;
    std::string net_speed_str = "0 B/s";
    bool vpn_connected = false;
    std::string vpn_name = "Proton VPN";
    bool net_connected = true;
    bool is_wifi = true;
    int wifi_quality = 75; // 0-100%
};

class SysMonitor {
public:
    static void init();
    static SysStats get_stats();
    static void update_tick();

private:
    static SysStats cached_stats;
    static unsigned long long prev_user, prev_nice, prev_system, prev_idle, prev_iowait, prev_irq, prev_softirq, prev_steal;
    static unsigned long long prev_rx_bytes, prev_tx_bytes;
    static std::chrono::steady_clock::time_point prev_net_time;

    static double read_cpu_usage();
    static double read_ram_usage();
    static int read_battery_percent(bool& is_charging);
    static std::string read_net_speed(bool& connected, bool& vpn_active, bool& is_wifi, int& wifi_quality);
};

} // namespace zenith
