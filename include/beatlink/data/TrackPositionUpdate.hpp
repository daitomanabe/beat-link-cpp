#pragma once

#include <format>
#include <memory>

#include "BeatGrid.hpp"

namespace beatlink::data {

class TrackPositionUpdate {
public:
    TrackPositionUpdate(int64_t timestamp,
                        int64_t milliseconds,
                        int beatNumber,
                        bool definitive,
                        bool playing,
                        double pitch,
                        bool reverse,
                        std::shared_ptr<BeatGrid> beatGrid,
                        bool precise,
                        bool fromBeat)
        : timestamp(timestamp)
        , milliseconds(milliseconds)
        , beatNumber(beatNumber)
        , definitive(definitive)
        , precise(precise)
        , fromBeat(fromBeat)
        , playing(playing)
        , pitch(pitch)
        , reverse(reverse)
        , beatGrid(std::move(beatGrid))
    {
    }

    int64_t timestamp;
    int64_t milliseconds;
    int beatNumber;
    bool definitive;
    bool precise;
    bool fromBeat;
    bool playing;
    double pitch;
    bool reverse;
    std::shared_ptr<BeatGrid> beatGrid;

    int getBeatWithinBar() const {
        if (beatGrid) {
            return beatGrid->getBeatWithinBar(beatNumber);
        }
        return 0;
    }

    std::string toString() const {
        return std::format(
            "TrackPositionUpdate[timestamp:{}, milliseconds:{}, beatNumber:{}, definitive:{}, playing:{}, pitch:{:.6f}, reverse:{}, beatGrid:{}, precise:{}, fromBeat:{}]",
            timestamp,
            milliseconds,
            beatNumber,
            definitive ? "true" : "false",
            playing ? "true" : "false",
            pitch,
            reverse ? "true" : "false",
            beatGrid ? beatGrid->toString() : "null",
            precise ? "true" : "false",
            fromBeat ? "true" : "false");
    }
};

} // namespace beatlink::data
