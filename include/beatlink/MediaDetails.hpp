#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <vector>

#include "CdjStatus.hpp"
#include "Util.hpp"
#include "data/ColorItem.hpp"
#include "data/OpusProvider.hpp"
#include "data/SlotReference.hpp"

namespace beatlink {

class MediaDetails {
public:
    static constexpr size_t MINIMUM_PACKET_SIZE = 0xc0;

    MediaDetails(const uint8_t* packet, size_t length);
    explicit MediaDetails(std::span<const uint8_t> packet);
    MediaDetails(const data::SlotReference& slotReference,
                 TrackType mediaType,
                 std::string name,
                 int trackCount,
                 int playlistCount,
                 int64_t lastModified);

    const data::SlotReference& getSlotReference() const { return slotReference_; }
    TrackType getMediaType() const { return mediaType_; }
    const std::string& getName() const { return name_; }
    const std::string& getCreationDate() const { return creationDate_; }
    int getTrackCount() const { return trackCount_; }
    int getPlaylistCount() const { return playlistCount_; }
    const data::Color& getColor() const { return color_; }
    bool hasMySettings() const { return hasMySettings_; }
    int64_t getTotalSize() const { return totalSize_; }
    int64_t getFreeSpace() const { return freeSpace_; }

    std::span<const uint8_t> getRawBytes() const { return rawBytes_; }

    std::string toString() const;
    std::string hashKey() const;
    bool hasChanged(const MediaDetails& original) const;

private:
    data::SlotReference slotReference_;
    TrackType mediaType_;
    std::string name_;
    std::string creationDate_;
    int trackCount_{0};
    int playlistCount_{0};
    data::Color color_{};
    bool hasMySettings_{false};
    int64_t totalSize_{0};
    int64_t freeSpace_{0};
    std::vector<uint8_t> rawBytes_;
};

} // namespace beatlink
