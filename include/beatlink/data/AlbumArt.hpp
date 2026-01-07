#pragma once

#include <format>
#include <span>
#include <string>
#include <vector>

#include "DataReference.hpp"

namespace beatlink::data {

class AlbumArt {
public:
    AlbumArt(DataReference reference, std::vector<uint8_t> bytes)
        : artReference(std::move(reference))
        , rawBytes_(std::move(bytes))
    {
    }

    const DataReference& getArtReference() const { return artReference; }

    std::span<const uint8_t> getRawBytes() const { return rawBytes_; }
    std::vector<uint8_t> getRawBytesCopy() const { return rawBytes_; }

    std::string toString() const {
        return std::format("AlbumArt[artReference={}, size={} bytes]",
                           artReference.toString(), rawBytes_.size());
    }

    DataReference artReference;

private:
    std::vector<uint8_t> rawBytes_;
};

} // namespace beatlink::data
