#pragma once

#include <string>

namespace zenith {

class BacklightManager {
public:
    static void init();
    static bool is_available();

    // Brightness queries
    static int get_brightness_percent();
    static long get_raw_brightness();
    static long get_max_brightness();
    static std::string get_device_name();

    // Brightness controls
    static bool set_brightness_percent(int percent);
    static bool set_raw_brightness(long raw_val);
    static bool increase_brightness(int delta_percent = 5);
    static bool decrease_brightness(int delta_percent = 5);

private:
    static void discover_devices();
    static bool write_sysfs(long raw_val);
    static bool write_login1(long raw_val);
    static bool fallback_brightnessctl(int percent);

    static std::string s_device_name;
    static std::string s_device_path;
    static long s_max_brightness;
    static bool s_initialized;
    static bool s_has_device;
};

} // namespace zenith
