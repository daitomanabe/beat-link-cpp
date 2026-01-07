#pragma once

#include <cstdint>
#include <format>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include "../Util.hpp"
#include "DataReference.hpp"
#include "WaveformTypes.hpp"
#include "beatlink/dbserver/BinaryField.hpp"
#include "beatlink/dbserver/Message.hpp"

namespace beatlink::data {

class WaveformPreview {
public:
    static constexpr int LEADING_DBSERVER_COLOR_JUNK_BYTES = 28;
    static const Color INTENSE_COLOR;
    static const Color NORMAL_COLOR;

    WaveformPreview(DataReference reference, const beatlink::dbserver::Message& message, WaveformStyle style);
    WaveformPreview(DataReference reference, std::vector<uint8_t> data, WaveformStyle style);

    std::span<const uint8_t> getData() const { return data_; }

    int getSegmentCount() const { return segmentCount_; }
    int getMaxHeight() const { return maxHeight_; }
    WaveformStyle getStyle() const { return style_; }
    bool isColor() const { return isColor_; }

    int segmentHeight(int segment, bool front) const;
    int segmentHeight(int segment, ThreeBandLayer band) const;
    Color segmentColor(int segment, bool front) const;

    bool operator==(const WaveformPreview& other) const {
        return dataReference == other.dataReference &&
               style_ == other.style_ &&
               data_ == other.data_;
    }

    bool operator!=(const WaveformPreview& other) const { return !(*this == other); }

    std::string toString() const;

    DataReference dataReference;

private:
    int calculateSegmentCount() const;
    int calculateMaxHeight() const;

    std::vector<uint8_t> data_;
    bool isColor_{false};
    WaveformStyle style_{WaveformStyle::BLUE};
    int segmentCount_{0};
    int maxHeight_{0};
};

} // namespace beatlink::data
