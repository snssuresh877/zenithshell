#include "system/backlight_manager.hpp"
#include <gio/gio.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <algorithm>
#include <vector>
#include <array>
#include <memory>

namespace fs = std::filesystem;

namespace zenith {

std::string BacklightManager::s_device_name = "";
std::string BacklightManager::s_device_path = "";
long BacklightManager::s_max_brightness = 1;
bool BacklightManager::s_initialized = false;
bool BacklightManager::s_has_device = false;

namespace {

int get_device_priority(const std::string& name) {
    if (name.rfind("amdgpu", 0) == 0 || name.rfind("intel", 0) == 0 || name.rfind("nvidia", 0) == 0) {
        return 100;
    }
    if (name.rfind("pwm", 0) == 0 || name.rfind("apple", 0) == 0 || name.rfind("backlight", 0) == 0) {
        return 50;
    }
    if (name.rfind("acpi", 0) == 0) {
        return 10;
    }
    return 20;
}

std::string exec_cmd_read(const char* cmd) {
    std::array<char, 128> buffer;
    std::string result;
    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd, "r"), pclose);
    if (!pipe) return "";
    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
        result += buffer.data();
    }
    while (!result.empty() && (result.back() == '\n' || result.back() == '\r' || result.back() == ' ')) {
        result.pop_back();
    }
    return result;
}

} // namespace

void BacklightManager::init() {
    if (s_initialized) return;
    discover_devices();
    s_initialized = true;
}

bool BacklightManager::is_available() {
    init();
    return s_has_device;
}

void BacklightManager::discover_devices() {
    const fs::path sysfs_backlight = "/sys/class/backlight";
    std::error_code ec;
    if (!fs::exists(sysfs_backlight, ec) || !fs::is_directory(sysfs_backlight, ec)) {
        s_has_device = false;
        return;
    }

    int best_priority = -1;
    std::string best_name;
    std::string best_path;
    long best_max = 1;

    for (const auto& entry : fs::directory_iterator(sysfs_backlight, ec)) {
        if (!entry.is_directory(ec) && !entry.is_symlink(ec)) continue;

        std::string dev_name = entry.path().filename().string();
        std::string max_p = (entry.path() / "max_brightness").string();
        std::ifstream max_f(max_p);
        long max_b = 0;
        if (max_f.is_open() && (max_f >> max_b) && max_b > 0) {
            int prio = get_device_priority(dev_name);
            if (prio > best_priority) {
                best_priority = prio;
                best_name = dev_name;
                best_path = entry.path().string();
                best_max = max_b;
            }
        }
    }

    if (!best_name.empty()) {
        s_device_name = best_name;
        s_device_path = best_path;
        s_max_brightness = best_max;
        s_has_device = true;
        if (g_getenv("ZENITH_DEBUG")) {
            std::cout << "[BacklightManager] Discovered backlight device: " << s_device_name
                      << " (max: " << s_max_brightness << ")\n";
        }
    } else {
        s_has_device = false;
    }
}

std::string BacklightManager::get_device_name() {
    init();
    return s_device_name;
}

long BacklightManager::get_max_brightness() {
    init();
    return s_max_brightness;
}

long BacklightManager::get_raw_brightness() {
    init();
    if (s_has_device) {
        std::ifstream bri_f(s_device_path + "/brightness");
        long bri = 0;
        if (bri_f.is_open() && (bri_f >> bri)) {
            return bri;
        }
    }
    return -1;
}

int BacklightManager::get_brightness_percent() {
    init();
    if (s_has_device) {
        long raw = get_raw_brightness();
        if (raw >= 0 && s_max_brightness > 0) {
            return std::clamp(static_cast<int>((raw * 100) / s_max_brightness), 1, 100);
        }
    }

    // Fallback to brightnessctl query
    std::string br = exec_cmd_read("brightnessctl -m 2>/dev/null | cut -d, -f4 | tr -d '%'");
    if (!br.empty()) {
        try {
            return std::clamp(std::stoi(br), 1, 100);
        } catch (...) {}
    }

    return 80;
}

bool BacklightManager::write_sysfs(long raw_val) {
    if (!s_has_device) return false;
    std::ofstream out(s_device_path + "/brightness");
    if (!out.is_open()) return false;
    out << raw_val << std::endl;
    return out.good();
}

bool BacklightManager::write_login1(long raw_val) {
    if (!s_has_device) return false;

    GError* error = nullptr;
    GDBusConnection* sys_bus = g_bus_get_sync(G_BUS_TYPE_SYSTEM, nullptr, &error);
    if (!sys_bus) {
        if (error) g_error_free(error);
        return false;
    }

    GVariant* res = g_dbus_connection_call_sync(
        sys_bus,
        "org.freedesktop.login1",
        "/org/freedesktop/login1/session/auto",
        "org.freedesktop.login1.Session",
        "SetBrightness",
        g_variant_new("(ssu)", "backlight", s_device_name.c_str(), static_cast<guint32>(raw_val)),
        nullptr,
        G_DBUS_CALL_FLAGS_NONE,
        1000,
        nullptr,
        &error
    );

    bool success = (res != nullptr && error == nullptr);
    if (res) g_variant_unref(res);
    if (error) g_error_free(error);
    g_object_unref(sys_bus);

    return success;
}

bool BacklightManager::fallback_brightnessctl(int percent) {
    std::string cmd = "brightnessctl set " + std::to_string(percent) + "% 2>/dev/null";
    return (system(cmd.c_str()) == 0);
}

bool BacklightManager::set_raw_brightness(long raw_val) {
    init();
    if (!s_has_device) return false;

    raw_val = std::clamp(raw_val, 1L, s_max_brightness);

    // Tier 1: Direct sysfs
    if (write_sysfs(raw_val)) {
        return true;
    }

    // Tier 2: Systemd/Elogind login1
    if (write_login1(raw_val)) {
        return true;
    }

    // Tier 3: brightnessctl
    int pct = std::clamp(static_cast<int>((raw_val * 100) / s_max_brightness), 1, 100);
    return fallback_brightnessctl(pct);
}

bool BacklightManager::set_brightness_percent(int percent) {
    init();
    percent = std::clamp(percent, 1, 100);

    if (s_has_device && s_max_brightness > 0) {
        long raw_val = (percent * s_max_brightness) / 100;
        if (raw_val < 1 && percent >= 1) raw_val = 1;
        return set_raw_brightness(raw_val);
    }

    return fallback_brightnessctl(percent);
}

bool BacklightManager::increase_brightness(int delta_percent) {
    int cur = get_brightness_percent();
    return set_brightness_percent(cur + delta_percent);
}

bool BacklightManager::decrease_brightness(int delta_percent) {
    int cur = get_brightness_percent();
    return set_brightness_percent(cur - delta_percent);
}

} // namespace zenith
