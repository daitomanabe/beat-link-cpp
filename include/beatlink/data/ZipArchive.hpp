#pragma once

/**
 * ZipArchive - ZIP archive handling using miniz
 *
 * Provides a simple C++ wrapper for reading ZIP archives,
 * used by OpusProvider to access metadata archives from Opus Quad/XDJ-AZ.
 */

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>
#include <functional>

namespace beatlink::data {

/**
 * Entry information in a ZIP archive
 */
struct ZipEntry {
    std::string filename;
    size_t uncompressedSize;
    size_t compressedSize;
    bool isDirectory;
    uint32_t crc32;
};

/**
 * ZIP archive reader class using miniz
 */
class ZipArchive {
public:
    ZipArchive();
    ~ZipArchive();

    // Non-copyable, movable
    ZipArchive(const ZipArchive&) = delete;
    ZipArchive& operator=(const ZipArchive&) = delete;
    ZipArchive(ZipArchive&& other) noexcept;
    ZipArchive& operator=(ZipArchive&& other) noexcept;

    /**
     * Open a ZIP archive from a file path
     * @param path Path to the ZIP file
     * @return true if successful
     */
    bool open(const std::string& path);

    /**
     * Open a ZIP archive from memory
     * @param data Pointer to ZIP data
     * @param size Size of the data
     * @return true if successful
     */
    bool openFromMemory(const uint8_t* data, size_t size);

    /**
     * Close the archive
     */
    void close();

    /**
     * Check if archive is open
     */
    bool isOpen() const;

    /**
     * Get the number of entries in the archive
     */
    size_t getEntryCount() const;

    /**
     * Get entry information by index
     */
    std::optional<ZipEntry> getEntry(size_t index) const;

    /**
     * Check if a file exists in the archive
     * @param filename Path within the archive
     */
    bool exists(const std::string& filename) const;

    /**
     * Find a file index by name
     * @param filename Path within the archive
     * @return Index if found, or -1 if not found
     */
    int findFile(const std::string& filename) const;

    /**
     * Extract a file to memory
     * @param filename Path within the archive
     * @return File contents, or empty vector if not found
     */
    std::vector<uint8_t> extractToMemory(const std::string& filename) const;

    /**
     * Extract a file to memory by index
     * @param index Entry index
     * @return File contents, or empty vector if failed
     */
    std::vector<uint8_t> extractToMemory(size_t index) const;

    /**
     * Extract a file to disk
     * @param filename Path within the archive
     * @param destPath Destination file path
     * @return true if successful
     */
    bool extractToFile(const std::string& filename, const std::string& destPath) const;

    /**
     * List all files in the archive
     * @param includeDirectories Whether to include directory entries
     * @return List of filenames
     */
    std::vector<std::string> listFiles(bool includeDirectories = false) const;

    /**
     * Iterate over all entries
     * @param callback Called for each entry, return false to stop
     */
    void forEachEntry(std::function<bool(const ZipEntry&)> callback) const;

    /**
     * Get the path of the opened archive
     */
    const std::string& getPath() const { return path_; }

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
    std::string path_;
};

/**
 * Normalize a path for HFS+ filesystem compatibility
 * Converts NFD (decomposed) Unicode to NFC (composed) form
 * @param path Path to normalize
 * @return Normalized path
 */
std::string normalizeForHfsPlus(const std::string& path);

} // namespace beatlink::data
