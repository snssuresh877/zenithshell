#include "system/sys_monitor.hpp"
#include <fstream>
#include <sstream>
#include <iostream>
#include <unistd.h>
#include <dirent.h>
#include <cstdio>
#include <cstring>
#include <cmath>

namespace zenith {

SysStats SysMonitor::cached_stats{};
unsigned long long SysMonitor::prev_user = 0;
unsigned long long SysMonitor::prev_nice = 0;
unsigned long long SysMonitor::prev_system = 0;
unsigned long long SysMonitor::prev_idle = 0;
unsigned long long SysMonitor::prev_iowait = 0;
unsigned long long SysMonitor::prev_irq = 0;
unsigned long long SysMonitor::prev_softirq = 0;
unsigned long long SysMonitor::prev_steal = 0;
unsigned long long SysMonitor::prev_rx_bytes = 0;
unsigned long long SysMonitor::prev_tx_bytes = 0;
std::chrono::steady_clock::time_point SysMonitor::prev_net_time{};

void SysMonitor::init() {
    prev_net_time = std::chrono::steady_clock::now();
    update_tick();
    g_timeout_add_seconds(1, +[](gpointer) -> gboolean {
        update_tick();
        return TRUE;
    }, nullptr);
}

void SysMonitor::update_tick() {
    cached_stats.cpu_usage = read_cpu_usage();
    cached_stats.ram_usage = read_ram_usage();
    cached_stats.battery_percent = read_battery_percent(cached_stats.battery_charging);
    cached_stats.net_speed_str = read_net_speed(cached_stats.net_connected, cached_stats.vpn_connected, cached_stats.is_wifi, cached_stats.wifi_quality);
}

SysStats SysMonitor::get_stats() {
    return cached_stats;
}

double SysMonitor::read_cpu_usage() {
    std::ifstream file("/proc/stat");
    if (!file.is_open()) return 0.0;

    std::string line;
    std::getline(file, line);
    std::istringstream ss(line);

    std::string cpu_label;
    unsigned long long user, nice, system, idle, iowait, irq, softirq, steal;
    ss >> cpu_label >> user >> nice >> system >> idle >> iowait >> irq >> softirq >> steal;

    unsigned long long prev_idle_total = prev_idle + prev_iowait;
    unsigned long long idle_total = idle + iowait;

    unsigned long long prev_non_idle = prev_user + prev_nice + prev_system + prev_irq + prev_softirq + prev_steal;
    unsigned long long non_idle = user + nice + system + irq + softirq + steal;

    unsigned long long prev_total = prev_idle_total + prev_non_idle;
    unsigned long long total = idle_total + non_idle;

    unsigned long long total_diff = total - prev_total;
    unsigned long long idle_diff = idle_total - prev_idle_total;

    prev_user = user;
    prev_nice = nice;
    prev_system = system;
    prev_idle = idle;
    prev_iowait = iowait;
    prev_irq = irq;
    prev_softirq = softirq;
    prev_steal = steal;

    if (total_diff == 0) return 0.0;
    double cpu_perc = (static_cast<double>(total_diff - idle_diff) / total_diff) * 100.0;
    return std::clamp(cpu_perc, 0.0, 100.0);
}

double SysMonitor::read_ram_usage() {
    std::ifstream file("/proc/meminfo");
    if (!file.is_open()) return 0.0;

    std::string key;
    unsigned long long value;
    std::string unit;

    unsigned long long mem_total = 0;
    unsigned long long mem_available = 0;

    while (file >> key >> value >> unit) {
        if (key == "MemTotal:") mem_total = value;
        else if (key == "MemAvailable:") mem_available = value;
        if (mem_total > 0 && mem_available > 0) break;
    }

    if (mem_total == 0) return 0.0;
    double used_perc = (static_cast<double>(mem_total - mem_available) / mem_total) * 100.0;
    return std::clamp(used_perc, 0.0, 100.0);
}

int SysMonitor::read_battery_percent(bool& is_charging) {
    is_charging = false;
    std::string bat_path = "/sys/class/power_supply";

    DIR* dir = opendir(bat_path.c_str());
    if (!dir) return 100;

    struct dirent* entry;
    std::string bat_name = "";
    while ((entry = readdir(dir)) != nullptr) {
        std::string dname = entry->d_name;
        if (dname.rfind("BAT", 0) == 0) {
            bat_name = dname;
            break;
        }
    }
    closedir(dir);

    if (bat_name.empty()) return 100;

    std::string cap_file = bat_path + "/" + bat_name + "/capacity";
    std::string stat_file = bat_path + "/" + bat_name + "/status";

    int capacity = 100;
    std::ifstream c_in(cap_file);
    if (c_in.is_open()) c_in >> capacity;

    std::ifstream s_in(stat_file);
    if (s_in.is_open()) {
        std::string status;
        s_in >> status;
        if (status == "Charging" || status == "Full") {
            is_charging = true;
        }
    }

    return std::clamp(capacity, 0, 100);
}

std::string SysMonitor::read_net_speed(bool& connected, bool& vpn_active, bool& is_wifi, int& wifi_quality) {
    connected = false;
    vpn_active = false;
    is_wifi = false;
    wifi_quality = 75;

    std::ifstream file("/proc/net/dev");
    if (!file.is_open()) return "0 B/s";

    std::string line;
    // Skip first two header lines
    std::getline(file, line);
    std::getline(file, line);

    unsigned long long total_rx = 0;
    unsigned long long total_tx = 0;

    while (std::getline(file, line)) {
        size_t colon = line.find(':');
        if (colon == std::string::npos) continue;

        std::string iface = line.substr(0, colon);
        // Trim leading spaces from iface
        iface.erase(0, iface.find_first_not_of(" \t"));

        if (iface == "lo") continue;

        if (iface.find("proton") != std::string::npos || iface.find("tun") != std::string::npos || iface.find("wg") != std::string::npos) {
            vpn_active = true;
        }
        if (iface.rfind("wl", 0) == 0 || iface.rfind("wlan", 0) == 0) {
            is_wifi = true;
        }

        std::istringstream ss(line.substr(colon + 1));
        unsigned long long rx_bytes, rx_packets, rx_errs, rx_drop, rx_fifo, rx_frame, rx_comp, rx_multicast;
        unsigned long long tx_bytes;
        ss >> rx_bytes >> rx_packets >> rx_errs >> rx_drop >> rx_fifo >> rx_frame >> rx_comp >> rx_multicast >> tx_bytes;

        total_rx += rx_bytes;
        total_tx += tx_bytes;
    }

    // Check Wi-Fi quality from /proc/net/wireless
    if (is_wifi) {
        std::ifstream wfile("/proc/net/wireless");
        if (wfile.is_open()) {
            std::string wline;
            std::getline(wfile, wline);
            std::getline(wfile, wline);
            if (std::getline(wfile, wline)) {
                size_t wcolon = wline.find(':');
                if (wcolon != std::string::npos) {
                    std::istringstream wss(wline.substr(wcolon + 1));
                    int status = 0;
                    double qual = 0.0;
                    wss >> status >> qual;
                    if (qual > 0) wifi_quality = std::clamp(static_cast<int>((qual / 70.0) * 100.0), 0, 100);
                }
            }
        }
    }

    auto now = std::chrono::steady_clock::now();
    double elapsed_sec = std::chrono::duration<double>(now - prev_net_time).count();
    if (elapsed_sec <= 0.0) elapsed_sec = 1.0;

    unsigned long long rx_diff = (total_rx >= prev_rx_bytes) ? (total_rx - prev_rx_bytes) : 0;
    unsigned long long tx_diff = (total_tx >= prev_tx_bytes) ? (total_tx - prev_tx_bytes) : 0;

    prev_rx_bytes = total_rx;
    prev_tx_bytes = total_tx;
    prev_net_time = now;

    unsigned long long total_speed_bytes = static_cast<unsigned long long>((rx_diff + tx_diff) / elapsed_sec);
    connected = (total_rx > 0 || total_tx > 0);

    char buf[64];
    if (total_speed_bytes >= 1024 * 1024) {
        double mbs = static_cast<double>(total_speed_bytes) / (1024.0 * 1024.0);
        snprintf(buf, sizeof(buf), "%.1f MB/s", mbs);
    } else if (total_speed_bytes >= 1024) {
        double kbs = static_cast<double>(total_speed_bytes) / 1024.0;
        snprintf(buf, sizeof(buf), "%.0f KB/s", kbs);
    } else {
        snprintf(buf, sizeof(buf), "%llu B/s", total_speed_bytes);
    }

    return std::string(buf);
}

} // namespace zenith
