#pragma once

#include <string>

namespace zenith {

struct MediaInfo {
    std::string player_name = "";
    std::string bus_name = "";
    std::string title = "";
    std::string artist = "";
    std::string album = "";
    std::string art_url = "";
    std::string status = "Stopped"; // "Playing", "Paused", "Stopped"
    bool is_playing = false;
    bool can_go_next = false;
    bool can_go_prev = false;
    bool can_play = false;
    bool can_pause = false;

    bool operator==(const MediaInfo& other) const = default;
};

struct MediaChangedEvent {
    MediaInfo info;
};

} // namespace zenith
