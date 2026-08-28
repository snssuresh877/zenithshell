#include "pipewire/audio_manager.hpp"
#include <array>
#include <memory>
#include <cstdio>
#include <cstdlib>
#include <algorithm>
#include <iostream>
#include <sstream>
#include <regex>

namespace zenith {

int AudioManager::cached_volume = 50;
bool AudioManager::cached_muted = false;
int AudioManager::cached_mic_volume = 100;
bool AudioManager::cached_mic_muted = false;
std::string AudioManager::cached_sink_name = "Speakers";
std::string AudioManager::cached_source_name = "Internal Microphone";

static std::string exec_cmd(const char* cmd) {
    std::array<char, 256> buffer;
    std::string result;
    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd, "r"), pclose);
    if (!pipe) return "";
    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
        result += buffer.data();
    }
    return result;
}

void AudioManager::init() {
    update();
    g_timeout_add_seconds(2, +[](gpointer) -> gboolean {
        update();
        return TRUE;
    }, nullptr);
}

int AudioManager::get_volume() {
    return cached_volume;
}

bool AudioManager::is_muted() {
    return cached_muted;
}

void AudioManager::set_volume(int volume) {
    int vol = std::clamp(volume, 0, 150);
    cached_volume = vol;
    float v = vol / 100.0f;
    char cmd[64];
    snprintf(cmd, sizeof(cmd), "wpctl set-volume @DEFAULT_AUDIO_SINK@ %.2f 2>/dev/null &", v);
    system(cmd);
}

void AudioManager::toggle_mute() {
    cached_muted = !cached_muted;
    system("wpctl set-mute @DEFAULT_AUDIO_SINK@ toggle 2>/dev/null &");
}

int AudioManager::get_mic_volume() {
    return cached_mic_volume;
}

bool AudioManager::is_mic_muted() {
    return cached_mic_muted;
}

void AudioManager::set_mic_volume(int volume) {
    int vol = std::clamp(volume, 0, 150);
    cached_mic_volume = vol;
    float v = vol / 100.0f;
    char cmd[64];
    snprintf(cmd, sizeof(cmd), "wpctl set-volume @DEFAULT_AUDIO_SOURCE@ %.2f 2>/dev/null &", v);
    system(cmd);
}

void AudioManager::toggle_mic_mute() {
    cached_mic_muted = !cached_mic_muted;
    system("wpctl set-mute @DEFAULT_AUDIO_SOURCE@ toggle 2>/dev/null &");
}

std::string AudioManager::get_default_sink_name() {
    return cached_sink_name;
}

std::string AudioManager::get_default_source_name() {
    return cached_source_name;
}

static std::vector<AudioDevice> parse_wpctl_section(const std::string& section) {
    std::vector<AudioDevice> devices;
    std::string out = exec_cmd("wpctl status 2>/dev/null");
    std::istringstream stream(out);
    std::string line;
    bool in_section = false;

    std::regex dev_regex(R"(([*]?)\s*(\d+)\.\s+(.+?)\s+\[vol:)");

    while (std::getline(stream, line)) {
        if (line.find("├─ " + section + ":") != std::string::npos || line.find("└─ " + section + ":") != std::string::npos) {
            in_section = true;
            continue;
        } else if (in_section) {
            if (line.find("├─") != std::string::npos || line.find("└─") != std::string::npos) {
                if (line.find("Streams:") != std::string::npos || line.find("Filters:") != std::string::npos || line.find("Devices:") != std::string::npos || line.find("Sources:") != std::string::npos) {
                    break;
                }
            }
            if (line.empty()) continue;
        }

        if (in_section) {
            std::smatch match;
            if (std::regex_search(line, match, dev_regex)) {
                AudioDevice dev;
                dev.is_default = (match[1].length() > 0);
                dev.id = std::stoi(match[2].str());
                dev.name = match[3].str();
                
                // Trim trailing spaces
                while (!dev.name.empty() && (dev.name.back() == ' ' || dev.name.back() == '\t')) {
                    dev.name.pop_back();
                }

                // Shorten common lengthy names for clean UI
                if (dev.name.find("Speaker") != std::string::npos) {
                    dev.name = "Built-in Speakers";
                } else if (dev.name.find("Headphones") != std::string::npos) {
                    dev.name = "Wired Headphones";
                } else if (dev.name.find("Digital Microphone") != std::string::npos) {
                    dev.name = "Built-in Microphone";
                } else if (dev.name.find("Stereo Microphone") != std::string::npos) {
                    dev.name = "Headset Microphone";
                }

                devices.push_back(dev);
            }
        }
    }
    return devices;
}

std::vector<AudioDevice> AudioManager::get_sinks() {
    auto sinks = parse_wpctl_section("Sinks");
    if (sinks.empty()) {
        sinks.push_back({52, "Built-in Speakers", true});
    }
    return sinks;
}

std::vector<AudioDevice> AudioManager::get_sources() {
    auto sources = parse_wpctl_section("Sources");
    if (sources.empty()) {
        sources.push_back({54, "Built-in Microphone", true});
    }
    return sources;
}

void AudioManager::set_default_sink(int id) {
    char cmd[64];
    snprintf(cmd, sizeof(cmd), "wpctl set-default %d 2>/dev/null &", id);
    system(cmd);
    update();
}

void AudioManager::set_default_source(int id) {
    char cmd[64];
    snprintf(cmd, sizeof(cmd), "wpctl set-default %d 2>/dev/null &", id);
    system(cmd);
    update();
}

bool AudioManager::is_noise_cancelling_active() {
    std::string out = exec_cmd("pgrep -x easyeffects 2>/dev/null || ip link show rnnoise 2>/dev/null");
    return !out.empty();
}

void AudioManager::toggle_noise_cancelling() {
    if (is_noise_cancelling_active()) {
        system("killall easyeffects 2>/dev/null || true");
    } else {
        system("easyeffects --gapplication-service 2>/dev/null &");
    }
}

void AudioManager::update() {
    // Sink Volume & Mute
    std::string sink_res = exec_cmd("wpctl get-volume @DEFAULT_AUDIO_SINK@ 2>/dev/null");
    if (!sink_res.empty()) {
        cached_muted = (sink_res.find("[MUTED]") != std::string::npos);
        float vol = 0.0f;
        if (sscanf(sink_res.c_str(), "Volume: %f", &vol) == 1) {
            cached_volume = static_cast<int>(vol * 100.0f + 0.5f);
        }
    }

    // Source Volume & Mute
    std::string src_res = exec_cmd("wpctl get-volume @DEFAULT_AUDIO_SOURCE@ 2>/dev/null");
    if (!src_res.empty()) {
        cached_mic_muted = (src_res.find("[MUTED]") != std::string::npos);
        float vol = 0.0f;
        if (sscanf(src_res.c_str(), "Volume: %f", &vol) == 1) {
            cached_mic_volume = static_cast<int>(vol * 100.0f + 0.5f);
        }
    }

    // Default Sink Name
    auto sinks = get_sinks();
    for (const auto& s : sinks) {
        if (s.is_default) {
            cached_sink_name = s.name;
            break;
        }
    }

    // Default Source Name
    auto sources = get_sources();
    for (const auto& s : sources) {
        if (s.is_default) {
            cached_source_name = s.name;
            break;
        }
    }
}

} // namespace zenith
