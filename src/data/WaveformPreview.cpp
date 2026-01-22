#include "beatlink/data/WaveformPreview.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace beatlink::data {

const Color WaveformPreview::INTENSE_COLOR = Color{116, 246, 244, 255};
const Color WaveformPreview::NORMAL_COLOR = Color{43, 89, 255, 255};

WaveformPreview::WaveformPreview(DataReference reference, const beatlink::dbserver::Message& message,
                                 WaveformStyle style)
    : dataReference(std::move(reference))
    , isColor_(style != WaveformStyle::BLUE)
    , style_(style)
{
    auto blob = std::dynamic_pointer_cast<beatlink::dbserver::BinaryField>(message.arguments.at(3));
    if (!blob) {
        throw std::runtime_error("WaveformPreview response missing binary field");
    }
    auto bytes = blob->getValueAsArray();
    const size_t offset = isColor_ ? LEADING_DBSERVER_COLOR_JUNK_BYTES : 0;
    if (offset < bytes.size()) {
        data_.assign(bytes.begin() + static_cast<std::ptrdiff_t>(offset), bytes.end());
    } else {
        data_.clear();
    }
    segmentCount_ = calculateSegmentCount();
    maxHeight_ = calculateMaxHeight();
}

WaveformPreview::WaveformPreview(DataReference reference, std::vector<uint8_t> data, WaveformStyle style)
    : dataReference(std::move(reference))
    , data_(std::move(data))
    , isColor_(style == WaveformStyle::RGB)
    , style_(style)
{
    segmentCount_ = calculateSegmentCount();
    maxHeight_ = calculateMaxHeight();
}

int WaveformPreview::calculateSegmentCount() const {
    const int bytes = static_cast<int>(data_.size());
    switch (style_) {
        case WaveformStyle::BLUE:
            return bytes / 2;
        case WaveformStyle::THREE_BAND:
            return bytes / 3;
        case WaveformStyle::RGB:
            return bytes / 6;
    }
    return 0;
}

int WaveformPreview::calculateMaxHeight() const {
    int result = 0;
    for (int i = 0; i < segmentCount_; ++i) {
        if (style_ == WaveformStyle::THREE_BAND) {
            result = std::max(result, segmentHeight(i, ThreeBandLayer::LOW));
            result = std::max(result, segmentHeight(i, ThreeBandLayer::MID));
            result = std::max(result, segmentHeight(i, ThreeBandLayer::HIGH));
        } else {
            result = std::max(result, segmentHeight(i, false));
        }
    }
    return result;
}

int WaveformPreview::segmentHeight(int segment, bool front) const {
    if (segment < 0 || segment >= segmentCount_) {
        return 0;
    }
    switch (style_) {
        case WaveformStyle::THREE_BAND:
            throw std::runtime_error("segmentHeight(front) unsupported for three-band waveforms");
        case WaveformStyle::RGB: {
            const int base = segment * 6;
            const int frontHeight = beatlink::Util::unsign(static_cast<int8_t>(data_.at(base + 5)));
            if (front) {
                return frontHeight;
            }
            const int backRed = beatlink::Util::unsign(static_cast<int8_t>(data_.at(base + 3)));
            const int backGreen = beatlink::Util::unsign(static_cast<int8_t>(data_.at(base + 4)));
            return std::max(frontHeight, std::max(backRed, backGreen));
        }
        case WaveformStyle::BLUE:
            return data_.at(segment * 2) & 0x1f;
    }
    return 0;
}

int WaveformPreview::segmentHeight(int segment, ThreeBandLayer band) const {
    if (style_ != WaveformStyle::THREE_BAND) {
        throw std::runtime_error("segmentHeight(ThreeBandLayer) only valid for three-band waveforms");
    }
    if (segment < 0 || segment >= segmentCount_) {
        return 0;
    }
    const int base = segment * 3;
    const float lowScaled = beatlink::Util::unsign(static_cast<int8_t>(data_.at(base + 2))) * 0.49f;
    if (band == ThreeBandLayer::LOW) {
        return static_cast<int>(std::round(lowScaled));
    }
    const float midScaled = beatlink::Util::unsign(static_cast<int8_t>(data_.at(base))) * 0.32f;
    if (band == ThreeBandLayer::MID) {
        return static_cast<int>(std::round(lowScaled + midScaled));
    }
    const float highScaled = beatlink::Util::unsign(static_cast<int8_t>(data_.at(base + 1))) * 0.25f;
    if (band == ThreeBandLayer::HIGH) {
        return static_cast<int>(std::round(lowScaled + midScaled + highScaled));
    }
    return static_cast<int>(std::round(lowScaled));
}

Color WaveformPreview::segmentColor(int segment, bool front) const {
    switch (style_) {
        case WaveformStyle::THREE_BAND:
            throw std::runtime_error("segmentColor unsupported for three-band waveforms");
        case WaveformStyle::RGB: {
            const int base = segment * 6;
            const int backHeight = segmentHeight(segment, false);
            if (backHeight == 0) {
                return Color{0, 0, 0, 255};
            }
            const int maxLevel = front ? 255 : 191;
            const int red = beatlink::Util::unsign(static_cast<int8_t>(data_.at(base + 3))) * maxLevel / backHeight;
            const int green = beatlink::Util::unsign(static_cast<int8_t>(data_.at(base + 4))) * maxLevel / backHeight;
            const int blue = beatlink::Util::unsign(static_cast<int8_t>(data_.at(base + 5))) * maxLevel / backHeight;
            return Color{static_cast<uint8_t>(red), static_cast<uint8_t>(green), static_cast<uint8_t>(blue), 255};
        }
        case WaveformStyle::BLUE: {
            const int intensity = data_.at(segment * 2 + 1) & 0x07;
            return (intensity >= 5) ? INTENSE_COLOR : NORMAL_COLOR;
        }
    }
    return Color{0, 0, 0, 255};
}

std::string WaveformPreview::toString() const {
    return fmt::format("WaveformPreview[dataReference={}, style:{}, size:{}, segments:{}]",
                       dataReference.toString(), static_cast<int>(style_), data_.size(), segmentCount_);
}

} // namespace beatlink::data
