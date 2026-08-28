#include "dbus/mpris_player.hpp"
#include <cstdlib>
#include <array>
#include <memory>
#include <iostream>

namespace zenith {

MediaInfo MprisPlayer::cached_info{};

MediaInfo MprisPlayer::get_info() {
    std::array<char, 256> buffer;
    std::string title_res;
    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen("playerctl metadata title 2>/dev/null", "r"), pclose);
    if (pipe) {
        while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
            title_res += buffer.data();
        }
    }
    if (!title_res.empty() && title_res.back() == '\n') title_res.pop_back();
    cached_info.title = title_res;

    std::string artist_res;
    std::unique_ptr<FILE, decltype(&pclose)> apipe(popen("playerctl metadata artist 2>/dev/null", "r"), pclose);
    if (apipe) {
        while (fgets(buffer.data(), buffer.size(), apipe.get()) != nullptr) {
            artist_res += buffer.data();
        }
    }
    if (!artist_res.empty() && artist_res.back() == '\n') artist_res.pop_back();
    cached_info.artist = artist_res;

    std::string stat_res;
    std::unique_ptr<FILE, decltype(&pclose)> spipe(popen("playerctl status 2>/dev/null", "r"), pclose);
    if (spipe) {
        while (fgets(buffer.data(), buffer.size(), spipe.get()) != nullptr) {
            stat_res += buffer.data();
        }
    }
    cached_info.is_playing = (stat_res.find("Playing") != std::string::npos);

    return cached_info;
}

void MprisPlayer::play_pause() {
    system("playerctl play-pause 2>/dev/null &");
}

void MprisPlayer::next() {
    system("playerctl next 2>/dev/null &");
}

void MprisPlayer::previous() {
    system("playerctl previous 2>/dev/null &");
}

} // namespace zenith
