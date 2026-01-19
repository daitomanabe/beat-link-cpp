#pragma once

/**
 * OpusProvider - Metadata provider for Opus Quad and XDJ-AZ devices
 *
 * Ported from: org.deepsymmetry.beatlink.data.OpusProvider
 *
 * This class allows users to attach metadata archives created by the Crate Digger
 * library to provide metadata when the corresponding USB is mounted in an Opus Quad,
 * which is not capable of providing most metadata on its own.
 *
 * SQLite database access requires linking with sqlite3 (and optionally sqlcipher
 * for encrypted database support).
 */

#include <atomic>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <set>
#include <string>
#include <vector>

namespace beatlink::data {

// Forward declarations
class Database;
class MediaDetails;
class TrackMetadata;
class BeatGrid;
class WaveformPreview;
class WaveformDetail;
class CueList;
class AlbumArt;

/**
 * Device name constants for Opus Quad and XDJ-AZ devices.
 */
namespace OpusDeviceNames {
    constexpr const char* OPUS_QUAD = "OPUS-QUAD";
    constexpr const char* XDJ_AZ = "XDJ-AZ";
}

/**
 * Container for rekordbox ID and USB slot number.
 * Used for matching tracks via PSSI (song structure).
 */
struct DeviceSqlRekordboxIdAndSlot {
    int rekordboxId{0};
    int usbSlot{0};

    bool operator==(const DeviceSqlRekordboxIdAndSlot& other) const {
        return rekordboxId == other.rekordboxId && usbSlot == other.usbSlot;
    }

    bool operator<(const DeviceSqlRekordboxIdAndSlot& other) const {
        if (usbSlot != other.usbSlot) {
            return usbSlot < other.usbSlot;
        }
        return rekordboxId < other.rekordboxId;
    }
};

/**
 * Container for USB archive information.
 */
struct RekordboxUsbArchive {
    int usbSlot{0};
    std::shared_ptr<Database> database;
    std::string archivePath;
    bool hasSqliteConnection{false};
};

/**
 * Listener interface for archive mount/unmount events.
 */
class ArchiveListener {
public:
    virtual ~ArchiveListener() = default;
    virtual void archiveMounted(int usbSlot, const std::string& archivePath) = 0;
    virtual void archiveUnmounted(int usbSlot) = 0;
};

using ArchiveListenerPtr = std::shared_ptr<ArchiveListener>;

/**
 * Callback-based archive listener.
 */
class ArchiveCallbacks : public ArchiveListener {
public:
    using MountCallback = std::function<void(int, const std::string&)>;
    using UnmountCallback = std::function<void(int)>;

    ArchiveCallbacks(MountCallback onMount, UnmountCallback onUnmount)
        : onMount_(std::move(onMount)), onUnmount_(std::move(onUnmount)) {}

    void archiveMounted(int usbSlot, const std::string& archivePath) override {
        if (onMount_) onMount_(usbSlot, archivePath);
    }

    void archiveUnmounted(int usbSlot) override {
        if (onUnmount_) onUnmount_(usbSlot);
    }

private:
    MountCallback onMount_;
    UnmountCallback onUnmount_;
};

/**
 * OpusProvider - Provides metadata for Opus Quad and XDJ-AZ devices.
 *
 * These devices cannot provide track metadata via the standard DJ Link protocol,
 * so this class allows attaching pre-created metadata archives to supply the
 * necessary information.
 */
class OpusProvider {
public:
    /**
     * Get the singleton instance.
     */
    static OpusProvider& getInstance() {
        static OpusProvider instance;
        return instance;
    }

    // Non-copyable
    OpusProvider(const OpusProvider&) = delete;
    OpusProvider& operator=(const OpusProvider&) = delete;

    /**
     * Check if the provider is running.
     */
    bool isRunning() const { return running_.load(); }

    /**
     * Check if we are using Device Library Plus (SQLite) databases.
     */
    bool usingDeviceLibraryPlus() const { return usingDeviceLibraryPlus_.load(); }

    /**
     * Start the provider.
     */
    void start() { running_.store(true); }

    /**
     * Stop the provider.
     */
    void stop() { running_.store(false); }

    /**
     * Set the database key for encrypted SQLite databases.
     *
     * If the key is known and correct, the provider can access the newer
     * Device Library Plus databases which contain more reliable metadata.
     *
     * @param key The encryption key for exportLibrary.db files
     */
    void setDatabaseKey(const std::string& key) {
        usingDeviceLibraryPlus_.store(!key.empty());
        databaseKey_ = key;
    }

    /**
     * Get the current database key.
     */
    const std::string& getDatabaseKey() const { return databaseKey_; }

    /**
     * Attach a metadata archive for a USB slot.
     *
     * The archive should be created using Crate Digger's Archivist.createArchive()
     * from the rekordbox USB media.
     *
     * @param archivePath Path to the metadata archive (ZIP file)
     * @param usbSlotNumber USB slot number (1, 2, or 3)
     * @return true if the archive was successfully attached
     */
    bool attachMetadataArchive(const std::string& archivePath, int usbSlotNumber);

    /**
     * Detach the metadata archive from a USB slot.
     *
     * @param usbSlotNumber USB slot number (1, 2, or 3)
     */
    void detachMetadataArchive(int usbSlotNumber);

    /**
     * Get the archive attached to a USB slot.
     *
     * @param usbSlotNumber USB slot number (1, 2, or 3)
     * @return The archive, or nullptr if none attached
     */
    std::shared_ptr<RekordboxUsbArchive> getArchive(int usbSlotNumber) const;

    /**
     * Check if a device name indicates an Opus Quad.
     */
    static bool isOpusQuad(const std::string& deviceName) {
        return deviceName == OpusDeviceNames::OPUS_QUAD;
    }

    /**
     * Check if a device name indicates an XDJ-AZ.
     */
    static bool isXdjAz(const std::string& deviceName) {
        return deviceName == OpusDeviceNames::XDJ_AZ;
    }

    /**
     * Check if a device name indicates a device that needs OpusProvider.
     */
    static bool needsOpusProvider(const std::string& deviceName) {
        return isOpusQuad(deviceName) || isXdjAz(deviceName);
    }

    /**
     * Poll and send media details for a player.
     * Called periodically to check if media details need to be sent.
     *
     * @param player The player number
     */
    void pollAndSendMediaDetails(int player);

    /**
     * Get the DeviceSQL rekordbox ID and slot number from PSSI data.
     *
     * Uses the song structure (PSSI) hash to match tracks across different
     * databases. This allows identifying tracks even when the rekordbox ID
     * differs between the Opus Quad's internal ID and the archive's ID.
     *
     * @param pssiData The PSSI data bytes
     * @param pssiLength Length of the PSSI data
     * @param rekordboxIdFromOpus The rekordbox ID reported by the Opus Quad
     * @return The matched rekordbox ID and slot, or nullopt if not found
     */
    std::optional<DeviceSqlRekordboxIdAndSlot> getDeviceSqlRekordboxIdAndSlotNumberFromPssi(
        const uint8_t* pssiData,
        size_t pssiLength,
        int rekordboxIdFromOpus) const;

    /**
     * Overload taking a vector.
     */
    std::optional<DeviceSqlRekordboxIdAndSlot> getDeviceSqlRekordboxIdAndSlotNumberFromPssi(
        const std::vector<uint8_t>& pssiData,
        int rekordboxIdFromOpus) const
    {
        return getDeviceSqlRekordboxIdAndSlotNumberFromPssi(
            pssiData.data(), pssiData.size(), rekordboxIdFromOpus);
    }

    /**
     * Add an archive listener.
     */
    void addArchiveListener(ArchiveListenerPtr listener);

    /**
     * Remove an archive listener.
     */
    void removeArchiveListener(ArchiveListenerPtr listener);

    /**
     * Get track metadata from an archive.
     *
     * @param usbSlot The USB slot number
     * @param rekordboxId The rekordbox track ID
     * @return The track metadata, or nullptr if not found
     */
    std::shared_ptr<TrackMetadata> getTrackMetadata(int usbSlot, int rekordboxId) const;

    /**
     * Get beat grid from an archive.
     *
     * @param usbSlot The USB slot number
     * @param rekordboxId The rekordbox track ID
     * @return The beat grid, or nullptr if not found
     */
    std::shared_ptr<BeatGrid> getBeatGrid(int usbSlot, int rekordboxId) const;

    /**
     * Get waveform preview from an archive.
     *
     * @param usbSlot The USB slot number
     * @param rekordboxId The rekordbox track ID
     * @return The waveform preview, or nullptr if not found
     */
    std::shared_ptr<WaveformPreview> getWaveformPreview(int usbSlot, int rekordboxId) const;

    /**
     * Get waveform detail from an archive.
     *
     * @param usbSlot The USB slot number
     * @param rekordboxId The rekordbox track ID
     * @return The waveform detail, or nullptr if not found
     */
    std::shared_ptr<WaveformDetail> getWaveformDetail(int usbSlot, int rekordboxId) const;

    /**
     * Get album art from an archive.
     *
     * @param usbSlot The USB slot number
     * @param artworkId The artwork ID
     * @return The album art, or nullptr if not found
     */
    std::shared_ptr<AlbumArt> getAlbumArt(int usbSlot, int artworkId) const;

private:
    OpusProvider() = default;

    /**
     * Compute SHA-1 hash of data for PSSI matching.
     */
    std::string computeSha1(const uint8_t* data, size_t length) const;

    /**
     * Index PSSI data from an archive for track matching.
     */
    void indexPssiFromArchive(int usbSlot, const RekordboxUsbArchive& archive);

    /**
     * Remove PSSI index entries for a slot.
     */
    void removePssiIndex(int usbSlot);

    /**
     * Notify archive listeners of a mount event.
     */
    void notifyArchiveMounted(int usbSlot, const std::string& archivePath);

    /**
     * Notify archive listeners of an unmount event.
     */
    void notifyArchiveUnmounted(int usbSlot);

    std::atomic<bool> running_{false};
    std::atomic<bool> usingDeviceLibraryPlus_{false};
    std::string databaseKey_;

    mutable std::mutex archivesMutex_;
    std::map<int, std::shared_ptr<RekordboxUsbArchive>> archives_;

    mutable std::mutex pssiIndexMutex_;
    std::map<std::string, std::set<DeviceSqlRekordboxIdAndSlot>> pssiToRekordboxIds_;

    mutable std::mutex listenersMutex_;
    std::vector<ArchiveListenerPtr> archiveListeners_;
};

} // namespace beatlink::data
