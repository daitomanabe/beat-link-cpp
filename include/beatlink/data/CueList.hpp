#pragma once

#include <cstdint>
#include <format>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include "../Util.hpp"
#include "AnlzTypes.hpp"
#include "ColorItem.hpp"
#include "beatlink/dbserver/BinaryField.hpp"
#include "beatlink/dbserver/Message.hpp"
#include "beatlink/dbserver/NumberField.hpp"

namespace beatlink::data {

class CueList {
public:
    struct Entry {
        int hotCueNumber{0};
        bool isLoop{false};
        int64_t cuePosition{0};
        int64_t cueTime{0};
        int64_t loopPosition{0};
        int64_t loopTime{0};
        std::string comment;
        int colorId{0};
        std::optional<Color> embeddedColor;
        std::optional<Color> rekordboxColor;

        Entry() = default;
        Entry(int64_t position, std::string commentText, int colorIdValue);
        Entry(int64_t startPosition, int64_t endPosition, std::string commentText, int colorIdValue);
        Entry(int hotCue, int64_t position, std::string commentText,
              std::optional<Color> embedded, std::optional<Color> rekordbox);
        Entry(int hotCue, int64_t startPosition, int64_t endPosition, std::string commentText,
              std::optional<Color> embedded, std::optional<Color> rekordbox);

        bool operator==(const Entry& other) const;
        bool operator!=(const Entry& other) const { return !(*this == other); }

        std::string toString() const;
    };

    explicit CueList(const beatlink::dbserver::Message& message);
    CueList(std::vector<std::vector<uint8_t>> rawTags, std::vector<std::vector<uint8_t>> rawExtendedTags);

    /**
     * Construct from ANLZ cue entries (for AnlzParser)
     * @param cueEntries Vector of cue entries from ANLZ file
     * @param isHotCueList True if these are hot cues, false for memory cues
     */
    CueList(std::vector<CueEntry> cueEntries, bool isHotCueList)
        : entries_(convertFromCueEntries(std::move(cueEntries)))
    {
        (void)isHotCueList;  // Could be used for categorization if needed
    }

private:
    static std::vector<Entry> convertFromCueEntries(std::vector<CueEntry> cueEntries) {
        std::vector<Entry> entries;
        entries.reserve(cueEntries.size());
        for (auto& ce : cueEntries) {
            Entry e;
            e.hotCueNumber = ce.hotCue;
            e.isLoop = ce.isLoop;
            e.cueTime = ce.timeMs;
            e.cuePosition = ce.timeMs;  // Assuming position == time for ANLZ
            e.loopTime = ce.loopTimeMs;
            e.loopPosition = ce.loopTimeMs;
            e.comment = std::move(ce.comment);
            if (ce.colorRed != 0 || ce.colorGreen != 0 || ce.colorBlue != 0) {
                e.embeddedColor = Color{ce.colorRed, ce.colorGreen, ce.colorBlue};
            }
            entries.push_back(std::move(e));
        }
        return entries;
    }

public:

    const std::optional<beatlink::dbserver::Message>& getRawMessage() const { return rawMessage_; }
    const std::vector<std::vector<uint8_t>>& getRawTags() const { return rawTags_; }
    const std::vector<std::vector<uint8_t>>& getRawExtendedTags() const { return rawExtendedTags_; }

    const std::vector<Entry>& getEntries() const { return entries_; }

    int getHotCueCount() const;
    int getMemoryPointCount() const;

    const Entry* findEntryBefore(int64_t milliseconds) const;
    const Entry* findEntryAfter(int64_t milliseconds) const;

    std::string toString() const;

    static std::optional<Color> findRekordboxColor(int colorCode);

private:
    static std::string decodeUtf16Le(std::span<const uint8_t> bytes);
    static std::string trimString(const std::string& value);
    static std::optional<Color> expectedEmbeddedColor(int colorCode);

    std::vector<Entry> parseNexusEntries(const beatlink::dbserver::Message& message) const;
    std::vector<Entry> parseNxs2Entries(const beatlink::dbserver::Message& message) const;
    static int safelyFetchColorByte(const std::vector<uint8_t>& entryBytes, size_t address);
    static std::vector<Entry> sortEntries(std::vector<Entry> entries);

    std::optional<beatlink::dbserver::Message> rawMessage_;
    std::vector<std::vector<uint8_t>> rawTags_;
    std::vector<std::vector<uint8_t>> rawExtendedTags_;
    std::vector<Entry> entries_;
};

} // namespace beatlink::data
