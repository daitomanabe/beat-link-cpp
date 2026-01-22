#pragma once

#include <fmt/format.h>
#include <memory>
#include <string>

#include "WaveformDetail.hpp"

namespace beatlink::data {

class WaveformDetailUpdate {
public:
    WaveformDetailUpdate(int player, std::shared_ptr<WaveformDetail> detail)
        : player(player)
        , detail(std::move(detail))
    {
    }

    std::string toString() const {
        return fmt::format("WaveformDetailUpdate[player:{}, detail:{}]", player,
                           detail ? detail->toString() : std::string("null"));
    }

    int player;
    std::shared_ptr<WaveformDetail> detail;
};

} // namespace beatlink::data
