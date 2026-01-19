/**
 * OpusProvider implementation
 *
 * Provides metadata for Opus Quad and XDJ-AZ devices by reading
 * pre-created metadata archives (ZIP files containing PDB and ANLZ data).
 */

#include "beatlink/data/OpusProvider.hpp"
#include "beatlink/data/AnlzParser.hpp"
#include "beatlink/data/BeatGrid.hpp"
#include "beatlink/data/PdbParser.hpp"
#include "beatlink/data/TrackMetadata.hpp"
#include "beatlink/data/WaveformDetail.hpp"
#include "beatlink/data/WaveformPreview.hpp"
#include "beatlink/data/ZipArchive.hpp"

#include <algorithm>
#include <array>
#include <iomanip>
#include <sstream>

// SHA-1 implementation (simple, portable)
// For production use, consider using OpenSSL or a crypto library
namespace {

class SHA1 {
public:
    SHA1() { reset(); }

    void reset() {
        h_[0] = 0x67452301;
        h_[1] = 0xEFCDAB89;
        h_[2] = 0x98BADCFE;
        h_[3] = 0x10325476;
        h_[4] = 0xC3D2E1F0;
        blockByteIndex_ = 0;
        byteCount_ = 0;
    }

    void update(const uint8_t* data, size_t len) {
        while (len--) {
            block_[blockByteIndex_++] = *data++;
            byteCount_++;
            if (blockByteIndex_ == 64) {
                processBlock();
                blockByteIndex_ = 0;
            }
        }
    }

    std::string finalize() {
        // Padding
        size_t i = blockByteIndex_;
        block_[i++] = 0x80;
        if (i > 56) {
            while (i < 64) block_[i++] = 0;
            processBlock();
            i = 0;
        }
        while (i < 56) block_[i++] = 0;

        // Append length in bits
        uint64_t bits = byteCount_ * 8;
        block_[56] = static_cast<uint8_t>(bits >> 56);
        block_[57] = static_cast<uint8_t>(bits >> 48);
        block_[58] = static_cast<uint8_t>(bits >> 40);
        block_[59] = static_cast<uint8_t>(bits >> 32);
        block_[60] = static_cast<uint8_t>(bits >> 24);
        block_[61] = static_cast<uint8_t>(bits >> 16);
        block_[62] = static_cast<uint8_t>(bits >> 8);
        block_[63] = static_cast<uint8_t>(bits);
        processBlock();

        // Convert to hex string
        std::ostringstream result;
        result << std::hex << std::setfill('0');
        for (int j = 0; j < 5; ++j) {
            result << std::setw(8) << h_[j];
        }
        return result.str();
    }

private:
    void processBlock() {
        std::array<uint32_t, 80> w{};

        // Break chunk into sixteen 32-bit big-endian words
        for (int i = 0; i < 16; ++i) {
            w[i] = (static_cast<uint32_t>(block_[i * 4]) << 24) |
                   (static_cast<uint32_t>(block_[i * 4 + 1]) << 16) |
                   (static_cast<uint32_t>(block_[i * 4 + 2]) << 8) |
                   (static_cast<uint32_t>(block_[i * 4 + 3]));
        }

        // Extend
        for (int i = 16; i < 80; ++i) {
            uint32_t temp = w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16];
            w[i] = (temp << 1) | (temp >> 31);
        }

        uint32_t a = h_[0], b = h_[1], c = h_[2], d = h_[3], e = h_[4];

        for (int i = 0; i < 80; ++i) {
            uint32_t f, k;
            if (i < 20) {
                f = (b & c) | ((~b) & d);
                k = 0x5A827999;
            } else if (i < 40) {
                f = b ^ c ^ d;
                k = 0x6ED9EBA1;
            } else if (i < 60) {
                f = (b & c) | (b & d) | (c & d);
                k = 0x8F1BBCDC;
            } else {
                f = b ^ c ^ d;
                k = 0xCA62C1D6;
            }
            uint32_t temp = ((a << 5) | (a >> 27)) + f + e + k + w[i];
            e = d;
            d = c;
            c = (b << 30) | (b >> 2);
            b = a;
            a = temp;
        }

        h_[0] += a;
        h_[1] += b;
        h_[2] += c;
        h_[3] += d;
        h_[4] += e;
    }

    uint32_t h_[5];
    uint8_t block_[64];
    size_t blockByteIndex_;
    uint64_t byteCount_;
};

} // anonymous namespace

namespace beatlink::data {

std::string OpusProvider::computeSha1(const uint8_t* data, size_t length) const {
    SHA1 sha1;
    sha1.update(data, length);
    return sha1.finalize();
}

bool OpusProvider::attachMetadataArchive(const std::string& archivePath, int usbSlotNumber) {
    if (usbSlotNumber < 1 || usbSlotNumber > 3) {
        return false;
    }

    // First detach any existing archive
    detachMetadataArchive(usbSlotNumber);

    // Create new archive entry
    auto archive = std::make_shared<RekordboxUsbArchive>();
    archive->usbSlot = usbSlotNumber;
    archive->archivePath = archivePath;
    archive->hasSqliteConnection = false;

    // TODO: Open ZIP archive and parse database
    // This requires libzip or similar ZIP library support
    // For now, we just store the path

    {
        std::lock_guard<std::mutex> lock(archivesMutex_);
        archives_[usbSlotNumber] = archive;
    }

    // Index PSSI data for track matching
    indexPssiFromArchive(usbSlotNumber, *archive);

    // Notify listeners
    notifyArchiveMounted(usbSlotNumber, archivePath);

    return true;
}

void OpusProvider::detachMetadataArchive(int usbSlotNumber) {
    std::shared_ptr<RekordboxUsbArchive> archive;

    {
        std::lock_guard<std::mutex> lock(archivesMutex_);
        auto it = archives_.find(usbSlotNumber);
        if (it != archives_.end()) {
            archive = it->second;
            archives_.erase(it);
        }
    }

    if (archive) {
        removePssiIndex(usbSlotNumber);
        notifyArchiveUnmounted(usbSlotNumber);
    }
}

std::shared_ptr<RekordboxUsbArchive> OpusProvider::getArchive(int usbSlotNumber) const {
    std::lock_guard<std::mutex> lock(archivesMutex_);
    auto it = archives_.find(usbSlotNumber);
    return (it != archives_.end()) ? it->second : nullptr;
}

void OpusProvider::pollAndSendMediaDetails(int /*player*/) {
    // TODO: Check if media details need to be sent for Opus Quad devices
    // This would normally queue media details updates for delivery
}

std::optional<DeviceSqlRekordboxIdAndSlot> OpusProvider::getDeviceSqlRekordboxIdAndSlotNumberFromPssi(
    const uint8_t* pssiData,
    size_t pssiLength,
    int /*rekordboxIdFromOpus*/) const
{
    if (pssiData == nullptr || pssiLength == 0) {
        return std::nullopt;
    }

    // Compute SHA-1 hash of PSSI data
    std::string hash = computeSha1(pssiData, pssiLength);

    // Look up in index
    std::lock_guard<std::mutex> lock(pssiIndexMutex_);
    auto it = pssiToRekordboxIds_.find(hash);
    if (it != pssiToRekordboxIds_.end() && !it->second.empty()) {
        // Return the first match
        return *it->second.begin();
    }

    return std::nullopt;
}

void OpusProvider::addArchiveListener(ArchiveListenerPtr listener) {
    std::lock_guard<std::mutex> lock(listenersMutex_);
    archiveListeners_.push_back(listener);
}

void OpusProvider::removeArchiveListener(ArchiveListenerPtr listener) {
    std::lock_guard<std::mutex> lock(listenersMutex_);
    archiveListeners_.erase(
        std::remove(archiveListeners_.begin(), archiveListeners_.end(), listener),
        archiveListeners_.end());
}

std::shared_ptr<TrackMetadata> OpusProvider::getTrackMetadata(int usbSlot, int rekordboxId) const {
    auto archive = getArchive(usbSlot);
    if (!archive) return nullptr;

    // Open the archive ZIP file
    ZipArchive zip;
    if (!zip.open(archive->archivePath)) return nullptr;

    // Try to extract and parse export.pdb
    auto pdbData = zip.extractToMemory("PIONEER/rekordbox/export.pdb");
    if (pdbData.empty()) return nullptr;

    PdbParser parser;
    if (!parser.parseMemory(pdbData.data(), pdbData.size())) return nullptr;

    // Find the track
    auto track = parser.findTrack(static_cast<uint32_t>(rekordboxId));
    if (!track) return nullptr;

    return parser.toTrackMetadata(*track);
}

std::shared_ptr<BeatGrid> OpusProvider::getBeatGrid(int usbSlot, int rekordboxId) const {
    auto archive = getArchive(usbSlot);
    if (!archive) return nullptr;

    ZipArchive zip;
    if (!zip.open(archive->archivePath)) return nullptr;

    // First get track to find ANLZ path
    auto pdbData = zip.extractToMemory("PIONEER/rekordbox/export.pdb");
    if (pdbData.empty()) return nullptr;

    PdbParser pdbParser;
    if (!pdbParser.parseMemory(pdbData.data(), pdbData.size())) return nullptr;

    auto track = pdbParser.findTrack(static_cast<uint32_t>(rekordboxId));
    if (!track || track->analyzePath.empty()) return nullptr;

    // Extract and parse ANLZ file
    // analyzePath is typically like "/PIONEER/USBANLZ/P016/0000B535/ANLZ0000.DAT"
    std::string anlzPath = track->analyzePath;
    if (!anlzPath.empty() && anlzPath[0] == '/') {
        anlzPath = anlzPath.substr(1);  // Remove leading slash
    }

    auto anlzData = zip.extractToMemory(anlzPath);
    if (anlzData.empty()) return nullptr;

    AnlzParser anlzParser;
    if (!anlzParser.parseMemory(anlzData.data(), anlzData.size())) return nullptr;

    return anlzParser.getBeatGrid();
}

std::shared_ptr<WaveformPreview> OpusProvider::getWaveformPreview(int usbSlot, int rekordboxId) const {
    auto archive = getArchive(usbSlot);
    if (!archive) return nullptr;

    ZipArchive zip;
    if (!zip.open(archive->archivePath)) return nullptr;

    // Get track to find ANLZ path
    auto pdbData = zip.extractToMemory("PIONEER/rekordbox/export.pdb");
    if (pdbData.empty()) return nullptr;

    PdbParser pdbParser;
    if (!pdbParser.parseMemory(pdbData.data(), pdbData.size())) return nullptr;

    auto track = pdbParser.findTrack(static_cast<uint32_t>(rekordboxId));
    if (!track || track->analyzePath.empty()) return nullptr;

    // Try .EXT file first for color waveform
    std::string extPath = track->analyzePath;
    if (extPath.size() > 4) {
        extPath = extPath.substr(0, extPath.size() - 4) + ".EXT";
    }
    if (!extPath.empty() && extPath[0] == '/') {
        extPath = extPath.substr(1);
    }

    auto extData = zip.extractToMemory(extPath);
    if (!extData.empty()) {
        AnlzParser anlzParser;
        if (anlzParser.parseMemory(extData.data(), extData.size())) {
            auto preview = anlzParser.getColorWaveformPreview();
            if (preview) return preview;
        }
    }

    // Fall back to .DAT file
    std::string datPath = track->analyzePath;
    if (!datPath.empty() && datPath[0] == '/') {
        datPath = datPath.substr(1);
    }

    auto datData = zip.extractToMemory(datPath);
    if (datData.empty()) return nullptr;

    AnlzParser anlzParser;
    if (!anlzParser.parseMemory(datData.data(), datData.size())) return nullptr;

    return anlzParser.getWaveformPreview();
}

std::shared_ptr<WaveformDetail> OpusProvider::getWaveformDetail(int usbSlot, int rekordboxId) const {
    auto archive = getArchive(usbSlot);
    if (!archive) return nullptr;

    ZipArchive zip;
    if (!zip.open(archive->archivePath)) return nullptr;

    // Get track to find ANLZ path
    auto pdbData = zip.extractToMemory("PIONEER/rekordbox/export.pdb");
    if (pdbData.empty()) return nullptr;

    PdbParser pdbParser;
    if (!pdbParser.parseMemory(pdbData.data(), pdbData.size())) return nullptr;

    auto track = pdbParser.findTrack(static_cast<uint32_t>(rekordboxId));
    if (!track || track->analyzePath.empty()) return nullptr;

    // Try .EXT file first for color waveform
    std::string extPath = track->analyzePath;
    if (extPath.size() > 4) {
        extPath = extPath.substr(0, extPath.size() - 4) + ".EXT";
    }
    if (!extPath.empty() && extPath[0] == '/') {
        extPath = extPath.substr(1);
    }

    auto extData = zip.extractToMemory(extPath);
    if (!extData.empty()) {
        AnlzParser anlzParser;
        if (anlzParser.parseMemory(extData.data(), extData.size())) {
            auto detail = anlzParser.getColorWaveformDetail();
            if (detail) return detail;
        }
    }

    // Fall back to .DAT file
    std::string datPath = track->analyzePath;
    if (!datPath.empty() && datPath[0] == '/') {
        datPath = datPath.substr(1);
    }

    auto datData = zip.extractToMemory(datPath);
    if (datData.empty()) return nullptr;

    AnlzParser anlzParser;
    if (!anlzParser.parseMemory(datData.data(), datData.size())) return nullptr;

    return anlzParser.getWaveformDetail();
}

std::shared_ptr<AlbumArt> OpusProvider::getAlbumArt(int usbSlot, int artworkId) const {
    auto archive = getArchive(usbSlot);
    if (!archive) return nullptr;

    ZipArchive zip;
    if (!zip.open(archive->archivePath)) return nullptr;

    // Get artwork path from PDB
    auto pdbData = zip.extractToMemory("PIONEER/rekordbox/export.pdb");
    if (pdbData.empty()) return nullptr;

    PdbParser pdbParser;
    if (!pdbParser.parseMemory(pdbData.data(), pdbData.size())) return nullptr;

    auto artwork = pdbParser.findArtwork(static_cast<uint32_t>(artworkId));
    if (!artwork || artwork->path.empty()) return nullptr;

    // Extract artwork file
    std::string artPath = artwork->path;
    if (!artPath.empty() && artPath[0] == '/') {
        artPath = artPath.substr(1);
    }

    auto artData = zip.extractToMemory(artPath);
    if (artData.empty()) return nullptr;

    // TODO: Create AlbumArt from image data
    // For now return nullptr as AlbumArt construction needs image parsing
    (void)artData;
    return nullptr;
}

void OpusProvider::indexPssiFromArchive(int /*usbSlot*/, const RekordboxUsbArchive& /*archive*/) {
    // TODO: Read PSSI data from archive and build index
    // This requires parsing the analysis files from the archive
}

void OpusProvider::removePssiIndex(int usbSlot) {
    std::lock_guard<std::mutex> lock(pssiIndexMutex_);

    // Remove all entries for this slot
    for (auto it = pssiToRekordboxIds_.begin(); it != pssiToRekordboxIds_.end(); ) {
        auto& idSet = it->second;
        for (auto setIt = idSet.begin(); setIt != idSet.end(); ) {
            if (setIt->usbSlot == usbSlot) {
                setIt = idSet.erase(setIt);
            } else {
                ++setIt;
            }
        }
        if (idSet.empty()) {
            it = pssiToRekordboxIds_.erase(it);
        } else {
            ++it;
        }
    }
}

void OpusProvider::notifyArchiveMounted(int usbSlot, const std::string& archivePath) {
    std::vector<ArchiveListenerPtr> listeners;
    {
        std::lock_guard<std::mutex> lock(listenersMutex_);
        listeners = archiveListeners_;
    }

    for (const auto& listener : listeners) {
        listener->archiveMounted(usbSlot, archivePath);
    }
}

void OpusProvider::notifyArchiveUnmounted(int usbSlot) {
    std::vector<ArchiveListenerPtr> listeners;
    {
        std::lock_guard<std::mutex> lock(listenersMutex_);
        listeners = archiveListeners_;
    }

    for (const auto& listener : listeners) {
        listener->archiveUnmounted(usbSlot);
    }
}

} // namespace beatlink::data
