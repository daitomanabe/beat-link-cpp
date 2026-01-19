#pragma once

#include <array>
#include <cstdint>
#include <format>
#include <span>
#include <string>
#include <vector>

#include "../Util.hpp"
#include "BeatGrid.hpp"
#include "DataReference.hpp"
#include "TrackMetadata.hpp"
#include "WaveformTypes.hpp"
#include "beatlink/dbserver/BinaryField.hpp"
#include "beatlink/dbserver/Message.hpp"

namespace beatlink::data {

class WaveformDetail {
public:
    static constexpr int LEADING_DBSERVER_JUNK_BYTES = 19;
    static constexpr int LEADING_DBSERVER_COLOR_JUNK_BYTES = 28;

    static const std::array<Color, 8> COLOR_MAP;

    WaveformDetail(DataReference reference, const beatlink::dbserver::Message& message, WaveformStyle style);
    WaveformDetail(DataReference reference, std::vector<uint8_t> data, WaveformStyle style);

    /**
     * Construct from ANLZ waveform detail entries (for AnlzParser)
     * @param entries Raw entry data
     * @param entrySize Size of each entry
     */
    WaveformDetail(const std::vector<uint8_t>& entries, unsigned int entrySize)
        : dataReference()
        , data_(entries)
        , isColor_(entrySize > 1)
        , style_(entrySize > 1 ? WaveformStyle::RGB : WaveformStyle::BLUE)
    {
        (void)entrySize;  // Used for style detection above
    }

    std::span<const uint8_t> getData() const { return data_; }
    int getFrameCount() const;
    int64_t getTotalTime() const;

    WaveformStyle getStyle() const { return style_; }
    bool isColor() const { return isColor_; }

    int segmentHeight(int segment, int scale) const;
    int segmentHeight(int segment, int scale, ThreeBandLayer band) const;
    Color segmentColor(int segment, int scale) const;

    bool operator==(const WaveformDetail& other) const {
        return dataReference == other.dataReference &&
               style_ == other.style_ &&
               data_ == other.data_;
    }

    bool operator!=(const WaveformDetail& other) const { return !(*this == other); }

    std::string toString() const;

    DataReference dataReference;

private:
    int getColorWaveformBits(int segment) const;

    std::vector<uint8_t> data_;
    bool isColor_{false};
    WaveformStyle style_{WaveformStyle::BLUE};
};

} // namespace beatlink::data
