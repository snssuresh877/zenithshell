#pragma once

#include <string>

namespace zenith {

struct MediaInfo {
    std::string title = "";
    std::string artist = "";
    bool is_playing = false;
};

class MprisPlayer {
public:
    static MediaInfo get_info();
    static void play_pause();
    static void next();
    static void previous();

private:
    static MediaInfo cached_info;
};

} // namespace zenith
