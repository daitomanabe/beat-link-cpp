#include "beatlink/data/SlotReference.hpp"
#include "beatlink/data/DataReference.hpp"

namespace beatlink::data {

SlotReference SlotReference::getSlotReference(const DataReference& reference) {
    return SlotReference(reference.getPlayer(), reference.getSlot());
}

} // namespace beatlink::data
