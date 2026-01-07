#pragma once

#include "ColorItem.hpp"

namespace beatlink::data {

enum class WaveformStyle {
    BLUE,
    RGB,
    THREE_BAND
};

enum class ThreeBandLayer {
    LOW,
    LOW_AND_MID,
    MID,
    HIGH
};

inline Color threeBandLayerColor(ThreeBandLayer layer) {
    switch (layer) {
        case ThreeBandLayer::LOW:
            return Color{32, 83, 217, 255};
        case ThreeBandLayer::LOW_AND_MID:
            return Color{169, 107, 39, 255};
        case ThreeBandLayer::MID:
            return Color{242, 170, 60, 255};
        case ThreeBandLayer::HIGH:
            return Color{255, 255, 255, 255};
    }
    return Color{255, 255, 255, 255};
}

} // namespace beatlink::data
