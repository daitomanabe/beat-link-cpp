#pragma once

#include <fmt/format.h>
#include <string>

namespace beatlink::data {

class PlaybackState {
public:
    PlaybackState(int player, int64_t position, bool playing)
        : player(player)
        , position(position)
        , playing(playing)
    {
    }

    int player;
    int64_t position;
    bool playing;

    bool operator==(const PlaybackState& other) const {
        return player == other.player && position == other.position && playing == other.playing;
    }

    bool operator!=(const PlaybackState& other) const {
        return !(*this == other);
    }

    std::string toString() const {
        return fmt::format("PlaybackState[player:{}, position:{}, playing:{}]",
                           player, position, playing ? "true" : "false");
    }
};

} // namespace beatlink::data
