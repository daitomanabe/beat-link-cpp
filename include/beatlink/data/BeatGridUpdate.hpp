#pragma once

#include <format>
#include <memory>
#include <string>

#include "BeatGrid.hpp"

namespace beatlink::data {

class BeatGridUpdate {
public:
    BeatGridUpdate(int player, std::shared_ptr<BeatGrid> beatGrid)
        : player(player)
        , beatGrid(std::move(beatGrid))
    {
    }

    std::string toString() const {
        return std::format("BeatGridUpdate[player:{}, beatGrid:{}]", player,
                           beatGrid ? beatGrid->toString() : std::string("null"));
    }

    int player;
    std::shared_ptr<BeatGrid> beatGrid;
};

} // namespace beatlink::data
