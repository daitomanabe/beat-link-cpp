#pragma once

#include <cstdint>
#include <format>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "../CdjStatus.hpp"
#include "ColorItem.hpp"
#include "CueList.hpp"
#include "DataReference.hpp"
#include "SearchableItem.hpp"
#include "beatlink/dbserver/Message.hpp"

namespace beatlink::data {

class TrackMetadata {
public:
    explicit TrackMetadata(DataReference reference, TrackType trackType,
                           std::vector<beatlink::dbserver::Message> items,
                           std::shared_ptr<CueList> cueList);

    const DataReference& getTrackReference() const { return trackReference_; }
    TrackType getTrackType() const { return trackType_; }
    int64_t getTimestampNanos() const { return timestampNanos_; }

    const std::vector<beatlink::dbserver::Message>& getRawItems() const { return rawItems_; }

    const std::optional<SearchableItem>& getAlbum() const { return album_; }
    const std::optional<SearchableItem>& getArtist() const { return artist_; }
    const std::optional<ColorItem>& getColor() const { return color_; }
    const std::string& getComment() const { return comment_; }
    const std::string& getDateAdded() const { return dateAdded_; }
    int getDuration() const { return duration_; }
    const std::optional<SearchableItem>& getGenre() const { return genre_; }
    const std::optional<SearchableItem>& getKey() const { return key_; }
    const std::optional<SearchableItem>& getLabel() const { return label_; }
    const std::optional<SearchableItem>& getOriginalArtist() const { return originalArtist_; }
    int getRating() const { return rating_; }
    const std::optional<SearchableItem>& getRemixer() const { return remixer_; }
    int getTempo() const { return tempo_; }
    int getYear() const { return year_; }
    int getBitRate() const { return bitRate_; }
    const std::string& getTitle() const { return title_; }
    int getArtworkId() const { return artworkId_; }
    const std::shared_ptr<CueList>& getCueList() const { return cueList_; }

    bool operator==(const TrackMetadata& other) const;
    bool operator!=(const TrackMetadata& other) const { return !(*this == other); }

    std::string toString() const;

private:
    void parseMetadataItem(const beatlink::dbserver::Message& item);
    static SearchableItem buildSearchableItem(const beatlink::dbserver::Message& menuItem);
    static std::string extractStringField(const beatlink::dbserver::Message& menuItem, size_t index);
    static int extractNumberField(const beatlink::dbserver::Message& menuItem, size_t index);

    DataReference trackReference_;
    TrackType trackType_;
    int64_t timestampNanos_{0};
    std::vector<beatlink::dbserver::Message> rawItems_;

    std::optional<SearchableItem> album_;
    std::optional<SearchableItem> artist_;
    std::optional<ColorItem> color_;
    std::string comment_;
    std::string dateAdded_;
    int duration_{0};
    std::optional<SearchableItem> genre_;
    std::optional<SearchableItem> key_;
    std::optional<SearchableItem> label_;
    std::optional<SearchableItem> originalArtist_;
    int rating_{0};
    std::optional<SearchableItem> remixer_;
    int tempo_{0};
    int year_{0};
    int bitRate_{0};
    std::string title_;
    int artworkId_{0};
    std::shared_ptr<CueList> cueList_;
};

} // namespace beatlink::data
