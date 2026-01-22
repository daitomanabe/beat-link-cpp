#pragma once

#include <fmt/format.h>
#include <memory>
#include <string>

#include "AlbumArt.hpp"

namespace beatlink::data {

class AlbumArtUpdate {
public:
    AlbumArtUpdate(int player, std::shared_ptr<AlbumArt> art)
        : player(player)
        , art(std::move(art))
    {
    }

    std::string toString() const {
        return fmt::format("AlbumArtUpdate[player:{}, art:{}]", player,
                           art ? art->toString() : std::string("null"));
    }

    int player;
    std::shared_ptr<AlbumArt> art;
};

} // namespace beatlink::data
