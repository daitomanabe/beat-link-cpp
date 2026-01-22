#pragma once

#include <fmt/format.h>
#include <functional>
#include <string>

#include "../CdjStatus.hpp"

namespace beatlink::data {

class DataReference;

class SlotReference {
public:
    SlotReference(int player, TrackSourceSlot slot)
        : player_(player)
        , slot_(slot)
    {
    }

    int getPlayer() const { return player_; }
    TrackSourceSlot getSlot() const { return slot_; }

    static SlotReference getSlotReference(int player, TrackSourceSlot slot) {
        return SlotReference(player, slot);
    }

    static SlotReference getSlotReference(const class DataReference& reference);

    std::string toString() const {
        return fmt::format("SlotReference[player:{}, slot:{}]", player_, static_cast<int>(slot_));
    }

    bool operator==(const SlotReference& other) const {
        return player_ == other.player_ && slot_ == other.slot_;
    }

    bool operator!=(const SlotReference& other) const {
        return !(*this == other);
    }

private:
    int player_;
    TrackSourceSlot slot_;
};

struct SlotReferenceHash {
    std::size_t operator()(const SlotReference& ref) const {
        return std::hash<int>{}(ref.getPlayer()) ^ (std::hash<int>{}(static_cast<int>(ref.getSlot())) << 1);
    }
};

} // namespace beatlink::data
