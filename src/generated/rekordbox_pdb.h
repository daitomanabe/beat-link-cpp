#pragma once

/**
 * rekordbox_pdb.h - DeviceSQL database parser
 *
 * Parses rekordbox export.pdb files
 * Based on Kaitai Struct definition from crate-digger
 */

#include <cstdint>
#include <functional>
#include <istream>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace rekordbox_pdb {

// Forward declarations
class RekordboxPdb;
class Page;
class Table;
class TrackRow;
class ArtworkRow;
class ArtistRow;
class AlbumRow;
class GenreRow;
class LabelRow;
class KeyRow;
class ColorRow;
class PlaylistTreeRow;
class PlaylistEntryRow;
class HistoryPlaylistRow;
class HistoryEntryRow;

// =============================================================================
// Enums
// =============================================================================

enum class PageType : uint32_t {
    TRACKS = 0,
    GENRES = 1,
    ARTISTS = 2,
    ALBUMS = 3,
    LABELS = 4,
    KEYS = 5,
    COLORS = 6,
    PLAYLIST_TREE = 7,
    PLAYLIST_ENTRIES = 8,
    UNKNOWN_9 = 9,
    UNKNOWN_10 = 10,
    HISTORY_PLAYLISTS = 11,
    HISTORY_ENTRIES = 12,
    ARTWORK = 13,
    UNKNOWN_14 = 14,
    UNKNOWN_15 = 15,
    COLUMNS = 16,
    UNKNOWN_17 = 17,
    UNKNOWN_18 = 18,
    HISTORY = 19
};

// =============================================================================
// Row Data Structures
// =============================================================================

struct TrackRow {
    uint32_t id;
    uint16_t indexShift;
    uint32_t bitmask;
    uint32_t sampleRate;
    uint32_t composer;
    uint32_t fileSize;
    uint32_t unknown1;
    uint16_t unknownString1;
    uint16_t unknownString2;
    uint32_t unknown2;
    uint16_t artworkId;
    uint32_t keyId;
    uint32_t originalArtistId;
    uint32_t labelId;
    uint32_t remixerId;
    uint32_t bitrateKbps;
    uint32_t trackNumber;
    uint32_t tempo;  // BPM * 100
    uint32_t genreId;
    uint32_t albumId;
    uint32_t artistId;
    uint32_t id2;
    uint16_t discNumber;
    uint16_t playCount;
    uint16_t year;
    uint16_t sampleDepth;
    uint16_t duration;
    uint16_t unknown3;
    uint8_t colorId;
    uint8_t rating;
    uint16_t unknown4;
    uint16_t unknown5;

    // String offsets (relative to start of page heap)
    uint16_t isoPathOffset;
    uint16_t titleOffset;
    uint16_t unknownString3Offset;
    uint16_t filenameOffset;
    uint16_t unknownString4Offset;
    uint16_t unknownString5Offset;
    uint16_t unknownString6Offset;
    uint16_t commentOffset;
    uint16_t kuvoPublicOffset;
    uint16_t autoloadHotcuesOffset;
    uint16_t unknownString7Offset;
    uint16_t dateAddedOffset;
    uint16_t releaseDateOffset;
    uint16_t mixNameOffset;
    uint16_t unknownString8Offset;
    uint16_t analyzePathOffset;
    uint16_t analyzeDateOffset;
    uint16_t unknownString9Offset;
    uint16_t unknownString10Offset;

    // Resolved strings (populated after parsing)
    std::string isoPath;
    std::string title;
    std::string filename;
    std::string comment;
    std::string dateAdded;
    std::string releaseDate;
    std::string mixName;
    std::string analyzePath;
    std::string analyzeDate;
};

struct ArtworkRow {
    uint32_t id;
    std::string path;
};

struct ArtistRow {
    uint16_t subtype;
    uint16_t indexShift;
    uint32_t id;
    uint8_t unknown1;
    uint16_t nameOffset;
    std::string name;
};

struct AlbumRow {
    uint16_t unknown1;
    uint16_t indexShift;
    uint32_t id;
    uint32_t artistId;
    uint8_t unknown2;
    uint16_t nameOffset;
    std::string name;
};

struct GenreRow {
    uint32_t id;
    std::string name;
};

struct LabelRow {
    uint32_t id;
    std::string name;
};

struct KeyRow {
    uint32_t id;
    uint32_t id2;
    std::string name;
};

struct ColorRow {
    uint32_t id;
    uint8_t unknown1;
    std::string name;
};

struct PlaylistTreeRow {
    uint32_t parentId;
    uint32_t unknown1;
    uint32_t sortOrder;
    uint32_t id;
    uint32_t rawIsFolder;
    std::string name;

    bool isFolder() const { return rawIsFolder != 0; }
};

struct PlaylistEntryRow {
    uint32_t entryIndex;
    uint32_t trackId;
    uint32_t playlistId;
};

struct HistoryPlaylistRow {
    uint32_t id;
    std::string name;
};

struct HistoryEntryRow {
    uint32_t trackId;
    uint32_t playlistId;
    uint32_t entryIndex;
};

// =============================================================================
// Page Structure
// =============================================================================

class Page {
public:
    uint32_t pageIndex;
    PageType type;
    uint32_t nextPageIndex;
    uint32_t sequence;
    uint16_t numRowOffsets;
    uint16_t numRows;
    uint8_t pageFlags;
    uint16_t freeSize;
    uint16_t usedSize;

    std::vector<uint8_t> heap;

    // Parsed rows (one of these will be populated based on type)
    std::vector<TrackRow> tracks;
    std::vector<ArtworkRow> artworks;
    std::vector<ArtistRow> artists;
    std::vector<AlbumRow> albums;
    std::vector<GenreRow> genres;
    std::vector<LabelRow> labels;
    std::vector<KeyRow> keys;
    std::vector<ColorRow> colors;
    std::vector<PlaylistTreeRow> playlistTrees;
    std::vector<PlaylistEntryRow> playlistEntries;
    std::vector<HistoryPlaylistRow> historyPlaylists;
    std::vector<HistoryEntryRow> historyEntries;

    bool isDataPage() const { return (pageFlags & 0x40) == 0; }

    static std::unique_ptr<Page> parse(const uint8_t* data, size_t pageLen, bool isExt);
};

// =============================================================================
// Table Structure
// =============================================================================

class Table {
public:
    PageType type;
    uint32_t firstPageIndex;
    uint32_t lastPageIndex;

    static std::unique_ptr<Table> parse(const uint8_t* data, bool isExt);
};

// =============================================================================
// Main Parser
// =============================================================================

class RekordboxPdb {
public:
    uint32_t pageLen;
    uint32_t numTables;
    uint32_t nextUnusedPage;
    uint32_t sequence;
    std::vector<std::unique_ptr<Table>> tables;
    bool isExt;

    /**
     * Parse PDB file from memory buffer
     * @param isExt true if parsing exportExt.pdb (extended schema)
     */
    static std::unique_ptr<RekordboxPdb> parse(const uint8_t* data, size_t size, bool isExt = false);

    /**
     * Parse PDB file from input stream
     */
    static std::unique_ptr<RekordboxPdb> parse(std::istream& stream, bool isExt = false);

    /**
     * Parse PDB file from file path
     */
    static std::unique_ptr<RekordboxPdb> parseFile(const std::string& path, bool isExt = false);

    /**
     * Get a specific page by index
     */
    std::unique_ptr<Page> getPage(uint32_t index) const;

    /**
     * Iterate all pages of a specific type
     */
    void forEachPage(PageType type, std::function<bool(const Page&)> callback) const;

    /**
     * Find a track by ID
     */
    std::optional<TrackRow> findTrack(uint32_t id) const;

    /**
     * Find artwork by ID
     */
    std::optional<ArtworkRow> findArtwork(uint32_t id) const;

    /**
     * Find artist by ID
     */
    std::optional<ArtistRow> findArtist(uint32_t id) const;

    /**
     * Find album by ID
     */
    std::optional<AlbumRow> findAlbum(uint32_t id) const;

    /**
     * Get all tracks
     */
    std::vector<TrackRow> getAllTracks() const;

    /**
     * Get all artworks
     */
    std::vector<ArtworkRow> getAllArtworks() const;

private:
    const uint8_t* rawData_ = nullptr;
    size_t rawSize_ = 0;
    mutable std::vector<uint8_t> ownedData_;
};

} // namespace rekordbox_pdb
