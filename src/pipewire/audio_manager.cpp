#include "pipewire/audio_manager.hpp"
#include "shell/osd/osd_window.hpp"
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
std::vector<AudioDevice> AudioManager::cached_sinks;
std::vector<AudioDevice> AudioManager::cached_sources;

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
    // Relaxed 15-second heartbeat for background synchronization (down from 2s)
    g_timeout_add_seconds(15, +[](gpointer) -> gboolean {
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
    snprintf(cmd, sizeof(cmd), "wpctl set-volume @DEFAULT_AUDIO_SINK@ %.2f", v);
    g_spawn_command_line_async(cmd, nullptr);
    OSDWindow::show_volume(cached_volume, cached_muted);
}

void AudioManager::toggle_mute() {
    cached_muted = !cached_muted;
    g_spawn_command_line_async("wpctl set-mute @DEFAULT_AUDIO_SINK@ toggle", nullptr);
    OSDWindow::show_volume(cached_volume, cached_muted);
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
    snprintf(cmd, sizeof(cmd), "wpctl set-volume @DEFAULT_AUDIO_SOURCE@ %.2f", v);
    g_spawn_command_line_async(cmd, nullptr);
    OSDWindow::show_mic(cached_mic_volume, cached_mic_muted);
}

void AudioManager::toggle_mic_mute() {
    cached_mic_muted = !cached_mic_muted;
    g_spawn_command_line_async("wpctl set-mute @DEFAULT_AUDIO_SOURCE@ toggle", nullptr);
    OSDWindow::show_mic(cached_mic_volume, cached_mic_muted);
}

std::string AudioManager::get_default_sink_name() {
    return cached_sink_name;
}

std::string AudioManager::get_default_source_name() {
    return cached_source_name;
}

std::vector<AudioDevice> AudioManager::get_sinks() {
    if (cached_sinks.empty()) {
        update();
    }
    if (cached_sinks.empty()) {
        return {{52, "Built-in Speakers", true}};
    }
    return cached_sinks;
}

std::vector<AudioDevice> AudioManager::get_sources() {
    if (cached_sources.empty()) {
        update();
    }
    if (cached_sources.empty()) {
        return {{54, "Built-in Microphone", true}};
    }
    return cached_sources;
}

void AudioManager::set_default_sink(int id) {
    char cmd[64];
    snprintf(cmd, sizeof(cmd), "wpctl set-default %d", id);
    g_spawn_command_line_async(cmd, nullptr);
    g_timeout_add(100, +[](gpointer) -> gboolean {
        AudioManager::update();
        return G_SOURCE_REMOVE;
    }, nullptr);
}

void AudioManager::set_default_source(int id) {
    char cmd[64];
    snprintf(cmd, sizeof(cmd), "wpctl set-default %d", id);
    g_spawn_command_line_async(cmd, nullptr);
    g_timeout_add(100, +[](gpointer) -> gboolean {
        AudioManager::update();
        return G_SOURCE_REMOVE;
    }, nullptr);
}

bool AudioManager::is_noise_cancelling_active() {
    std::string out = exec_cmd("pgrep -x easyeffects 2>/dev/null || ip link show rnnoise 2>/dev/null");
    return !out.empty();
}

void AudioManager::toggle_noise_cancelling() {
    if (is_noise_cancelling_active()) {
        g_spawn_command_line_async("killall easyeffects", nullptr);
    } else {
        g_spawn_command_line_async("easyeffects --gapplication-service", nullptr);
    }
}

void AudioManager::update() {
    std::string out = exec_cmd("wpctl status 2>/dev/null");
    if (out.empty()) return;

    std::istringstream stream(out);
    std::string line;

    bool in_audio = false;
    bool in_sinks = false;
    bool in_sources = false;

    std::vector<AudioDevice> new_sinks;
    std::vector<AudioDevice> new_sources;

    std::regex dev_regex(R"(([*]?)\s*(\d+)\.\s+(.+?)(?:\s+\[vol:\s*([\d.]+)(.*?)\])?$)");

    while (std::getline(stream, line)) {
        if (line.rfind("Audio", 0) == 0) {
            in_audio = true;
            continue;
        } else if (line.rfind("Video", 0) == 0 || line.rfind("Settings", 0) == 0) {
            in_audio = false;
            in_sinks = false;
            in_sources = false;
            continue;
        }

        if (!in_audio) continue;

        if (line.find("Sinks:") != std::string::npos) {
            in_sinks = true;
            in_sources = false;
            continue;
        } else if (line.find("Sources:") != std::string::npos) {
            in_sinks = false;
            in_sources = true;
            continue;
        } else if (line.find("Filters:") != std::string::npos ||
                   line.find("Streams:") != std::string::npos ||
                   line.find("Devices:") != std::string::npos) {
            in_sinks = false;
            in_sources = false;
            continue;
        }

        if (in_sinks || in_sources) {
            std::smatch match;
            if (std::regex_search(line, match, dev_regex)) {
                bool is_default = (match[1].length() > 0);
                int dev_id = std::stoi(match[2].str());
                std::string dev_name = match[3].str();

                // Trim trailing spaces / tabs
                while (!dev_name.empty() && (dev_name.back() == ' ' || dev_name.back() == '\t')) {
                    dev_name.pop_back();
                }

                // Friendly UI presentation
                if (dev_name.find("Speaker") != std::string::npos) {
                    dev_name = "Built-in Speakers";
                } else if (dev_name.find("Headphones") != std::string::npos) {
                    dev_name = "Wired Headphones";
                } else if (dev_name.find("Digital Microphone") != std::string::npos) {
                    dev_name = "Built-in Microphone";
                } else if (dev_name.find("Stereo Microphone") != std::string::npos) {
                    dev_name = "Headset Microphone";
                }

                AudioDevice dev;
                dev.id = dev_id;
                dev.name = dev_name;
                dev.is_default = is_default;

                float vol_f = 1.0f;
                bool is_muted_dev = false;

                if (match[4].matched) {
                    try {
                        vol_f = std::stof(match[4].str());
                    } catch (...) {
                        vol_f = 1.0f;
                    }
                }
                if (match[5].matched) {
                    std::string flags = match[5].str();
                    if (flags.find("MUTED") != std::string::npos) {
                        is_muted_dev = true;
                    }
                }

                if (in_sinks) {
                    new_sinks.push_back(dev);
                    if (is_default) {
                        cached_volume = static_cast<int>(vol_f * 100.0f + 0.5f);
                        cached_muted = is_muted_dev;
                        cached_sink_name = dev_name;
                    }
                } else if (in_sources) {
                    new_sources.push_back(dev);
                    if (is_default) {
                        cached_mic_volume = static_cast<int>(vol_f * 100.0f + 0.5f);
                        cached_mic_muted = is_muted_dev;
                        cached_source_name = dev_name;
                    }
                }
            }
        }
    }

    if (!new_sinks.empty()) {
        cached_sinks = std::move(new_sinks);
    }
    if (!new_sources.empty()) {
        cached_sources = std::move(new_sources);
    }
}

} // namespace zenith
