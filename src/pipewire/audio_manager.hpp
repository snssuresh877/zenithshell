#pragma once

#include <string>
#include <vector>
#include <glib.h>

namespace zenith {

struct AudioDevice {
    int id = 0;
    std::string name;
    bool is_default = false;
};

class AudioManager {
public:
    static void init();
    static int get_volume();
    static bool is_muted();
    static void set_volume(int percent);
    static void toggle_mute();

    static int get_mic_volume();
    static bool is_mic_muted();
    static void set_mic_volume(int percent);
    static void toggle_mic_mute();

    static std::string get_default_sink_name();
    static std::string get_default_source_name();
    static std::vector<AudioDevice> get_sinks();
    static std::vector<AudioDevice> get_sources();
    static void set_default_sink(int id);
    static void set_default_source(int id);

    static bool is_noise_cancelling_active();
    static void toggle_noise_cancelling();

    static void update();

private:
    static int cached_volume;
    static bool cached_muted;
    static int cached_mic_volume;
    static bool cached_mic_muted;
    static std::string cached_sink_name;
    static std::string cached_source_name;
};

} // namespace zenith
