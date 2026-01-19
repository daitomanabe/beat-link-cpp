#include "rekordbox_anlz.h"

#include <fstream>
#include <cstring>

namespace rekordbox_anlz {

// =============================================================================
// Helper functions for big-endian reading
// =============================================================================

static uint16_t readU16BE(const uint8_t* data) {
    return (static_cast<uint16_t>(data[0]) << 8) | data[1];
}

static uint32_t readU32BE(const uint8_t* data) {
    return (static_cast<uint32_t>(data[0]) << 24) |
           (static_cast<uint32_t>(data[1]) << 16) |
           (static_cast<uint32_t>(data[2]) << 8) |
           data[3];
}

static int32_t readS32BE(const uint8_t* data) {
    return static_cast<int32_t>(readU32BE(data));
}

static std::string readUTF16BE(const uint8_t* data, size_t byteLen) {
    std::string result;
    result.reserve(byteLen / 2);
    for (size_t i = 0; i + 1 < byteLen; i += 2) {
        uint16_t ch = readU16BE(data + i);
        if (ch == 0) break;
        if (ch < 128) {
            result.push_back(static_cast<char>(ch));
        } else if (ch < 0x800) {
            result.push_back(static_cast<char>(0xC0 | (ch >> 6)));
            result.push_back(static_cast<char>(0x80 | (ch & 0x3F)));
        } else {
            result.push_back(static_cast<char>(0xE0 | (ch >> 12)));
            result.push_back(static_cast<char>(0x80 | ((ch >> 6) & 0x3F)));
            result.push_back(static_cast<char>(0x80 | (ch & 0x3F)));
        }
    }
    return result;
}

// =============================================================================
// PSSI XOR mask for song structure
// =============================================================================

static void unmaskSongStructure(uint8_t* data, size_t size, uint16_t numEntries) {
    static const uint8_t baseMask[] = {
        0xCB, 0xE1, 0xEE, 0xFA, 0xE5, 0xEE, 0xAD, 0xEE,
        0xE9, 0xD2, 0xE9, 0xEB, 0xE1, 0xE9, 0xF3, 0xE8,
        0xE9, 0xF4, 0xE1
    };
    const size_t maskLen = sizeof(baseMask);

    // Check if masking is applied (raw_mood > 20)
    if (size >= 2) {
        uint16_t rawMood = readU16BE(data);
        if (rawMood <= 20) {
            return;  // Not masked
        }
    }

    // Apply XOR mask
    uint8_t c = static_cast<uint8_t>(numEntries);
    for (size_t i = 0; i < size && i < 256; ++i) {
        uint8_t mask = (baseMask[i % maskLen] + c) & 0xFF;
        data[i] ^= mask;
    }
}

// =============================================================================
// BeatGridTag
// =============================================================================

std::unique_ptr<BeatGridTag> BeatGridTag::parse(const uint8_t* data, size_t size) {
    auto tag = std::make_unique<BeatGridTag>();

    if (size < 12) return tag;

    // Skip first 8 bytes (unknown fields)
    uint32_t numBeats = readU32BE(data + 8);

    const uint8_t* beatData = data + 12;
    size_t remaining = size - 12;

    for (uint32_t i = 0; i < numBeats && remaining >= 8; ++i) {
        BeatGridBeat beat;
        beat.beatNumber = readU16BE(beatData);
        beat.tempo = readU16BE(beatData + 2);
        beat.time = readU32BE(beatData + 4);
        tag->beats.push_back(beat);
        beatData += 8;
        remaining -= 8;
    }

    return tag;
}

// =============================================================================
// CueTag
// =============================================================================

std::unique_ptr<CueTag> CueTag::parse(const uint8_t* data, size_t size) {
    auto tag = std::make_unique<CueTag>();

    if (size < 12) return tag;

    tag->type = static_cast<CueListType>(readU32BE(data));
    uint16_t numCues = readU16BE(data + 6);
    // Skip memory_count at offset 8

    const uint8_t* cueData = data + 12;
    size_t remaining = size - 12;

    for (uint16_t i = 0; i < numCues && remaining >= 36; ++i) {
        // Check PCPT magic
        if (cueData[0] != 'P' || cueData[1] != 'C' ||
            cueData[2] != 'P' || cueData[3] != 'T') {
            break;
        }

        uint32_t lenHeader = readU32BE(cueData + 4);
        uint32_t lenEntry = readU32BE(cueData + 8);

        if (remaining < lenEntry) break;

        CueEntry cue;
        cue.hotCue = readU32BE(cueData + 12);
        cue.status = static_cast<CueEntryStatus>(readU32BE(cueData + 16));
        cue.type = static_cast<CueEntryType>(cueData[24]);
        cue.time = readU32BE(cueData + 28);
        cue.loopTime = readU32BE(cueData + 32);

        tag->cues.push_back(cue);
        cueData += lenEntry;
        remaining -= lenEntry;
    }

    return tag;
}

// =============================================================================
// CueExtendedTag
// =============================================================================

std::unique_ptr<CueExtendedTag> CueExtendedTag::parse(const uint8_t* data, size_t size) {
    auto tag = std::make_unique<CueExtendedTag>();

    if (size < 8) return tag;

    tag->type = static_cast<CueListType>(readU32BE(data));
    uint16_t numCues = readU16BE(data + 4);

    const uint8_t* cueData = data + 8;
    size_t remaining = size - 8;

    for (uint16_t i = 0; i < numCues && remaining >= 24; ++i) {
        // Check PCP2 magic
        if (cueData[0] != 'P' || cueData[1] != 'C' ||
            cueData[2] != 'P' || cueData[3] != '2') {
            break;
        }

        uint32_t lenHeader = readU32BE(cueData + 4);
        uint32_t lenEntry = readU32BE(cueData + 8);

        if (remaining < lenEntry) break;

        CueExtendedEntry cue;
        cue.hotCue = readU32BE(cueData + 12);
        cue.type = static_cast<CueEntryType>(cueData[16]);
        cue.time = readU32BE(cueData + 20);
        cue.loopTime = readU32BE(cueData + 24);
        cue.colorId = cueData[28];
        cue.loopNumerator = readU16BE(cueData + 36);
        cue.loopDenominator = readU16BE(cueData + 38);

        // Comment
        if (lenEntry > 43) {
            uint32_t lenComment = readU32BE(cueData + 40);
            if (lenEntry >= 44 + lenComment) {
                cue.comment = readUTF16BE(cueData + 44, lenComment);
            }

            // Color info
            size_t colorOffset = 44 + lenComment;
            if (lenEntry > colorOffset) {
                cue.colorCode = cueData[colorOffset];
            }
            if (lenEntry > colorOffset + 1) {
                cue.colorRed = cueData[colorOffset + 1];
            }
            if (lenEntry > colorOffset + 2) {
                cue.colorGreen = cueData[colorOffset + 2];
            }
            if (lenEntry > colorOffset + 3) {
                cue.colorBlue = cueData[colorOffset + 3];
            }
        }

        tag->cues.push_back(cue);
        cueData += lenEntry;
        remaining -= lenEntry;
    }

    return tag;
}

// =============================================================================
// PathTag
// =============================================================================

std::unique_ptr<PathTag> PathTag::parse(const uint8_t* data, size_t size) {
    auto tag = std::make_unique<PathTag>();

    if (size < 4) return tag;

    uint32_t lenPath = readU32BE(data);
    if (lenPath > 1 && size >= 4 + lenPath) {
        tag->path = readUTF16BE(data + 4, lenPath - 2);
    }

    return tag;
}

// =============================================================================
// WavePreviewTag
// =============================================================================

std::unique_ptr<WavePreviewTag> WavePreviewTag::parse(const uint8_t* data, size_t size) {
    auto tag = std::make_unique<WavePreviewTag>();

    if (size < 8) return tag;

    uint32_t lenData = readU32BE(data);
    if (size >= 8 + lenData) {
        tag->data.assign(data + 8, data + 8 + lenData);
    }

    return tag;
}

// =============================================================================
// WaveScrollTag
// =============================================================================

std::unique_ptr<WaveScrollTag> WaveScrollTag::parse(const uint8_t* data, size_t size) {
    auto tag = std::make_unique<WaveScrollTag>();

    if (size < 12) return tag;

    tag->entrySize = readU32BE(data);
    uint32_t numEntries = readU32BE(data + 4);
    size_t dataSize = numEntries * tag->entrySize;

    if (size >= 12 + dataSize) {
        tag->entries.assign(data + 12, data + 12 + dataSize);
    }

    return tag;
}

// =============================================================================
// WaveColorPreviewTag
// =============================================================================

std::unique_ptr<WaveColorPreviewTag> WaveColorPreviewTag::parse(const uint8_t* data, size_t size) {
    auto tag = std::make_unique<WaveColorPreviewTag>();

    if (size < 12) return tag;

    tag->entrySize = readU32BE(data);
    uint32_t numEntries = readU32BE(data + 4);
    size_t dataSize = numEntries * tag->entrySize;

    if (size >= 12 + dataSize) {
        tag->entries.assign(data + 12, data + 12 + dataSize);
    }

    return tag;
}

// =============================================================================
// WaveColorScrollTag
// =============================================================================

std::unique_ptr<WaveColorScrollTag> WaveColorScrollTag::parse(const uint8_t* data, size_t size) {
    auto tag = std::make_unique<WaveColorScrollTag>();

    if (size < 12) return tag;

    tag->entrySize = readU32BE(data);
    uint32_t numEntries = readU32BE(data + 4);
    size_t dataSize = numEntries * tag->entrySize;

    if (size >= 12 + dataSize) {
        tag->entries.assign(data + 12, data + 12 + dataSize);
    }

    return tag;
}

// =============================================================================
// SongStructureTag
// =============================================================================

std::unique_ptr<SongStructureTag> SongStructureTag::parse(const uint8_t* data, size_t size) {
    auto tag = std::make_unique<SongStructureTag>();

    if (size < 6) return tag;

    uint32_t lenEntryBytes = readU32BE(data);
    uint16_t numEntries = readU16BE(data + 4);

    // Body starts at offset 6
    size_t bodySize = size - 6;
    if (bodySize == 0) return tag;

    // Make a copy for unmasking
    std::vector<uint8_t> body(data + 6, data + size);
    unmaskSongStructure(body.data(), body.size(), numEntries);

    // Parse unmasked body
    if (body.size() >= 14) {
        tag->mood = static_cast<TrackMood>(readU16BE(body.data()));
        tag->endBeat = readU16BE(body.data() + 8);
        tag->bank = body[12];

        // Entries start at offset 14
        const uint8_t* entryData = body.data() + 14;
        size_t remaining = body.size() - 14;

        for (uint16_t i = 0; i < numEntries && remaining >= lenEntryBytes; ++i) {
            SongStructureEntry entry;
            entry.index = readU16BE(entryData);
            entry.beat = readU16BE(entryData + 2);
            entry.kind = readU16BE(entryData + 4);
            entry.fill = entryData[19];
            entry.beatFill = readU16BE(entryData + 20);

            tag->entries.push_back(entry);
            entryData += lenEntryBytes;
            remaining -= lenEntryBytes;
        }
    }

    return tag;
}

// =============================================================================
// TaggedSection
// =============================================================================

std::unique_ptr<TaggedSection> TaggedSection::parse(const uint8_t* data, size_t size, size_t& bytesRead) {
    auto section = std::make_unique<TaggedSection>();
    bytesRead = 0;

    if (size < 12) return nullptr;

    section->fourcc = static_cast<SectionTags>(readS32BE(data));
    section->lenHeader = readU32BE(data + 4);
    section->lenTag = readU32BE(data + 8);

    if (size < section->lenTag) return nullptr;

    bytesRead = section->lenTag;

    // Body starts at offset 12
    const uint8_t* bodyData = data + 12;
    size_t bodySize = section->lenTag - 12;

    switch (section->fourcc) {
        case SectionTags::BEAT_GRID:
            section->beatGrid = BeatGridTag::parse(bodyData, bodySize);
            break;

        case SectionTags::CUES:
            section->cues = CueTag::parse(bodyData, bodySize);
            break;

        case SectionTags::CUES_2:
            section->cuesExtended = CueExtendedTag::parse(bodyData, bodySize);
            break;

        case SectionTags::PATH:
            section->path = PathTag::parse(bodyData, bodySize);
            break;

        case SectionTags::WAVE_PREVIEW:
        case SectionTags::WAVE_TINY:
            section->wavePreview = WavePreviewTag::parse(bodyData, bodySize);
            break;

        case SectionTags::WAVE_SCROLL:
            section->waveScroll = WaveScrollTag::parse(bodyData, bodySize);
            break;

        case SectionTags::WAVE_COLOR_PREVIEW:
        case SectionTags::WAVE_3BAND_PREVIEW:
            section->waveColorPreview = WaveColorPreviewTag::parse(bodyData, bodySize);
            break;

        case SectionTags::WAVE_COLOR_SCROLL:
        case SectionTags::WAVE_3BAND_SCROLL:
            section->waveColorScroll = WaveColorScrollTag::parse(bodyData, bodySize);
            break;

        case SectionTags::SONG_STRUCTURE:
            section->songStructure = SongStructureTag::parse(bodyData, bodySize);
            break;

        default:
            section->rawData.assign(bodyData, bodyData + bodySize);
            break;
    }

    return section;
}

// =============================================================================
// RekordboxAnlz
// =============================================================================

std::unique_ptr<RekordboxAnlz> RekordboxAnlz::parse(const uint8_t* data, size_t size) {
    auto anlz = std::make_unique<RekordboxAnlz>();

    if (size < 12) return nullptr;

    // Check PMAI magic
    if (data[0] != 'P' || data[1] != 'M' || data[2] != 'A' || data[3] != 'I') {
        return nullptr;
    }

    anlz->lenHeader = readU32BE(data + 4);
    anlz->lenFile = readU32BE(data + 8);

    // Parse sections starting after header
    size_t offset = anlz->lenHeader;
    while (offset < size) {
        size_t bytesRead = 0;
        auto section = TaggedSection::parse(data + offset, size - offset, bytesRead);
        if (!section || bytesRead == 0) break;

        anlz->sections.push_back(std::move(section));
        offset += bytesRead;
    }

    return anlz;
}

std::unique_ptr<RekordboxAnlz> RekordboxAnlz::parse(std::istream& stream) {
    stream.seekg(0, std::ios::end);
    size_t size = stream.tellg();
    stream.seekg(0, std::ios::beg);

    std::vector<uint8_t> buffer(size);
    stream.read(reinterpret_cast<char*>(buffer.data()), size);

    return parse(buffer.data(), buffer.size());
}

std::unique_ptr<RekordboxAnlz> RekordboxAnlz::parseFile(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) return nullptr;
    return parse(file);
}

const BeatGridTag* RekordboxAnlz::getBeatGrid() const {
    for (const auto& section : sections) {
        if (section->beatGrid) return section->beatGrid.get();
    }
    return nullptr;
}

const CueTag* RekordboxAnlz::getMemoryCues() const {
    for (const auto& section : sections) {
        if (section->cues && section->cues->type == CueListType::MEMORY_CUES) {
            return section->cues.get();
        }
    }
    return nullptr;
}

const CueTag* RekordboxAnlz::getHotCues() const {
    for (const auto& section : sections) {
        if (section->cues && section->cues->type == CueListType::HOT_CUES) {
            return section->cues.get();
        }
    }
    return nullptr;
}

const CueExtendedTag* RekordboxAnlz::getMemoryCuesExtended() const {
    for (const auto& section : sections) {
        if (section->cuesExtended && section->cuesExtended->type == CueListType::MEMORY_CUES) {
            return section->cuesExtended.get();
        }
    }
    return nullptr;
}

const CueExtendedTag* RekordboxAnlz::getHotCuesExtended() const {
    for (const auto& section : sections) {
        if (section->cuesExtended && section->cuesExtended->type == CueListType::HOT_CUES) {
            return section->cuesExtended.get();
        }
    }
    return nullptr;
}

const PathTag* RekordboxAnlz::getPath() const {
    for (const auto& section : sections) {
        if (section->path) return section->path.get();
    }
    return nullptr;
}

const WavePreviewTag* RekordboxAnlz::getWavePreview() const {
    for (const auto& section : sections) {
        if (section->wavePreview) return section->wavePreview.get();
    }
    return nullptr;
}

const WaveScrollTag* RekordboxAnlz::getWaveScroll() const {
    for (const auto& section : sections) {
        if (section->waveScroll) return section->waveScroll.get();
    }
    return nullptr;
}

const WaveColorPreviewTag* RekordboxAnlz::getWaveColorPreview() const {
    for (const auto& section : sections) {
        if (section->waveColorPreview) return section->waveColorPreview.get();
    }
    return nullptr;
}

const WaveColorScrollTag* RekordboxAnlz::getWaveColorScroll() const {
    for (const auto& section : sections) {
        if (section->waveColorScroll) return section->waveColorScroll.get();
    }
    return nullptr;
}

const SongStructureTag* RekordboxAnlz::getSongStructure() const {
    for (const auto& section : sections) {
        if (section->songStructure) return section->songStructure.get();
    }
    return nullptr;
}

} // namespace rekordbox_anlz
