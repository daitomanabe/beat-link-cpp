#pragma once

#include <format>
#include <functional>
#include <string>

#include "../CdjStatus.hpp"
#include "SlotReference.hpp"

namespace beatlink::data {

class DataReference {
public:
    DataReference(int player, TrackSourceSlot slot, int rekordboxId, TrackType trackType)
        : player_(player)
        , slot_(slot)
        , rekordboxId_(rekordboxId)
        , trackType_(trackType)
    {
        std::size_t scratch = 7;
        scratch = scratch * 31 + static_cast<std::size_t>(player_);
        scratch = scratch * 31 + static_cast<std::size_t>(slot_);
        scratch = scratch * 31 + static_cast<std::size_t>(rekordboxId_);
        hash_ = scratch * 31 + static_cast<std::size_t>(trackType_);
    }

    DataReference(int player, TrackSourceSlot slot, int rekordboxId)
        : DataReference(player, slot, rekordboxId, TrackType::REKORDBOX)
    {
    }

    DataReference(const SlotReference& slot, int rekordboxId, TrackType trackType)
        : DataReference(slot.getPlayer(), slot.getSlot(), rekordboxId, trackType)
    {
    }

    DataReference(const SlotReference& slot, int rekordboxId)
        : DataReference(slot.getPlayer(), slot.getSlot(), rekordboxId, TrackType::REKORDBOX)
    {
    }

    int getPlayer() const { return player_; }
    TrackSourceSlot getSlot() const { return slot_; }
    int getRekordboxId() const { return rekordboxId_; }
    TrackType getTrackType() const { return trackType_; }

    SlotReference getSlotReference() const {
        return SlotReference::getSlotReference(player_, slot_);
    }

    std::string toString() const {
        return std::format("DataReference[player:{}, slot:{}, rekordboxId:{}, trackType:{}]",
                           player_, static_cast<int>(slot_), rekordboxId_, static_cast<int>(trackType_));
    }

    bool operator==(const DataReference& other) const {
        return player_ == other.player_ && slot_ == other.slot_ && rekordboxId_ == other.rekordboxId_ &&
               trackType_ == other.trackType_;
    }

    bool operator!=(const DataReference& other) const {
        return !(*this == other);
    }

    std::size_t hash() const { return hash_; }

private:
    int player_;
    TrackSourceSlot slot_;
    int rekordboxId_;
    TrackType trackType_;
    std::size_t hash_;
};

struct DataReferenceHash {
    std::size_t operator()(const DataReference& ref) const {
        return ref.hash();
    }
};

} // namespace beatlink::data
