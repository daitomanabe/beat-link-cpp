#pragma once

#include <algorithm>
#include <cstdint>
#include <fmt/format.h>
#include "../Span.hpp"
#include <stdexcept>
#include <vector>

#include "../Util.hpp"
#include "AnlzTypes.hpp"
#include "DataReference.hpp"
#include "beatlink/dbserver/BinaryField.hpp"
#include "beatlink/dbserver/Message.hpp"

namespace beatlink::data {

class BeatGrid {
public:
    /**
     * Construct from ANLZ beat grid entries (for AnlzParser)
     */
    explicit BeatGrid(std::vector<BeatGridEntry> entries)
        : dataReference(DataReference{})
    {
        beatWithinBarValues_.reserve(entries.size());
        bpmValues_.reserve(entries.size());
        timeWithinTrackValues_.reserve(entries.size());

        for (const auto& entry : entries) {
            beatWithinBarValues_.push_back(entry.beatNumber);
            bpmValues_.push_back(static_cast<int>(entry.tempo * 100.0f));  // Convert to BPM*100
            timeWithinTrackValues_.push_back(entry.timeMs);
        }
    }

    explicit BeatGrid(DataReference reference, const beatlink::dbserver::Message& message)
        : dataReference(std::move(reference))
    {
        auto binary = std::dynamic_pointer_cast<beatlink::dbserver::BinaryField>(message.arguments.at(3));
        if (!binary) {
            throw std::runtime_error("BeatGrid response missing binary field payload");
        }
        rawData_.assign(binary->getValue().begin(), binary->getValue().end());
        parseData(rawData_);
    }

    BeatGrid(DataReference reference, std::span<const uint8_t> buffer)
        : dataReference(std::move(reference))
        , rawData_(buffer.begin(), buffer.end())
    {
        parseData(rawData_);
    }

    BeatGrid(DataReference reference, std::vector<int> beatWithinBarValues,
             std::vector<int> bpmValues, std::vector<int64_t> timeWithinTrackValues)
        : dataReference(std::move(reference))
        , rawData_()
        , beatWithinBarValues_(std::move(beatWithinBarValues))
        , bpmValues_(std::move(bpmValues))
        , timeWithinTrackValues_(std::move(timeWithinTrackValues))
    {
        if (beatWithinBarValues_.size() != bpmValues_.size() ||
            beatWithinBarValues_.size() != timeWithinTrackValues_.size()) {
            throw std::invalid_argument("Beat grid arrays must contain the same number of beats.");
        }
    }

    std::span<const uint8_t> getRawData() const { return rawData_; }

    int getBeatCount() const { return static_cast<int>(beatWithinBarValues_.size()); }

    int64_t getTimeWithinTrack(int beatNumber) const {
        if (beatNumber == 0) {
            return 0;
        }
        return timeWithinTrackValues_.at(beatOffset(beatNumber));
    }

    int getBeatWithinBar(int beatNumber) const {
        return beatWithinBarValues_.at(beatOffset(beatNumber));
    }

    int getBpm(int beatNumber) const {
        return bpmValues_.at(beatOffset(beatNumber));
    }

    int getBarNumber(int beatNumber) const {
        const int offset = getBeatWithinBar(1) - 1;
        const int bar = (offset + beatOffset(beatNumber)) / 4;
        if (offset == 0) {
            return bar + 1;
        }
        if (bar == 0) {
            return -1;
        }
        return bar;
    }

    int findBeatAtTime(int64_t milliseconds) const {
        auto it = std::lower_bound(timeWithinTrackValues_.begin(), timeWithinTrackValues_.end(), milliseconds);
        if (it == timeWithinTrackValues_.begin()) {
            return (it != timeWithinTrackValues_.end() && *it == milliseconds) ? 1 : -1;
        }
        if (it != timeWithinTrackValues_.end() && *it == milliseconds) {
            return static_cast<int>(std::distance(timeWithinTrackValues_.begin(), it)) + 1;
        }
        return static_cast<int>(std::distance(timeWithinTrackValues_.begin(), it));
    }

    std::string toString() const {
        return fmt::format("BeatGrid[dataReference:{}, beats:{}]",
                           dataReference.toString(), getBeatCount());
    }

    DataReference dataReference;

private:
    int beatOffset(int beatNumber) const {
        const int count = getBeatCount();
        if (count == 0) {
            throw std::runtime_error("There are no beats in this beat grid.");
        }
        if (beatNumber < 1) {
            return 0;
        }
        if (beatNumber > count) {
            return count - 1;
        }
        return beatNumber - 1;
    }

    void parseData(std::span<const uint8_t> raw) {
        const size_t length = raw.size();
        const int beatCount = (length > 20) ? static_cast<int>((length - 20) / 16) : 0;
        beatWithinBarValues_.resize(beatCount);
        bpmValues_.resize(beatCount);
        timeWithinTrackValues_.resize(beatCount);
        for (int beatNumber = 0; beatNumber < beatCount; ++beatNumber) {
            const size_t base = 20 + static_cast<size_t>(beatNumber) * 16;
            beatWithinBarValues_[beatNumber] = static_cast<int>(beatlink::Util::bytesToNumberLittleEndian(raw.data(), base, 2));
            bpmValues_[beatNumber] = static_cast<int>(beatlink::Util::bytesToNumberLittleEndian(raw.data(), base + 2, 2));
            timeWithinTrackValues_[beatNumber] = beatlink::Util::bytesToNumberLittleEndian(raw.data(), base + 4, 4);
        }
    }

    std::vector<uint8_t> rawData_;
    std::vector<int> beatWithinBarValues_;
    std::vector<int> bpmValues_;
    std::vector<int64_t> timeWithinTrackValues_;
};

} // namespace beatlink::data
