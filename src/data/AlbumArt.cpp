// stb_image implementation - must be defined before include
#define STB_IMAGE_IMPLEMENTATION
#define STBI_NO_STDIO  // We use memory-based loading only
#include <stb_image.h>

#include "beatlink/data/AlbumArt.hpp"

namespace beatlink::data {

std::string AlbumArt::getImageFormat() const {
    if (rawBytes_.size() < 8) {
        return "unknown";
    }

    // Check for JPEG magic bytes (FFD8FF)
    if (rawBytes_[0] == 0xFF && rawBytes_[1] == 0xD8 && rawBytes_[2] == 0xFF) {
        return "jpeg";
    }

    // Check for PNG magic bytes (89504E470D0A1A0A)
    if (rawBytes_[0] == 0x89 && rawBytes_[1] == 0x50 && rawBytes_[2] == 0x4E &&
        rawBytes_[3] == 0x47 && rawBytes_[4] == 0x0D && rawBytes_[5] == 0x0A &&
        rawBytes_[6] == 0x1A && rawBytes_[7] == 0x0A) {
        return "png";
    }

    return "unknown";
}

std::optional<AlbumArt::DecodedImage> AlbumArt::decode() const {
    if (rawBytes_.empty()) {
        return std::nullopt;
    }

    int width = 0;
    int height = 0;
    int channels = 0;

    // Request 4 channels (RGBA) regardless of source format
    constexpr int desiredChannels = 4;

    unsigned char* pixels = stbi_load_from_memory(
        rawBytes_.data(),
        static_cast<int>(rawBytes_.size()),
        &width,
        &height,
        &channels,
        desiredChannels
    );

    if (!pixels) {
        return std::nullopt;
    }

    DecodedImage result;
    result.width = width;
    result.height = height;
    result.channels = desiredChannels;

    // Copy pixel data to vector
    const size_t pixelCount = static_cast<size_t>(width) * height * desiredChannels;
    result.pixels.assign(pixels, pixels + pixelCount);

    // Free stb_image allocated memory
    stbi_image_free(pixels);

    return result;
}

std::vector<uint8_t> AlbumArt::getDecodedPixels() const {
    auto decoded = decode();
    if (decoded) {
        return std::move(decoded->pixels);
    }
    return {};
}

std::pair<int, int> AlbumArt::getDimensions() const {
    if (rawBytes_.empty()) {
        return {0, 0};
    }

    int width = 0;
    int height = 0;
    int channels = 0;

    // Use stbi_info to get dimensions without decoding
    if (stbi_info_from_memory(
            rawBytes_.data(),
            static_cast<int>(rawBytes_.size()),
            &width,
            &height,
            &channels)) {
        return {width, height};
    }

    return {0, 0};
}

} // namespace beatlink::data
