#pragma once

#include <format>
#include <memory>
#include <string>

#include "TrackMetadata.hpp"

namespace beatlink::data {

class TrackMetadataUpdate {
public:
    TrackMetadataUpdate(int player, std::shared_ptr<TrackMetadata> metadata)
        : player(player)
        , metadata(std::move(metadata))
    {
    }

    std::string toString() const {
        return std::format("TrackMetadataUpdate[player:{}, metadata:{}]", player,
                           metadata ? metadata->toString() : std::string("null"));
    }

    int player;
    std::shared_ptr<TrackMetadata> metadata;
};

} // namespace beatlink::data
