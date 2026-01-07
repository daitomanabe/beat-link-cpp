#include "beatlink/data/TrackMetadata.hpp"

#include "beatlink/dbserver/NumberField.hpp"
#include "beatlink/dbserver/StringField.hpp"

#include <chrono>
#include <stdexcept>

namespace beatlink::data {

TrackMetadata::TrackMetadata(DataReference reference, TrackType trackType,
                             std::vector<beatlink::dbserver::Message> items,
                             std::shared_ptr<CueList> cueList)
    : trackReference_(std::move(reference))
    , trackType_(trackType)
    , rawItems_(std::move(items))
    , cueList_(std::move(cueList))
{
    timestampNanos_ = std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();

    for (const auto& item : rawItems_) {
        parseMetadataItem(item);
    }

    if (title_.empty()) {
        title_.clear();
    }
    if (comment_.empty()) {
        comment_.clear();
    }
    if (dateAdded_.empty()) {
        dateAdded_.clear();
    }
}

SearchableItem TrackMetadata::buildSearchableItem(const beatlink::dbserver::Message& menuItem) {
    const int id = extractNumberField(menuItem, 1);
    const auto label = extractStringField(menuItem, 3);
    return SearchableItem(id, label);
}

std::string TrackMetadata::extractStringField(const beatlink::dbserver::Message& menuItem, size_t index) {
    if (index >= menuItem.arguments.size()) {
        return {};
    }
    auto stringField = std::dynamic_pointer_cast<beatlink::dbserver::StringField>(menuItem.arguments.at(index));
    if (!stringField) {
        return {};
    }
    return stringField->getValue();
}

int TrackMetadata::extractNumberField(const beatlink::dbserver::Message& menuItem, size_t index) {
    if (index >= menuItem.arguments.size()) {
        return 0;
    }
    auto numberField = std::dynamic_pointer_cast<beatlink::dbserver::NumberField>(menuItem.arguments.at(index));
    if (!numberField) {
        return 0;
    }
    return static_cast<int>(numberField->getValue());
}

void TrackMetadata::parseMetadataItem(const beatlink::dbserver::Message& item) {
    switch (item.getMenuItemType()) {
        case beatlink::dbserver::Message::MenuItemType::TRACK_TITLE:
            title_ = extractStringField(item, 3);
            artworkId_ = extractNumberField(item, 8);
            break;

        case beatlink::dbserver::Message::MenuItemType::ARTIST:
            artist_ = buildSearchableItem(item);
            break;

        case beatlink::dbserver::Message::MenuItemType::ORIGINAL_ARTIST:
            originalArtist_ = buildSearchableItem(item);
            break;

        case beatlink::dbserver::Message::MenuItemType::REMIXER:
            remixer_ = buildSearchableItem(item);
            [[fallthrough]];

        case beatlink::dbserver::Message::MenuItemType::ALBUM_TITLE:
            album_ = buildSearchableItem(item);
            break;

        case beatlink::dbserver::Message::MenuItemType::LABEL:
            label_ = buildSearchableItem(item);
            break;

        case beatlink::dbserver::Message::MenuItemType::DURATION:
            duration_ = extractNumberField(item, 1);
            break;

        case beatlink::dbserver::Message::MenuItemType::TEMPO:
            tempo_ = extractNumberField(item, 1);
            break;

        case beatlink::dbserver::Message::MenuItemType::COMMENT:
            comment_ = extractStringField(item, 3);
            break;

        case beatlink::dbserver::Message::MenuItemType::KEY:
            key_ = buildSearchableItem(item);
            break;

        case beatlink::dbserver::Message::MenuItemType::RATING:
            rating_ = extractNumberField(item, 1);
            break;

        case beatlink::dbserver::Message::MenuItemType::COLOR_NONE:
        case beatlink::dbserver::Message::MenuItemType::COLOR_AQUA:
        case beatlink::dbserver::Message::MenuItemType::COLOR_BLUE:
        case beatlink::dbserver::Message::MenuItemType::COLOR_GREEN:
        case beatlink::dbserver::Message::MenuItemType::COLOR_ORANGE:
        case beatlink::dbserver::Message::MenuItemType::COLOR_PINK:
        case beatlink::dbserver::Message::MenuItemType::COLOR_PURPLE:
        case beatlink::dbserver::Message::MenuItemType::COLOR_RED:
        case beatlink::dbserver::Message::MenuItemType::COLOR_YELLOW: {
            const int colorId = extractNumberField(item, 1);
            const auto label = extractStringField(item, 3);
            color_ = ColorItem(colorId, label);
            break;
        }

        case beatlink::dbserver::Message::MenuItemType::GENRE:
            genre_ = buildSearchableItem(item);
            break;

        case beatlink::dbserver::Message::MenuItemType::DATE_ADDED:
            dateAdded_ = extractStringField(item, 3);
            break;

        case beatlink::dbserver::Message::MenuItemType::YEAR:
            year_ = extractNumberField(item, 1);
            break;

        case beatlink::dbserver::Message::MenuItemType::BIT_RATE:
            bitRate_ = extractNumberField(item, 1);
            break;

        default:
            break;
    }
}

bool TrackMetadata::operator==(const TrackMetadata& other) const {
    return trackReference_ == other.trackReference_ &&
           trackType_ == other.trackType_ &&
           album_ == other.album_ &&
           artist_ == other.artist_ &&
           color_ == other.color_ &&
           comment_ == other.comment_ &&
           dateAdded_ == other.dateAdded_ &&
           duration_ == other.duration_ &&
           genre_ == other.genre_ &&
           key_ == other.key_ &&
           label_ == other.label_ &&
           originalArtist_ == other.originalArtist_ &&
           rating_ == other.rating_ &&
           remixer_ == other.remixer_ &&
           tempo_ == other.tempo_ &&
           year_ == other.year_ &&
           bitRate_ == other.bitRate_ &&
           title_ == other.title_ &&
           artworkId_ == other.artworkId_;
}

std::string TrackMetadata::toString() const {
    return std::format("TrackMetadata[reference:{}, title:{}]", trackReference_.toString(), title_);
}

} // namespace beatlink::data
