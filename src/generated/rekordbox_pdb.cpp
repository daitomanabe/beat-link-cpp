#include "rekordbox_pdb.h"

#include <cstring>
#include <fstream>

namespace rekordbox_pdb {

// =============================================================================
// Helper functions for little-endian reading
// =============================================================================

static uint16_t readU16LE(const uint8_t* data) {
    return data[0] | (static_cast<uint16_t>(data[1]) << 8);
}

static uint32_t readU32LE(const uint8_t* data) {
    return data[0] |
           (static_cast<uint32_t>(data[1]) << 8) |
           (static_cast<uint32_t>(data[2]) << 16) |
           (static_cast<uint32_t>(data[3]) << 24);
}

static std::string readDeviceSqlString(const uint8_t* heap, uint16_t offset, size_t heapSize) {
    if (offset == 0 || offset >= heapSize) return "";

    const uint8_t* strData = heap + offset;
    size_t remaining = heapSize - offset;

    if (remaining < 4) return "";

    // First byte indicates string type
    uint8_t strType = strData[0];

    if (strType == 0x90) {
        // Short ASCII string
        if (remaining < 2) return "";
        uint8_t len = strData[1];
        if (remaining < 2 + len) return "";
        return std::string(reinterpret_cast<const char*>(strData + 2), len);
    } else if (strType == 0x40) {
        // Long ASCII string (length as u16)
        if (remaining < 3) return "";
        uint16_t len = readU16LE(strData + 1);
        if (remaining < 3 + len) return "";
        return std::string(reinterpret_cast<const char*>(strData + 3), len);
    } else if (strType == 0x80) {
        // Short ISRC (same as short ASCII for our purposes)
        if (remaining < 2) return "";
        uint8_t len = strData[1];
        if (remaining < 2 + len) return "";
        return std::string(reinterpret_cast<const char*>(strData + 2), len);
    } else if ((strType & 0xF0) == 0x00) {
        // Possibly just raw bytes, return empty
        return "";
    }

    return "";
}

// =============================================================================
// Table parsing
// =============================================================================

std::unique_ptr<Table> Table::parse(const uint8_t* data, bool isExt) {
    auto table = std::make_unique<Table>();

    table->type = static_cast<PageType>(readU32LE(data));
    // empty_candidate at offset 4
    table->firstPageIndex = readU32LE(data + 8);
    table->lastPageIndex = readU32LE(data + 12);

    return table;
}

// =============================================================================
// Page parsing
// =============================================================================

std::unique_ptr<Page> Page::parse(const uint8_t* data, size_t pageLen, bool isExt) {
    auto page = std::make_unique<Page>();

    if (pageLen < 40) return nullptr;

    // Skip gap (4 bytes)
    page->pageIndex = readU32LE(data + 4);
    page->type = static_cast<PageType>(readU32LE(data + 8));
    page->nextPageIndex = readU32LE(data + 12);
    page->sequence = readU32LE(data + 16);
    // Skip 4 bytes at offset 20

    // Bit-packed fields at offset 24
    uint32_t packed = readU32LE(data + 24);
    page->numRowOffsets = packed & 0x1FFF;           // 13 bits
    page->numRows = (packed >> 13) & 0x7FF;          // 11 bits
    page->pageFlags = (packed >> 24) & 0xFF;         // 8 bits

    page->freeSize = readU16LE(data + 28);
    page->usedSize = readU16LE(data + 30);
    // transaction fields at 32-36
    // more unknown at 36-40

    // Header ends at offset 40, heap follows
    size_t heapOffset = 40;
    size_t heapSize = pageLen - heapOffset;
    page->heap.assign(data + heapOffset, data + pageLen);

    if (!page->isDataPage()) {
        return page;  // Non-data pages don't have row data
    }

    // Parse row groups
    size_t numRowGroups = (page->numRowOffsets + 15) / 16;

    for (size_t groupIdx = 0; groupIdx < numRowGroups; ++groupIdx) {
        size_t groupBase = pageLen - (groupIdx * 0x24);

        if (groupBase < heapOffset + 4) break;

        // Row present flags at groupBase - 4
        uint16_t rowPresentFlags = readU16LE(data + groupBase - 4);

        for (int rowInGroup = 0; rowInGroup < 16; ++rowInGroup) {
            // Check if row is present
            if ((rowPresentFlags & (1 << rowInGroup)) == 0) continue;

            // Row offset at groupBase - 6 - (rowInGroup * 2)
            size_t rowOffsetPos = groupBase - 6 - (rowInGroup * 2);
            if (rowOffsetPos < heapOffset || rowOffsetPos + 2 > pageLen) continue;

            uint16_t rowOffset = readU16LE(data + rowOffsetPos);
            if (rowOffset == 0 || rowOffset >= pageLen) continue;

            const uint8_t* rowData = data + heapOffset + rowOffset;
            size_t rowRemaining = pageLen - heapOffset - rowOffset;

            // Parse row based on page type
            switch (page->type) {
                case PageType::TRACKS: {
                    if (rowRemaining < 96) break;
                    TrackRow track;
                    track.id = readU32LE(rowData + 0);
                    track.indexShift = readU16LE(rowData + 4);
                    track.bitmask = readU32LE(rowData + 6);
                    track.sampleRate = readU32LE(rowData + 10);
                    track.composer = readU32LE(rowData + 14);
                    track.fileSize = readU32LE(rowData + 18);
                    track.artworkId = readU16LE(rowData + 30);
                    track.keyId = readU32LE(rowData + 32);
                    track.originalArtistId = readU32LE(rowData + 36);
                    track.labelId = readU32LE(rowData + 40);
                    track.remixerId = readU32LE(rowData + 44);
                    track.bitrateKbps = readU32LE(rowData + 48);
                    track.trackNumber = readU32LE(rowData + 52);
                    track.tempo = readU32LE(rowData + 56);
                    track.genreId = readU32LE(rowData + 60);
                    track.albumId = readU32LE(rowData + 64);
                    track.artistId = readU32LE(rowData + 68);
                    track.id2 = readU32LE(rowData + 72);
                    track.discNumber = readU16LE(rowData + 76);
                    track.playCount = readU16LE(rowData + 78);
                    track.year = readU16LE(rowData + 80);
                    track.sampleDepth = readU16LE(rowData + 82);
                    track.duration = readU16LE(rowData + 84);
                    track.colorId = rowData[88];
                    track.rating = rowData[89];

                    // String offsets (these are relative to heap start)
                    track.isoPathOffset = readU16LE(rowData + 92);
                    track.titleOffset = readU16LE(rowData + 94);

                    // Read strings from heap
                    track.isoPath = readDeviceSqlString(page->heap.data(), track.isoPathOffset, page->heap.size());
                    track.title = readDeviceSqlString(page->heap.data(), track.titleOffset, page->heap.size());

                    page->tracks.push_back(track);
                    break;
                }

                case PageType::ARTWORK: {
                    if (rowRemaining < 6) break;
                    ArtworkRow artwork;
                    artwork.id = readU32LE(rowData + 0);
                    uint16_t pathOffset = readU16LE(rowData + 4);
                    artwork.path = readDeviceSqlString(page->heap.data(), pathOffset, page->heap.size());
                    page->artworks.push_back(artwork);
                    break;
                }

                case PageType::ARTISTS: {
                    if (rowRemaining < 9) break;
                    ArtistRow artist;
                    artist.subtype = readU16LE(rowData + 0);
                    artist.indexShift = readU16LE(rowData + 2);
                    artist.id = readU32LE(rowData + 4);
                    artist.unknown1 = rowData[8];
                    artist.nameOffset = readU16LE(rowData + 9);
                    artist.name = readDeviceSqlString(page->heap.data(), artist.nameOffset, page->heap.size());
                    page->artists.push_back(artist);
                    break;
                }

                case PageType::ALBUMS: {
                    if (rowRemaining < 13) break;
                    AlbumRow album;
                    album.unknown1 = readU16LE(rowData + 0);
                    album.indexShift = readU16LE(rowData + 2);
                    album.id = readU32LE(rowData + 4);
                    album.artistId = readU32LE(rowData + 8);
                    album.unknown2 = rowData[12];
                    album.nameOffset = readU16LE(rowData + 13);
                    album.name = readDeviceSqlString(page->heap.data(), album.nameOffset, page->heap.size());
                    page->albums.push_back(album);
                    break;
                }

                case PageType::GENRES: {
                    if (rowRemaining < 6) break;
                    GenreRow genre;
                    genre.id = readU32LE(rowData + 0);
                    uint16_t nameOffset = readU16LE(rowData + 4);
                    genre.name = readDeviceSqlString(page->heap.data(), nameOffset, page->heap.size());
                    page->genres.push_back(genre);
                    break;
                }

                case PageType::LABELS: {
                    if (rowRemaining < 6) break;
                    LabelRow label;
                    label.id = readU32LE(rowData + 0);
                    uint16_t nameOffset = readU16LE(rowData + 4);
                    label.name = readDeviceSqlString(page->heap.data(), nameOffset, page->heap.size());
                    page->labels.push_back(label);
                    break;
                }

                case PageType::KEYS: {
                    if (rowRemaining < 10) break;
                    KeyRow key;
                    key.id = readU32LE(rowData + 0);
                    key.id2 = readU32LE(rowData + 4);
                    uint16_t nameOffset = readU16LE(rowData + 8);
                    key.name = readDeviceSqlString(page->heap.data(), nameOffset, page->heap.size());
                    page->keys.push_back(key);
                    break;
                }

                case PageType::COLORS: {
                    if (rowRemaining < 7) break;
                    ColorRow color;
                    color.id = readU32LE(rowData + 0);
                    color.unknown1 = rowData[4];
                    uint16_t nameOffset = readU16LE(rowData + 5);
                    color.name = readDeviceSqlString(page->heap.data(), nameOffset, page->heap.size());
                    page->colors.push_back(color);
                    break;
                }

                case PageType::PLAYLIST_TREE: {
                    if (rowRemaining < 22) break;
                    PlaylistTreeRow playlist;
                    playlist.parentId = readU32LE(rowData + 0);
                    playlist.unknown1 = readU32LE(rowData + 4);
                    playlist.sortOrder = readU32LE(rowData + 8);
                    playlist.id = readU32LE(rowData + 12);
                    playlist.rawIsFolder = readU32LE(rowData + 16);
                    uint16_t nameOffset = readU16LE(rowData + 20);
                    playlist.name = readDeviceSqlString(page->heap.data(), nameOffset, page->heap.size());
                    page->playlistTrees.push_back(playlist);
                    break;
                }

                case PageType::PLAYLIST_ENTRIES: {
                    if (rowRemaining < 12) break;
                    PlaylistEntryRow entry;
                    entry.entryIndex = readU32LE(rowData + 0);
                    entry.trackId = readU32LE(rowData + 4);
                    entry.playlistId = readU32LE(rowData + 8);
                    page->playlistEntries.push_back(entry);
                    break;
                }

                default:
                    break;
            }
        }
    }

    return page;
}

// =============================================================================
// RekordboxPdb
// =============================================================================

std::unique_ptr<RekordboxPdb> RekordboxPdb::parse(const uint8_t* data, size_t size, bool isExt) {
    auto pdb = std::make_unique<RekordboxPdb>();
    pdb->isExt = isExt;

    if (size < 28) return nullptr;

    // Header: skip first 4 bytes (unknown/signature)
    pdb->pageLen = readU32LE(data + 4);
    pdb->numTables = readU32LE(data + 8);
    pdb->nextUnusedPage = readU32LE(data + 12);
    // skip 4 bytes
    pdb->sequence = readU32LE(data + 20);
    // skip 4 bytes (gap)

    // Tables start at offset 28
    const uint8_t* tableData = data + 28;
    for (uint32_t i = 0; i < pdb->numTables && (28 + i * 16) < size; ++i) {
        auto table = Table::parse(tableData + i * 16, isExt);
        pdb->tables.push_back(std::move(table));
    }

    // Store raw data for page access
    pdb->ownedData_.assign(data, data + size);
    pdb->rawData_ = pdb->ownedData_.data();
    pdb->rawSize_ = size;

    return pdb;
}

std::unique_ptr<RekordboxPdb> RekordboxPdb::parse(std::istream& stream, bool isExt) {
    stream.seekg(0, std::ios::end);
    size_t size = stream.tellg();
    stream.seekg(0, std::ios::beg);

    std::vector<uint8_t> buffer(size);
    stream.read(reinterpret_cast<char*>(buffer.data()), size);

    return parse(buffer.data(), buffer.size(), isExt);
}

std::unique_ptr<RekordboxPdb> RekordboxPdb::parseFile(const std::string& path, bool isExt) {
    std::ifstream file(path, std::ios::binary);
    if (!file) return nullptr;
    return parse(file, isExt);
}

std::unique_ptr<Page> RekordboxPdb::getPage(uint32_t index) const {
    if (!rawData_ || rawSize_ == 0) return nullptr;

    size_t pageOffset = static_cast<size_t>(pageLen) * index;
    if (pageOffset + pageLen > rawSize_) return nullptr;

    return Page::parse(rawData_ + pageOffset, pageLen, isExt);
}

void RekordboxPdb::forEachPage(PageType type, std::function<bool(const Page&)> callback) const {
    // Find the table for this type
    for (const auto& table : tables) {
        if (table->type == type) {
            uint32_t pageIdx = table->firstPageIndex;
            uint32_t lastPageIdx = table->lastPageIndex;

            while (pageIdx < nextUnusedPage) {
                auto page = getPage(pageIdx);
                if (!page) break;

                if (page->type == type && page->isDataPage()) {
                    if (!callback(*page)) return;  // Stop if callback returns false
                }

                if (pageIdx == lastPageIdx) break;
                pageIdx = page->nextPageIndex;

                // Safety check to prevent infinite loops
                if (pageIdx <= table->firstPageIndex) break;
            }
            break;
        }
    }
}

std::optional<TrackRow> RekordboxPdb::findTrack(uint32_t id) const {
    std::optional<TrackRow> result;

    forEachPage(PageType::TRACKS, [&](const Page& page) {
        for (const auto& track : page.tracks) {
            if (track.id == id) {
                result = track;
                return false;  // Stop iterating
            }
        }
        return true;  // Continue
    });

    return result;
}

std::optional<ArtworkRow> RekordboxPdb::findArtwork(uint32_t id) const {
    std::optional<ArtworkRow> result;

    forEachPage(PageType::ARTWORK, [&](const Page& page) {
        for (const auto& artwork : page.artworks) {
            if (artwork.id == id) {
                result = artwork;
                return false;
            }
        }
        return true;
    });

    return result;
}

std::optional<ArtistRow> RekordboxPdb::findArtist(uint32_t id) const {
    std::optional<ArtistRow> result;

    forEachPage(PageType::ARTISTS, [&](const Page& page) {
        for (const auto& artist : page.artists) {
            if (artist.id == id) {
                result = artist;
                return false;
            }
        }
        return true;
    });

    return result;
}

std::optional<AlbumRow> RekordboxPdb::findAlbum(uint32_t id) const {
    std::optional<AlbumRow> result;

    forEachPage(PageType::ALBUMS, [&](const Page& page) {
        for (const auto& album : page.albums) {
            if (album.id == id) {
                result = album;
                return false;
            }
        }
        return true;
    });

    return result;
}

std::vector<TrackRow> RekordboxPdb::getAllTracks() const {
    std::vector<TrackRow> result;

    forEachPage(PageType::TRACKS, [&](const Page& page) {
        result.insert(result.end(), page.tracks.begin(), page.tracks.end());
        return true;
    });

    return result;
}

std::vector<ArtworkRow> RekordboxPdb::getAllArtworks() const {
    std::vector<ArtworkRow> result;

    forEachPage(PageType::ARTWORK, [&](const Page& page) {
        result.insert(result.end(), page.artworks.begin(), page.artworks.end());
        return true;
    });

    return result;
}

} // namespace rekordbox_pdb
