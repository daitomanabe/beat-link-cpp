#include "beatlink/data/CueList.hpp"

#include <algorithm>
#include <cctype>
#include <codecvt>
#include <locale>
#include <stdexcept>

namespace beatlink::data {

CueList::Entry::Entry(int64_t position, std::string commentText, int colorIdValue)
    : hotCueNumber(0)
    , isLoop(false)
    , cuePosition(position)
    , cueTime(beatlink::Util::halfFrameToTime(position))
    , loopPosition(0)
    , loopTime(0)
    , comment(CueList::trimString(commentText))
    , colorId(colorIdValue)
    , embeddedColor(std::nullopt)
    , rekordboxColor(std::nullopt)
{
}

CueList::Entry::Entry(int64_t startPosition, int64_t endPosition, std::string commentText, int colorIdValue)
    : hotCueNumber(0)
    , isLoop(true)
    , cuePosition(startPosition)
    , cueTime(beatlink::Util::halfFrameToTime(startPosition))
    , loopPosition(endPosition)
    , loopTime(beatlink::Util::halfFrameToTime(endPosition))
    , comment(CueList::trimString(commentText))
    , colorId(colorIdValue)
    , embeddedColor(std::nullopt)
    , rekordboxColor(std::nullopt)
{
}

CueList::Entry::Entry(int hotCue, int64_t position, std::string commentText,
                      std::optional<Color> embedded, std::optional<Color> rekordbox)
    : hotCueNumber(hotCue)
    , isLoop(false)
    , cuePosition(position)
    , cueTime(beatlink::Util::halfFrameToTime(position))
    , loopPosition(0)
    , loopTime(0)
    , comment(CueList::trimString(commentText))
    , colorId(0)
    , embeddedColor(embedded)
    , rekordboxColor(rekordbox)
{
    if (hotCue == 0) {
        throw std::invalid_argument("Hot cues must have non-zero numbers");
    }
}

CueList::Entry::Entry(int hotCue, int64_t startPosition, int64_t endPosition, std::string commentText,
                      std::optional<Color> embedded, std::optional<Color> rekordbox)
    : hotCueNumber(hotCue)
    , isLoop(true)
    , cuePosition(startPosition)
    , cueTime(beatlink::Util::halfFrameToTime(startPosition))
    , loopPosition(endPosition)
    , loopTime(beatlink::Util::halfFrameToTime(endPosition))
    , comment(CueList::trimString(commentText))
    , colorId(0)
    , embeddedColor(embedded)
    , rekordboxColor(rekordbox)
{
    if (hotCue == 0) {
        throw std::invalid_argument("Hot cues must have non-zero numbers");
    }
}

bool CueList::Entry::operator==(const Entry& other) const {
    return hotCueNumber == other.hotCueNumber &&
           isLoop == other.isLoop &&
           cuePosition == other.cuePosition &&
           cueTime == other.cueTime &&
           loopPosition == other.loopPosition &&
           loopTime == other.loopTime &&
           comment == other.comment &&
           colorId == other.colorId &&
           embeddedColor == other.embeddedColor &&
           rekordboxColor == other.rekordboxColor;
}

std::string CueList::Entry::toString() const {
    return std::format("CueEntry[hotCue:{}, isLoop:{}, cuePos:{}, loopPos:{}, comment:{}]",
                       hotCueNumber, isLoop, cuePosition, loopPosition, comment);
}

CueList::CueList(const beatlink::dbserver::Message& message)
    : rawMessage_(message)
{
    if (message.knownType && *message.knownType == beatlink::dbserver::Message::KnownType::CUE_LIST) {
        entries_ = parseNexusEntries(message);
    } else if (message.knownType && *message.knownType == beatlink::dbserver::Message::KnownType::CUE_LIST_EXT) {
        entries_ = parseNxs2Entries(message);
    } else {
        throw std::invalid_argument("CueList expects CUE_LIST or CUE_LIST_EXT message");
    }
}

CueList::CueList(std::vector<std::vector<uint8_t>> rawTags, std::vector<std::vector<uint8_t>> rawExtendedTags)
    : rawMessage_(std::nullopt)
    , rawTags_(std::move(rawTags))
    , rawExtendedTags_(std::move(rawExtendedTags))
{
    entries_.clear();
}

int CueList::getHotCueCount() const {
    if (rawMessage_) {
        const auto& message = *rawMessage_;
        if (message.arguments.size() > 5) {
            if (auto number = std::dynamic_pointer_cast<beatlink::dbserver::NumberField>(message.arguments.at(5))) {
                return static_cast<int>(number->getValue());
            }
        }
    }
    int total = 0;
    for (const auto& entry : entries_) {
        if (entry.hotCueNumber > 0) {
            ++total;
        }
    }
    return total;
}

int CueList::getMemoryPointCount() const {
    if (rawMessage_) {
        const auto& message = *rawMessage_;
        if (message.arguments.size() > 6) {
            if (auto number = std::dynamic_pointer_cast<beatlink::dbserver::NumberField>(message.arguments.at(6))) {
                return static_cast<int>(number->getValue());
            }
        }
    }
    int total = 0;
    for (const auto& entry : entries_) {
        if (entry.hotCueNumber == 0) {
            ++total;
        }
    }
    return total;
}

const CueList::Entry* CueList::findEntryBefore(int64_t milliseconds) const {
    const int targetPosition = beatlink::Util::timeToHalfFrameRounded(milliseconds);
    const auto it = std::lower_bound(entries_.begin(), entries_.end(), targetPosition,
                                     [](const Entry& entry, int position) {
                                         return entry.cuePosition < position;
                                     });
    if (it == entries_.end()) {
        return entries_.empty() ? nullptr : &entries_.back();
    }
    if (it->cuePosition == targetPosition) {
        return &(*it);
    }
    if (it == entries_.begin()) {
        return nullptr;
    }
    return &(*(it - 1));
}

const CueList::Entry* CueList::findEntryAfter(int64_t milliseconds) const {
    const int targetPosition = beatlink::Util::timeToHalfFrameRounded(milliseconds);
    const auto it = std::lower_bound(entries_.begin(), entries_.end(), targetPosition,
                                     [](const Entry& entry, int position) {
                                         return entry.cuePosition < position;
                                     });
    if (it == entries_.end()) {
        return nullptr;
    }
    return &(*it);
}

std::string CueList::toString() const {
    return std::format("CueList[entries:{}]", entries_.size());
}

std::optional<Color> CueList::expectedEmbeddedColor(int colorCode) {
    if (colorCode == 0) {
        return Color{0, 255, 0, 255};
    }
    return findRekordboxColor(colorCode);
}

std::optional<Color> CueList::findRekordboxColor(int colorCode) {
    switch (colorCode) {
        case 0x01: return Color{0x30, 0x5a, 0xff, 0xff};
        case 0x02: return Color{0x50, 0x73, 0xff, 0xff};
        case 0x03: return Color{0x50, 0x8c, 0xff, 0xff};
        case 0x04: return Color{0x50, 0xa0, 0xff, 0xff};
        case 0x05: return Color{0x50, 0xb4, 0xff, 0xff};
        case 0x06: return Color{0x50, 0xb0, 0xf2, 0xff};
        case 0x07: return Color{0x50, 0xae, 0xe8, 0xff};
        case 0x08: return Color{0x45, 0xac, 0xdb, 0xff};
        case 0x09: return Color{0x00, 0xe0, 0xff, 0xff};
        case 0x0a: return Color{0x19, 0xda, 0xf0, 0xff};
        case 0x0b: return Color{0x32, 0xd2, 0xe6, 0xff};
        case 0x0c: return Color{0x21, 0xb4, 0xb9, 0xff};
        case 0x0d: return Color{0x20, 0xaa, 0xa0, 0xff};
        case 0x0e: return Color{0x1f, 0xa3, 0x92, 0xff};
        case 0x0f: return Color{0x19, 0xa0, 0x8c, 0xff};
        case 0x10: return Color{0x14, 0xa5, 0x84, 0xff};
        case 0x11: return Color{0x14, 0xaa, 0x7d, 0xff};
        case 0x12: return Color{0x10, 0xb1, 0x76, 0xff};
        case 0x13: return Color{0x30, 0xd2, 0x6e, 0xff};
        case 0x14: return Color{0x37, 0xde, 0x5a, 0xff};
        case 0x15: return Color{0x3c, 0xeb, 0x50, 0xff};
        case 0x16: return Color{0x28, 0xe2, 0x14, 0xff};
        case 0x17: return Color{0x7d, 0xc1, 0x3d, 0xff};
        case 0x18: return Color{0x8c, 0xc8, 0x32, 0xff};
        case 0x19: return Color{0x9b, 0xd7, 0x23, 0xff};
        case 0x1a: return Color{0xa5, 0xe1, 0x16, 0xff};
        case 0x1b: return Color{0xa5, 0xdc, 0x0a, 0xff};
        case 0x1c: return Color{0xaa, 0xd2, 0x08, 0xff};
        case 0x1d: return Color{0xb4, 0xc8, 0x05, 0xff};
        case 0x1e: return Color{0xb4, 0xbe, 0x04, 0xff};
        case 0x1f: return Color{0xba, 0xb4, 0x04, 0xff};
        case 0x20: return Color{0xc3, 0xaf, 0x04, 0xff};
        case 0x21: return Color{0xe1, 0xaa, 0x00, 0xff};
        case 0x22: return Color{0xff, 0xa0, 0x00, 0xff};
        case 0x23: return Color{0xff, 0x96, 0x00, 0xff};
        case 0x24: return Color{0xff, 0x8c, 0x00, 0xff};
        case 0x25: return Color{0xff, 0x75, 0x00, 0xff};
        case 0x26: return Color{0xe0, 0x64, 0x1b, 0xff};
        case 0x27: return Color{0xe0, 0x46, 0x1e, 0xff};
        case 0x28: return Color{0xe0, 0x30, 0x1e, 0xff};
        case 0x29: return Color{0xe0, 0x28, 0x23, 0xff};
        case 0x2a: return Color{0xe6, 0x28, 0x28, 0xff};
        case 0x2b: return Color{0xff, 0x37, 0x6f, 0xff};
        case 0x2c: return Color{0xff, 0x2d, 0x6f, 0xff};
        case 0x2d: return Color{0xff, 0x12, 0x7b, 0xff};
        case 0x2e: return Color{0xf5, 0x1e, 0x8c, 0xff};
        case 0x2f: return Color{0xeb, 0x2d, 0xa0, 0xff};
        case 0x30: return Color{0xe6, 0x37, 0xb4, 0xff};
        case 0x31: return Color{0xde, 0x44, 0xcf, 0xff};
        case 0x32: return Color{0xde, 0x44, 0x8d, 0xff};
        case 0x33: return Color{0xe6, 0x30, 0xb4, 0xff};
        case 0x34: return Color{0xe6, 0x19, 0xdc, 0xff};
        case 0x35: return Color{0xe6, 0x00, 0xff, 0xff};
        case 0x36: return Color{0xdc, 0x00, 0xff, 0xff};
        case 0x37: return Color{0xcc, 0x00, 0xff, 0xff};
        case 0x38: return Color{0xb4, 0x32, 0xff, 0xff};
        case 0x39: return Color{0xb9, 0x3c, 0xff, 0xff};
        case 0x3a: return Color{0xc5, 0x42, 0xff, 0xff};
        case 0x3b: return Color{0xaa, 0x5a, 0xff, 0xff};
        case 0x3c: return Color{0xaa, 0x72, 0xff, 0xff};
        case 0x3d: return Color{0x82, 0x72, 0xff, 0xff};
        case 0x3e: return Color{0x64, 0x73, 0xff, 0xff};
        case 0x00:
            return std::nullopt;
        default:
            return std::nullopt;
    }
}

std::string CueList::decodeUtf16Le(std::span<const uint8_t> bytes) {
    if (bytes.empty()) {
        return {};
    }
    if (bytes.size() % 2 != 0) {
        throw std::runtime_error("Invalid UTF-16LE string length");
    }
    std::u16string utf16;
    utf16.resize(bytes.size() / 2);
    for (size_t i = 0; i < utf16.size(); ++i) {
        const uint16_t value = static_cast<uint16_t>(bytes[i * 2]) |
                               static_cast<uint16_t>(bytes[i * 2 + 1] << 8);
        utf16[i] = static_cast<char16_t>(value);
    }
    std::wstring_convert<std::codecvt_utf8_utf16<char16_t>, char16_t> converter;
    return converter.to_bytes(utf16);
}

std::string CueList::trimString(const std::string& value) {
    size_t start = 0;
    while (start < value.size() && std::isspace(static_cast<unsigned char>(value[start]))) {
        ++start;
    }
    size_t end = value.size();
    while (end > start && std::isspace(static_cast<unsigned char>(value[end - 1]))) {
        --end;
    }
    return value.substr(start, end - start);
}

int CueList::safelyFetchColorByte(const std::vector<uint8_t>& entryBytes, size_t address) {
    if (address < entryBytes.size()) {
        return static_cast<int>(beatlink::Util::unsign(static_cast<int8_t>(entryBytes[address])));
    }
    return 0;
}

std::vector<CueList::Entry> CueList::parseNexusEntries(const beatlink::dbserver::Message& message) const {
    auto blobField = std::dynamic_pointer_cast<beatlink::dbserver::BinaryField>(message.arguments.at(3));
    if (!blobField) {
        throw std::runtime_error("Cue list response missing binary field");
    }
    const auto entryBytes = blobField->getValueAsArray();
    const int entryCount = static_cast<int>(entryBytes.size() / 36);
    std::vector<Entry> entries;
    entries.reserve(static_cast<size_t>(entryCount));

    for (int i = 0; i < entryCount; ++i) {
        const size_t offset = static_cast<size_t>(i) * 36;
        const int cueFlag = entryBytes[offset + 1];
        const int hotCueNumber = entryBytes[offset + 2];
        if (cueFlag != 0 || hotCueNumber != 0) {
            const int64_t position = beatlink::Util::bytesToNumberLittleEndian(entryBytes.data(), offset + 12, 4);
            if (entryBytes[offset] != 0) {
                const int64_t endPosition = beatlink::Util::bytesToNumberLittleEndian(entryBytes.data(), offset + 16, 4);
                if (hotCueNumber == 0) {
                    entries.emplace_back(position, endPosition, "", 0);
                } else {
                    entries.emplace_back(hotCueNumber, position, endPosition, "", std::nullopt, std::nullopt);
                }
            } else {
                if (hotCueNumber == 0) {
                    entries.emplace_back(position, "", 0);
                } else {
                    entries.emplace_back(hotCueNumber, position, "", std::nullopt, std::nullopt);
                }
            }
        }
    }
    return sortEntries(std::move(entries));
}

std::vector<CueList::Entry> CueList::parseNxs2Entries(const beatlink::dbserver::Message& message) const {
    auto blobField = std::dynamic_pointer_cast<beatlink::dbserver::BinaryField>(message.arguments.at(3));
    auto countField = std::dynamic_pointer_cast<beatlink::dbserver::NumberField>(message.arguments.at(4));
    if (!blobField || !countField) {
        throw std::runtime_error("Extended cue list response missing fields");
    }
    const auto entryBytes = blobField->getValueAsArray();
    const int entryCount = static_cast<int>(countField->getValue());
    std::vector<Entry> entries;
    entries.reserve(static_cast<size_t>(entryCount));

    size_t offset = 0;
    for (int i = 0; i < entryCount; ++i) {
        if (offset + 8 > entryBytes.size()) {
            break;
        }
        const int entrySize = static_cast<int>(beatlink::Util::bytesToNumberLittleEndian(entryBytes.data(), offset, 4));
        const int cueFlag = entryBytes[offset + 6];
        const int hotCueNumber = entryBytes[offset + 4];

        if (cueFlag != 0 || hotCueNumber != 0) {
            const int64_t position = beatlink::Util::timeToHalfFrame(
                beatlink::Util::bytesToNumberLittleEndian(entryBytes.data(), offset + 12, 4));

            std::string comment;
            int commentSize = 0;
            if (entrySize > 0x49 && offset + 0x4a <= entryBytes.size()) {
                commentSize = static_cast<int>(beatlink::Util::bytesToNumberLittleEndian(entryBytes.data(), offset + 0x48, 2));
            }
            if (commentSize > 0 && offset + 0x4a + static_cast<size_t>(commentSize) <= entryBytes.size()) {
                comment = decodeUtf16Le(std::span<const uint8_t>(entryBytes.data() + offset + 0x4a,
                                                                static_cast<size_t>(commentSize - 2)));
            }

            if (hotCueNumber == 0) {
                const int colorId = entryBytes[offset + 0x22];
                if (cueFlag == 2) {
                    const int64_t endPosition = beatlink::Util::timeToHalfFrame(
                        beatlink::Util::bytesToNumberLittleEndian(entryBytes.data(), offset + 16, 4));
                    entries.emplace_back(position, endPosition, comment, colorId);
                } else {
                    entries.emplace_back(position, comment, colorId);
                }
            } else {
                const size_t base = offset + static_cast<size_t>(commentSize) + 0x4e;
                const int colorCode = safelyFetchColorByte(entryBytes, base);
                const int red = safelyFetchColorByte(entryBytes, base + 1);
                const int green = safelyFetchColorByte(entryBytes, base + 2);
                const int blue = safelyFetchColorByte(entryBytes, base + 3);

                std::optional<Color> embedded;
                if (!(red == 0 && green == 0 && blue == 0)) {
                    embedded = Color{static_cast<uint8_t>(red), static_cast<uint8_t>(green),
                                     static_cast<uint8_t>(blue), 255};
                }
                const auto rekordboxColor = findRekordboxColor(colorCode);
                const auto expectedColor = expectedEmbeddedColor(colorCode);
                if ((embedded != expectedColor) && (colorCode != 0 || embedded.has_value())) {
                    // Keep parsing even if colors don't match expected values.
                }

                if (cueFlag == 2) {
                    const int64_t endPosition = beatlink::Util::timeToHalfFrame(
                        beatlink::Util::bytesToNumberLittleEndian(entryBytes.data(), offset + 16, 4));
                    entries.emplace_back(hotCueNumber, position, endPosition, comment, embedded, rekordboxColor);
                } else {
                    entries.emplace_back(hotCueNumber, position, comment, embedded, rekordboxColor);
                }
            }
        }

        if (entrySize <= 0) {
            break;
        }
        offset += static_cast<size_t>(entrySize);
    }
    return sortEntries(std::move(entries));
}

std::vector<CueList::Entry> CueList::sortEntries(std::vector<Entry> entries) {
    std::sort(entries.begin(), entries.end(), [](const Entry& a, const Entry& b) {
        if (a.cuePosition != b.cuePosition) {
            return a.cuePosition < b.cuePosition;
        }
        const int aHot = (a.hotCueNumber != 0) ? 1 : 0;
        const int bHot = (b.hotCueNumber != 0) ? 1 : 0;
        return aHot < bHot;
    });
    return entries;
}

} // namespace beatlink::data
