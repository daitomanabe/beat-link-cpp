#pragma once

#include <format>
#include <memory>
#include <string>

#include "WaveformPreview.hpp"

namespace beatlink::data {

class WaveformPreviewUpdate {
public:
    WaveformPreviewUpdate(int player, std::shared_ptr<WaveformPreview> preview)
        : player(player)
        , preview(std::move(preview))
    {
    }

    std::string toString() const {
        return std::format("WaveformPreviewUpdate[player:{}, preview:{}]", player,
                           preview ? preview->toString() : std::string("null"));
    }

    int player;
    std::shared_ptr<WaveformPreview> preview;
};

} // namespace beatlink::data
