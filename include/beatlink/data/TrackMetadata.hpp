#pragma once

#include <cstdint>
#include <fmt/format.h>
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

    /**
     * Builder for constructing TrackMetadata from PDB data
     */
    class Builder {
    public:
        Builder& setTrackReference(DataReference ref) { trackReference_ = std::move(ref); return *this; }
        Builder& setTrackType(TrackType type) { trackType_ = type; return *this; }
        Builder& setTitle(std::string title) { title_ = std::move(title); return *this; }
        Builder& setArtist(std::string name, int id = 0) {
            artist_ = SearchableItem{id, std::move(name)};
            return *this;
        }
        Builder& setAlbum(std::string name, int id = 0) {
            album_ = SearchableItem{id, std::move(name)};
            return *this;
        }
        Builder& setGenre(std::string name, int id = 0) {
            genre_ = SearchableItem{id, std::move(name)};
            return *this;
        }
        Builder& setLabel(std::string name, int id = 0) {
            label_ = SearchableItem{id, std::move(name)};
            return *this;
        }
        Builder& setKey(std::string name, int id = 0) {
            key_ = SearchableItem{id, std::move(name)};
            return *this;
        }
        Builder& setComment(std::string comment) { comment_ = std::move(comment); return *this; }
        Builder& setDateAdded(std::string date) { dateAdded_ = std::move(date); return *this; }
        Builder& setDuration(int seconds) { duration_ = seconds; return *this; }
        Builder& setTempo(int bpmTimes100) { tempo_ = bpmTimes100; return *this; }
        Builder& setRating(int rating) { rating_ = rating; return *this; }
        Builder& setYear(int year) { year_ = year; return *this; }
        Builder& setBitRate(int kbps) { bitRate_ = kbps; return *this; }
        Builder& setArtworkId(int id) { artworkId_ = id; return *this; }
        Builder& setColor(int colorId) {
            color_ = ColorItem{colorId, ColorItem::colorNameForId(colorId)};
            return *this;
        }
        Builder& setCueList(std::shared_ptr<CueList> cues) { cueList_ = std::move(cues); return *this; }

        std::shared_ptr<TrackMetadata> build();

    private:
        DataReference trackReference_;
        TrackType trackType_{TrackType::REKORDBOX};
        std::string title_;
        std::optional<SearchableItem> artist_;
        std::optional<SearchableItem> album_;
        std::optional<SearchableItem> genre_;
        std::optional<SearchableItem> label_;
        std::optional<SearchableItem> key_;
        std::string comment_;
        std::string dateAdded_;
        int duration_{0};
        int tempo_{0};
        int rating_{0};
        int year_{0};
        int bitRate_{0};
        int artworkId_{0};
        std::optional<ColorItem> color_;
        std::shared_ptr<CueList> cueList_;
    };

    // Private constructor for Builder
    friend class Builder;

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
    // Private constructor for Builder
    TrackMetadata() = default;

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
