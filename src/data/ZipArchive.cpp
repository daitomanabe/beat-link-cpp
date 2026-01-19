#include "beatlink/data/ZipArchive.hpp"

#include <miniz.h>
#include <utf8proc.h>

#include <cstring>
#include <fstream>

namespace beatlink::data {

// =============================================================================
// Implementation struct using miniz
// =============================================================================

struct ZipArchive::Impl {
    mz_zip_archive zip{};
    bool isOpen = false;
    std::vector<uint8_t> memoryData;  // Holds data for memory-based archives

    ~Impl() {
        if (isOpen) {
            mz_zip_reader_end(&zip);
        }
    }
};

// =============================================================================
// ZipArchive
// =============================================================================

ZipArchive::ZipArchive() : impl_(std::make_unique<Impl>()) {}

ZipArchive::~ZipArchive() = default;

ZipArchive::ZipArchive(ZipArchive&& other) noexcept
    : impl_(std::move(other.impl_)), path_(std::move(other.path_)) {}

ZipArchive& ZipArchive::operator=(ZipArchive&& other) noexcept {
    if (this != &other) {
        impl_ = std::move(other.impl_);
        path_ = std::move(other.path_);
    }
    return *this;
}

bool ZipArchive::open(const std::string& path) {
    close();

    impl_ = std::make_unique<Impl>();
    memset(&impl_->zip, 0, sizeof(impl_->zip));

    if (!mz_zip_reader_init_file(&impl_->zip, path.c_str(), 0)) {
        return false;
    }

    impl_->isOpen = true;
    path_ = path;
    return true;
}

bool ZipArchive::openFromMemory(const uint8_t* data, size_t size) {
    close();

    impl_ = std::make_unique<Impl>();
    memset(&impl_->zip, 0, sizeof(impl_->zip));

    // Copy data since miniz doesn't take ownership
    impl_->memoryData.assign(data, data + size);

    if (!mz_zip_reader_init_mem(&impl_->zip, impl_->memoryData.data(),
                                 impl_->memoryData.size(), 0)) {
        return false;
    }

    impl_->isOpen = true;
    path_.clear();
    return true;
}

void ZipArchive::close() {
    if (impl_ && impl_->isOpen) {
        mz_zip_reader_end(&impl_->zip);
        impl_->isOpen = false;
        impl_->memoryData.clear();
    }
    path_.clear();
}

bool ZipArchive::isOpen() const {
    return impl_ && impl_->isOpen;
}

size_t ZipArchive::getEntryCount() const {
    if (!isOpen()) return 0;
    return mz_zip_reader_get_num_files(&impl_->zip);
}

std::optional<ZipEntry> ZipArchive::getEntry(size_t index) const {
    if (!isOpen()) return std::nullopt;

    mz_zip_archive_file_stat stat;
    if (!mz_zip_reader_file_stat(&impl_->zip, static_cast<mz_uint>(index), &stat)) {
        return std::nullopt;
    }

    ZipEntry entry;
    entry.filename = stat.m_filename;
    entry.uncompressedSize = static_cast<size_t>(stat.m_uncomp_size);
    entry.compressedSize = static_cast<size_t>(stat.m_comp_size);
    entry.isDirectory = mz_zip_reader_is_file_a_directory(&impl_->zip,
                                                           static_cast<mz_uint>(index));
    entry.crc32 = stat.m_crc32;

    return entry;
}

bool ZipArchive::exists(const std::string& filename) const {
    return findFile(filename) >= 0;
}

int ZipArchive::findFile(const std::string& filename) const {
    if (!isOpen()) return -1;

    int index = mz_zip_reader_locate_file(&impl_->zip, filename.c_str(), nullptr, 0);
    return index;
}

std::vector<uint8_t> ZipArchive::extractToMemory(const std::string& filename) const {
    int index = findFile(filename);
    if (index < 0) return {};
    return extractToMemory(static_cast<size_t>(index));
}

std::vector<uint8_t> ZipArchive::extractToMemory(size_t index) const {
    if (!isOpen()) return {};

    mz_zip_archive_file_stat stat;
    if (!mz_zip_reader_file_stat(&impl_->zip, static_cast<mz_uint>(index), &stat)) {
        return {};
    }

    std::vector<uint8_t> data(static_cast<size_t>(stat.m_uncomp_size));

    if (!mz_zip_reader_extract_to_mem(&impl_->zip, static_cast<mz_uint>(index),
                                       data.data(), data.size(), 0)) {
        return {};
    }

    return data;
}

bool ZipArchive::extractToFile(const std::string& filename,
                                const std::string& destPath) const {
    if (!isOpen()) return false;

    int index = findFile(filename);
    if (index < 0) return false;

    return mz_zip_reader_extract_to_file(&impl_->zip, static_cast<mz_uint>(index),
                                          destPath.c_str(), 0);
}

std::vector<std::string> ZipArchive::listFiles(bool includeDirectories) const {
    std::vector<std::string> files;
    if (!isOpen()) return files;

    size_t count = getEntryCount();
    files.reserve(count);

    for (size_t i = 0; i < count; ++i) {
        bool isDir = mz_zip_reader_is_file_a_directory(&impl_->zip,
                                                        static_cast<mz_uint>(i));
        if (!includeDirectories && isDir) continue;

        mz_zip_archive_file_stat stat;
        if (mz_zip_reader_file_stat(&impl_->zip, static_cast<mz_uint>(i), &stat)) {
            files.emplace_back(stat.m_filename);
        }
    }

    return files;
}

void ZipArchive::forEachEntry(std::function<bool(const ZipEntry&)> callback) const {
    if (!isOpen()) return;

    size_t count = getEntryCount();
    for (size_t i = 0; i < count; ++i) {
        auto entry = getEntry(i);
        if (entry) {
            if (!callback(*entry)) break;
        }
    }
}

// =============================================================================
// HFS+ Path Normalization
// =============================================================================

std::string normalizeForHfsPlus(const std::string& path) {
    // Use utf8proc to convert NFD (decomposed) to NFC (composed)
    utf8proc_uint8_t* normalized = utf8proc_NFC(
        reinterpret_cast<const utf8proc_uint8_t*>(path.c_str()));

    if (!normalized) {
        return path;  // Return original if normalization fails
    }

    std::string result(reinterpret_cast<const char*>(normalized));
    free(normalized);

    return result;
}

} // namespace beatlink::data
