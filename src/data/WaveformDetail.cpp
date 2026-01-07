#include "beatlink/data/WaveformDetail.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace beatlink::data {

const std::array<Color, 8> WaveformDetail::COLOR_MAP = {
    Color{0, 104, 144, 255},
    Color{0, 136, 176, 255},
    Color{0, 168, 232, 255},
    Color{0, 184, 216, 255},
    Color{120, 184, 216, 255},
    Color{136, 192, 232, 255},
    Color{136, 192, 232, 255},
    Color{200, 224, 232, 255}
};

WaveformDetail::WaveformDetail(DataReference reference, const beatlink::dbserver::Message& message,
                               WaveformStyle style)
    : dataReference(std::move(reference))
    , isColor_(style != WaveformStyle::BLUE)
    , style_(style)
{
    auto blob = std::dynamic_pointer_cast<beatlink::dbserver::BinaryField>(message.arguments.at(3));
    if (!blob) {
        throw std::runtime_error("WaveformDetail response missing binary field");
    }
    auto bytes = blob->getValueAsArray();
    const size_t offset = isColor_ ? LEADING_DBSERVER_COLOR_JUNK_BYTES : LEADING_DBSERVER_JUNK_BYTES;
    if (offset < bytes.size()) {
        data_.assign(bytes.begin() + static_cast<std::ptrdiff_t>(offset), bytes.end());
    } else {
        data_.clear();
    }
}

WaveformDetail::WaveformDetail(DataReference reference, std::vector<uint8_t> data, WaveformStyle style)
    : dataReference(std::move(reference))
    , data_(std::move(data))
    , isColor_(style != WaveformStyle::BLUE)
    , style_(style)
{
}

int WaveformDetail::getFrameCount() const {
    const int bytes = static_cast<int>(data_.size());
    switch (style_) {
        case WaveformStyle::THREE_BAND:
            return bytes / 3;
        case WaveformStyle::RGB:
            return bytes / 2;
        case WaveformStyle::BLUE:
            return bytes;
    }
    return 0;
}

int64_t WaveformDetail::getTotalTime() const {
    return beatlink::Util::halfFrameToTime(getFrameCount());
}

int WaveformDetail::getColorWaveformBits(int segment) const {
    const int base = segment * 2;
    const int big = beatlink::Util::unsign(static_cast<int8_t>(data_.at(base)));
    const int small = beatlink::Util::unsign(static_cast<int8_t>(data_.at(base + 1)));
    return big * 256 + small;
}

int WaveformDetail::segmentHeight(int segment, int scale) const {
    const int limit = getFrameCount();
    int sum = 0;
    for (int i = segment; (i < segment + scale) && (i < limit); ++i) {
        switch (style_) {
            case WaveformStyle::THREE_BAND:
                throw std::runtime_error("segmentHeight(scale) unsupported for three-band waveforms");
            case WaveformStyle::RGB:
                sum += (getColorWaveformBits(i) >> 2) & 0x1f;
                break;
            case WaveformStyle::BLUE:
                sum += data_.at(i) & 0x1f;
                break;
        }
    }
    return scale > 0 ? sum / scale : 0;
}

int WaveformDetail::segmentHeight(int segment, int scale, ThreeBandLayer band) const {
    if (style_ != WaveformStyle::THREE_BAND) {
        throw std::runtime_error("segmentHeight(ThreeBandLayer) only valid for three-band waveforms");
    }
    const int limit = getFrameCount();
    int sum = 0;
    for (int i = segment; (i < segment + scale) && (i < limit); ++i) {
        const int base = i * 3;
        switch (band) {
            case ThreeBandLayer::LOW:
                sum += static_cast<int>(std::round(beatlink::Util::unsign(static_cast<int8_t>(data_.at(base + 2))) * 0.4f));
                break;
            case ThreeBandLayer::MID:
                sum += static_cast<int>(std::round(beatlink::Util::unsign(static_cast<int8_t>(data_.at(base))) * 0.3f));
                break;
            case ThreeBandLayer::HIGH:
                sum += static_cast<int>(std::round(beatlink::Util::unsign(static_cast<int8_t>(data_.at(base + 1))) * 0.06f));
                break;
            case ThreeBandLayer::LOW_AND_MID:
                sum += static_cast<int>(std::round(beatlink::Util::unsign(static_cast<int8_t>(data_.at(base + 2))) * 0.4f));
                sum += static_cast<int>(std::round(beatlink::Util::unsign(static_cast<int8_t>(data_.at(base))) * 0.3f));
                break;
        }
    }
    return scale > 0 ? sum / scale : 0;
}

Color WaveformDetail::segmentColor(int segment, int scale) const {
    const int limit = getFrameCount();
    switch (style_) {
        case WaveformStyle::THREE_BAND:
            throw std::runtime_error("segmentColor unsupported for three-band waveforms");
        case WaveformStyle::RGB: {
            int red = 0;
            int green = 0;
            int blue = 0;
            for (int i = segment; (i < segment + scale) && (i < limit); ++i) {
                const int bits = getColorWaveformBits(i);
                red += (bits >> 13) & 7;
                green += (bits >> 10) & 7;
                blue += (bits >> 7) & 7;
            }
            if (scale <= 0) {
                return Color{0, 0, 0, 255};
            }
            const int divisor = scale * 7;
            return Color{static_cast<uint8_t>(red * 255 / divisor),
                         static_cast<uint8_t>(blue * 255 / divisor),
                         static_cast<uint8_t>(green * 255 / divisor),
                         255};
        }
        case WaveformStyle::BLUE: {
            int sum = 0;
            for (int i = segment; (i < segment + scale) && (i < limit); ++i) {
                sum += (data_.at(i) & 0xe0) >> 5;
            }
            if (scale <= 0) {
                return Color{0, 0, 0, 255};
            }
            return COLOR_MAP.at(static_cast<size_t>(sum / scale));
        }
    }
    return Color{0, 0, 0, 255};
}

std::string WaveformDetail::toString() const {
    return std::format("WaveformDetail[dataReference={}, size:{}]",
                       dataReference.toString(), data_.size());
}

} // namespace beatlink::data
