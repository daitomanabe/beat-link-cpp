#pragma once

#include <cstdint>
#include <format>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include "DataReference.hpp"

namespace beatlink::data {

/**
 * Represents album artwork retrieved from a player.
 * Stores raw JPEG/PNG bytes and provides decoded RGBA pixel data.
 */
class AlbumArt {
public:
    /**
     * Decoded image data with RGBA pixels.
     */
    struct DecodedImage {
        std::vector<uint8_t> pixels;  // RGBA pixels (4 bytes per pixel)
        int width{0};
        int height{0};
        int channels{4};  // Always 4 (RGBA) after decoding

        bool isValid() const { return !pixels.empty() && width > 0 && height > 0; }
        size_t byteSize() const { return pixels.size(); }
    };

    AlbumArt(DataReference reference, std::vector<uint8_t> bytes)
        : artReference(std::move(reference))
        , rawBytes_(std::move(bytes))
    {
    }

    const DataReference& getArtReference() const { return artReference; }

    std::span<const uint8_t> getRawBytes() const { return rawBytes_; }
    std::vector<uint8_t> getRawBytesCopy() const { return rawBytes_; }

    /**
     * Get the size of the raw image data in bytes.
     */
    size_t getRawSize() const { return rawBytes_.size(); }

    /**
     * Detect the image format from the raw bytes.
     * @return "jpeg", "png", or "unknown"
     */
    std::string getImageFormat() const;

    /**
     * Decode the raw JPEG/PNG bytes to RGBA pixel data.
     * Uses stb_image for decoding.
     * @return Decoded image with RGBA pixels, or nullopt if decoding fails.
     */
    std::optional<DecodedImage> decode() const;

    /**
     * Decode the image and return just the RGBA pixel data.
     * Convenience method that returns empty vector on failure.
     */
    std::vector<uint8_t> getDecodedPixels() const;

    /**
     * Get the dimensions of the image without fully decoding it.
     * @return {width, height} or {0, 0} if unable to determine.
     */
    std::pair<int, int> getDimensions() const;

    std::string toString() const {
        return std::format("AlbumArt[artReference={}, size={} bytes, format={}]",
                           artReference.toString(), rawBytes_.size(), getImageFormat());
    }

    DataReference artReference;

private:
    std::vector<uint8_t> rawBytes_;
};

} // namespace beatlink::data
