#pragma once

#include <string>

namespace zenith {

struct VolumeChangedEvent {
    int volume = 0;
    bool muted = false;
    std::string sink_name;
};

struct MicrophoneChangedEvent {
    int volume = 0;
    bool muted = false;
    std::string source_name;
};

} // namespace zenith
