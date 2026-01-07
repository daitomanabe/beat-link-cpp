#include "beatlink/MediaDetails.hpp"

#include <algorithm>
#include <cctype>
#include <format>
#include <iostream>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <unordered_set>

namespace beatlink {
namespace {

std::optional<TrackSourceSlot> trackSourceSlotFromProtocol(uint8_t value) {
    switch (value) {
        case 0:
            return TrackSourceSlot::NO_TRACK;
        case 1:
            return TrackSourceSlot::CD_SLOT;
        case 2:
            return TrackSourceSlot::SD_SLOT;
        case 3:
            return TrackSourceSlot::USB_SLOT;
        case 4:
            return TrackSourceSlot::COLLECTION;
        case 7:
            return TrackSourceSlot::USB_2_SLOT;
        case 0xff:
            return TrackSourceSlot::UNKNOWN;
        default:
            return std::nullopt;
    }
}

std::optional<TrackType> trackTypeFromProtocol(uint8_t value) {
    switch (value) {
        case 0:
            return TrackType::NO_TRACK;
        case 1:
            return TrackType::REKORDBOX;
        case 2:
            return TrackType::UNANALYZED;
        case 5:
            return TrackType::CD_DIGITAL_AUDIO;
        case 0xff:
            return TrackType::UNKNOWN;
        default:
            return std::nullopt;
    }
}

size_t utf16StringLength(std::span<const uint8_t> data, size_t offset, size_t maxLength) {
    if (offset >= data.size()) {
        return 0;
    }
    size_t clampedMax = std::min(maxLength, data.size() - offset);
    size_t length = clampedMax;
    for (size_t i = offset; i + 1 < offset + clampedMax; i += 2) {
        if (data[i] == 0x00 && data[i + 1] == 0x00) {
            length = i - offset;
            break;
        }
    }
    return length;
}

void appendUtf8(std::string& out, uint32_t codepoint) {
    if (codepoint <= 0x7f) {
        out.push_back(static_cast<char>(codepoint));
    } else if (codepoint <= 0x7ff) {
        out.push_back(static_cast<char>(0xc0 | ((codepoint >> 6) & 0x1f)));
        out.push_back(static_cast<char>(0x80 | (codepoint & 0x3f)));
    } else if (codepoint <= 0xffff) {
        out.push_back(static_cast<char>(0xe0 | ((codepoint >> 12) & 0x0f)));
        out.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3f)));
        out.push_back(static_cast<char>(0x80 | (codepoint & 0x3f)));
    } else {
        out.push_back(static_cast<char>(0xf0 | ((codepoint >> 18) & 0x07)));
        out.push_back(static_cast<char>(0x80 | ((codepoint >> 12) & 0x3f)));
        out.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3f)));
        out.push_back(static_cast<char>(0x80 | (codepoint & 0x3f)));
    }
}

std::string decodeUtf16Be(std::span<const uint8_t> data) {
    std::string out;
    for (size_t i = 0; i + 1 < data.size(); i += 2) {
        uint16_t unit = static_cast<uint16_t>((data[i] << 8) | data[i + 1]);
        if (unit == 0) {
            break;
        }
        if (unit >= 0xd800 && unit <= 0xdbff) {
            if (i + 3 < data.size()) {
                uint16_t low = static_cast<uint16_t>((data[i + 2] << 8) | data[i + 3]);
                if (low >= 0xdc00 && low <= 0xdfff) {
                    uint32_t codepoint = 0x10000 + ((unit - 0xd800) << 10) + (low - 0xdc00);
                    appendUtf8(out, codepoint);
                    i += 2;
                    continue;
                }
            }
            appendUtf8(out, 0xfffd);
            continue;
        }
        if (unit >= 0xdc00 && unit <= 0xdfff) {
            appendUtf8(out, 0xfffd);
            continue;
        }
        appendUtf8(out, unit);
    }
    return out;
}

std::string trimAscii(std::string value) {
    auto is_trim = [](unsigned char c) { return c <= 0x20; };
    value.erase(value.begin(),
                std::find_if(value.begin(), value.end(), [&](unsigned char c) { return !is_trim(c); }));
    value.erase(std::find_if(value.rbegin(), value.rend(),
                             [&](unsigned char c) { return !is_trim(c); })
                    .base(),
                value.end());
    return value;
}

std::unordered_set<size_t> expectedMediaPacketSizes = {0xc0};
std::mutex expectedMediaPacketSizesMutex;

} // namespace

MediaDetails::MediaDetails(const uint8_t* packet, size_t length)
    : slotReference_(data::SlotReference::getSlotReference(0, TrackSourceSlot::NO_TRACK))
    , mediaType_(TrackType::NO_TRACK)
{
    rawBytes_.assign(packet, packet + length);

    if (rawBytes_.size() < MINIMUM_PACKET_SIZE) {
        throw std::invalid_argument(
            std::format("Unable to create MediaDetails, packet too short: need {} bytes, got {}",
                        MINIMUM_PACKET_SIZE, rawBytes_.size()));
    }

    const int payloadLength = static_cast<int>(Util::bytesToNumber(rawBytes_, 0x22, 2));
    if (rawBytes_.size() != static_cast<size_t>(payloadLength + 0x24)) {
        std::cerr << "Media response packet reported payload length " << payloadLength
                  << " but actual payload length was " << (rawBytes_.size() - 0x24) << ".\n";
    }

    {
        std::lock_guard<std::mutex> lock(expectedMediaPacketSizesMutex);
        if (!expectedMediaPacketSizes.contains(rawBytes_.size())) {
            std::cerr << "Processing Media response packets with unexpected length " << rawBytes_.size() << ".\n";
            expectedMediaPacketSizes.insert(rawBytes_.size());
        }
    }

    const int hostPlayer = rawBytes_[0x27];
    auto slot = trackSourceSlotFromProtocol(rawBytes_[0x2b]);
    if (!slot) {
        throw std::invalid_argument(
            std::format("Unrecognized slot for media response: {}", rawBytes_[0x2b]));
    }
    slotReference_ = data::SlotReference::getSlotReference(hostPlayer, *slot);

    auto type = trackTypeFromProtocol(rawBytes_[0xaa]);
    if (!type) {
        throw std::invalid_argument(
            std::format("Unrecognized media type for media response: {}", rawBytes_[0xaa]));
    }
    mediaType_ = *type;

    if (hostPlayer >= 40) {
        name_ = "rekordbox mobile";
        creationDate_.clear();
    } else {
        const auto dataSpan = std::span<const uint8_t>(rawBytes_.data(), rawBytes_.size());
        const size_t nameLen = utf16StringLength(dataSpan, 0x2c, 0x40);
        const size_t dateLen = utf16StringLength(dataSpan, 0x6c, 0x18);
        name_ = trimAscii(decodeUtf16Be(dataSpan.subspan(0x2c, nameLen)));
        creationDate_ = trimAscii(decodeUtf16Be(dataSpan.subspan(0x6c, dateLen)));
    }

    trackCount_ = static_cast<int>(Util::bytesToNumber(rawBytes_, 0xa6, 2));
    color_ = data::ColorItem::colorForId(rawBytes_[0xa8]);
    playlistCount_ = static_cast<int>(Util::bytesToNumber(rawBytes_, 0xae, 2));
    hasMySettings_ = rawBytes_[0xab] != 0;
    totalSize_ = Util::bytesToNumber(rawBytes_, 0xb0, 8);
    freeSpace_ = Util::bytesToNumber(rawBytes_, 0xb8, 8);
}

MediaDetails::MediaDetails(std::span<const uint8_t> packet)
    : MediaDetails(packet.data(), packet.size())
{
}

MediaDetails::MediaDetails(const data::SlotReference& slotReference,
                           TrackType mediaType,
                           std::string name,
                           int trackCount,
                           int playlistCount,
                           int64_t lastModified)
    : slotReference_(slotReference)
    , mediaType_(mediaType)
    , name_(std::move(name))
    , creationDate_(std::to_string(lastModified))
    , trackCount_(trackCount)
    , playlistCount_(playlistCount)
    , color_(data::ColorItem::colorForId(0))
    , hasMySettings_(false)
    , totalSize_(0)
    , freeSpace_(0)
{
    if (!data::OpusProvider::getInstance().isRunning()) {
        throw std::runtime_error("OpusProvider must be started to use this constructor");
    }
}

std::string MediaDetails::toString() const {
    return std::format(
        "MediaDetails[slotReference:{}, name:{}, creationDate:{}, mediaType:{}, trackCount:{}, "
        "playlistCount:{}, totalSize:{}, freeSpace:{}]",
        slotReference_.toString(), name_, creationDate_, static_cast<int>(mediaType_), trackCount_, playlistCount_,
        totalSize_, freeSpace_);
}

std::string MediaDetails::hashKey() const {
    return creationDate_ + ":" + std::to_string(static_cast<int>(mediaType_)) + ":" + std::to_string(totalSize_) +
           ":" + name_;
}

bool MediaDetails::hasChanged(const MediaDetails& original) const {
    if (hashKey() != original.hashKey()) {
        throw std::invalid_argument("Can't compare media details with different hashKey values");
    }
    return playlistCount_ != original.playlistCount_ || trackCount_ != original.trackCount_;
}

} // namespace beatlink
