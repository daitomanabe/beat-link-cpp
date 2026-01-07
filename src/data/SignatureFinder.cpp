#include "beatlink/data/SignatureFinder.hpp"

#include "beatlink/Util.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <sstream>

namespace beatlink::data {

namespace {
constexpr size_t kMaxPendingUpdates = 20;

class Sha1 {
public:
    Sha1() { reset(); }

    void reset() {
        h0_ = 0x67452301;
        h1_ = 0xefcdab89;
        h2_ = 0x98badcfe;
        h3_ = 0x10325476;
        h4_ = 0xc3d2e1f0;
        messageBits_ = 0;
        bufferSize_ = 0;
    }

    void update(const uint8_t* data, size_t length) {
        if (!data || length == 0) {
            return;
        }
        messageBits_ += static_cast<uint64_t>(length) * 8;
        size_t offset = 0;
        while (offset < length) {
            const size_t toCopy = std::min(length - offset, sizeof(buffer_) - bufferSize_);
            std::memcpy(buffer_.data() + bufferSize_, data + offset, toCopy);
            bufferSize_ += toCopy;
            offset += toCopy;
            if (bufferSize_ == buffer_.size()) {
                processBlock(buffer_.data());
                bufferSize_ = 0;
            }
        }
    }

    void update(const std::string& value) {
        update(reinterpret_cast<const uint8_t*>(value.data()), value.size());
    }

    std::array<uint8_t, 20> final() {
        const uint64_t totalBits = messageBits_;
        std::array<uint8_t, 64> pad{};
        pad[0] = 0x80;
        const size_t padLen = (bufferSize_ < 56) ? (56 - bufferSize_) : (120 - bufferSize_);
        update(pad.data(), padLen);

        std::array<uint8_t, 8> lengthBytes{};
        uint64_t length = totalBits;
        for (int i = 7; i >= 0; --i) {
            lengthBytes[static_cast<size_t>(i)] = static_cast<uint8_t>(length & 0xff);
            length >>= 8;
        }
        update(lengthBytes.data(), lengthBytes.size());

        std::array<uint8_t, 20> digest{};
        writeUint32(h0_, digest.data(), 0);
        writeUint32(h1_, digest.data(), 4);
        writeUint32(h2_, digest.data(), 8);
        writeUint32(h3_, digest.data(), 12);
        writeUint32(h4_, digest.data(), 16);
        return digest;
    }

private:
    void processBlock(const uint8_t* block) {
        uint32_t w[80];
        for (int i = 0; i < 16; ++i) {
            w[i] = readUint32(block, static_cast<size_t>(i) * 4);
        }
        for (int i = 16; i < 80; ++i) {
            w[i] = rol(w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16], 1);
        }

        uint32_t a = h0_;
        uint32_t b = h1_;
        uint32_t c = h2_;
        uint32_t d = h3_;
        uint32_t e = h4_;

        for (int i = 0; i < 80; ++i) {
            uint32_t f = 0;
            uint32_t k = 0;
            if (i < 20) {
                f = (b & c) | ((~b) & d);
                k = 0x5a827999;
            } else if (i < 40) {
                f = b ^ c ^ d;
                k = 0x6ed9eba1;
            } else if (i < 60) {
                f = (b & c) | (b & d) | (c & d);
                k = 0x8f1bbcdc;
            } else {
                f = b ^ c ^ d;
                k = 0xca62c1d6;
            }

            const uint32_t temp = rol(a, 5) + f + e + k + w[i];
            e = d;
            d = c;
            c = rol(b, 30);
            b = a;
            a = temp;
        }

        h0_ += a;
        h1_ += b;
        h2_ += c;
        h3_ += d;
        h4_ += e;
    }

    static uint32_t rol(uint32_t value, uint32_t bits) {
        return (value << bits) | (value >> (32 - bits));
    }

    static uint32_t readUint32(const uint8_t* data, size_t offset) {
        return (static_cast<uint32_t>(data[offset]) << 24) |
               (static_cast<uint32_t>(data[offset + 1]) << 16) |
               (static_cast<uint32_t>(data[offset + 2]) << 8) |
               static_cast<uint32_t>(data[offset + 3]);
    }

    static void writeUint32(uint32_t value, uint8_t* buffer, size_t offset) {
        buffer[offset] = static_cast<uint8_t>((value >> 24) & 0xff);
        buffer[offset + 1] = static_cast<uint8_t>((value >> 16) & 0xff);
        buffer[offset + 2] = static_cast<uint8_t>((value >> 8) & 0xff);
        buffer[offset + 3] = static_cast<uint8_t>(value & 0xff);
    }

    uint32_t h0_{0};
    uint32_t h1_{0};
    uint32_t h2_{0};
    uint32_t h3_{0};
    uint32_t h4_{0};
    uint64_t messageBits_{0};
    std::array<uint8_t, 64> buffer_{};
    size_t bufferSize_{0};
};

} // namespace

SignatureFinder& SignatureFinder::getInstance() {
    static SignatureFinder instance;
    return instance;
}

SignatureFinder::SignatureFinder() {
    metadataListener_ = std::make_shared<TrackMetadataCallbacks>(
        [this](const TrackMetadataUpdate& update) {
            if (!update.metadata) {
                clearSignature(update.player);
            } else {
                checkIfSignatureReady(update.player);
            }
        });

    rgbWaveformListener_ = std::make_shared<AnalysisTagCallbacks>(
        [this](const AnalysisTagUpdate& update) {
            if (!update.analysisTag) {
                std::lock_guard<std::mutex> lock(waveformsMutex_);
                rgbWaveforms_.erase(update.player);
                clearSignature(update.player);
                return;
            }
            auto detail = waveformFromAnalysisTag(update.analysisTag);
            if (!detail) {
                clearSignature(update.player);
                return;
            }
            {
                std::lock_guard<std::mutex> lock(waveformsMutex_);
                rgbWaveforms_[update.player] = detail;
            }
            checkIfSignatureReady(update.player);
        });

    beatGridListener_ = std::make_shared<BeatGridCallbacks>(
        [this](const BeatGridUpdate& update) {
            if (!update.beatGrid) {
                clearSignature(update.player);
            } else {
                checkIfSignatureReady(update.player);
            }
        });

    lifecycleListener_ = std::make_shared<beatlink::LifecycleCallbacks>(
        [](beatlink::LifecycleParticipant&) {},
        [this](beatlink::LifecycleParticipant&) {
            if (isRunning()) {
                stop();
            }
        });
}

std::unordered_map<int, std::string> SignatureFinder::getSignatures() {
    ensureRunning();
    std::lock_guard<std::mutex> lock(signaturesMutex_);
    return signatures_;
}

std::string SignatureFinder::getLatestSignatureFor(int player) {
    ensureRunning();
    std::lock_guard<std::mutex> lock(signaturesMutex_);
    auto it = signatures_.find(player);
    if (it == signatures_.end()) {
        return {};
    }
    return it->second;
}

std::string SignatureFinder::getLatestSignatureFor(const beatlink::DeviceUpdate& update) {
    return getLatestSignatureFor(update.getDeviceNumber());
}

void SignatureFinder::addSignatureListener(const SignatureListenerPtr& listener) {
    if (!listener) {
        return;
    }
    std::lock_guard<std::mutex> lock(listenersMutex_);
    signatureListeners_.push_back(listener);
}

void SignatureFinder::removeSignatureListener(const SignatureListenerPtr& listener) {
    if (!listener) {
        return;
    }
    std::lock_guard<std::mutex> lock(listenersMutex_);
    signatureListeners_.erase(
        std::remove(signatureListeners_.begin(), signatureListeners_.end(), listener),
        signatureListeners_.end());
}

std::vector<SignatureListenerPtr> SignatureFinder::getSignatureListeners() const {
    std::lock_guard<std::mutex> lock(listenersMutex_);
    return signatureListeners_;
}

std::string SignatureFinder::computeTrackSignature(const std::string& title,
                                                   const std::optional<SearchableItem>& artist,
                                                   int duration,
                                                   const WaveformDetail& waveformDetail,
                                                   const BeatGrid& beatGrid) {
    if (waveformDetail.getStyle() != WaveformStyle::RGB) {
        throw std::invalid_argument("Signatures can only be computed from RGB waveform details.");
    }

    Sha1 digest;
    const std::string safeTitle = title;
    const std::string artistName = artist ? artist->getLabel() : "[no artist]";

    digest.update(safeTitle);
    const uint8_t nullByte = 0;
    digest.update(&nullByte, 1);
    digest.update(artistName);
    digest.update(&nullByte, 1);

    std::array<uint8_t, 4> intBytes{};
    beatlink::Util::numberToBytes(duration, intBytes.data(), intBytes.size(), 0, intBytes.size());
    digest.update(intBytes.data(), intBytes.size());

    auto data = waveformDetail.getData();
    digest.update(data.data(), data.size());

    for (int beat = 1; beat <= beatGrid.getBeatCount(); ++beat) {
        beatlink::Util::numberToBytes(beatGrid.getBeatWithinBar(beat), intBytes.data(), intBytes.size(), 0, intBytes.size());
        digest.update(intBytes.data(), intBytes.size());
        beatlink::Util::numberToBytes(static_cast<int>(beatGrid.getTimeWithinTrack(beat)),
                                      intBytes.data(),
                                      intBytes.size(),
                                      0,
                                      intBytes.size());
        digest.update(intBytes.data(), intBytes.size());
    }

    auto hash = digest.final();
    std::ostringstream oss;
    for (const auto byte : hash) {
        oss << std::hex << std::setw(2) << std::setfill('0')
            << static_cast<int>(byte);
    }
    return oss.str();
}

void SignatureFinder::start() {
    if (isRunning()) {
        return;
    }

    MetadataFinder::getInstance().addLifecycleListener(lifecycleListener_);
    MetadataFinder::getInstance().start();
    MetadataFinder::getInstance().addTrackMetadataListener(metadataListener_);

    AnalysisTagFinder::getInstance().addLifecycleListener(lifecycleListener_);
    AnalysisTagFinder::getInstance().start();
    AnalysisTagFinder::getInstance().addAnalysisTagListener(rgbWaveformListener_, ".EXT", "PWV5");

    BeatGridFinder::getInstance().addLifecycleListener(lifecycleListener_);
    BeatGridFinder::getInstance().start();
    BeatGridFinder::getInstance().addBeatGridListener(beatGridListener_);

    running_.store(true);
    queueHandler_ = std::thread([this]() {
        while (isRunning()) {
            std::unique_lock<std::mutex> lock(pendingMutex_);
            pendingCv_.wait(lock, [this]() { return !pendingUpdates_.empty() || !isRunning(); });
            if (!isRunning()) {
                break;
            }
            int player = pendingUpdates_.front();
            pendingUpdates_.pop_front();
            lock.unlock();

            try {
                handleUpdate(player);
            } catch (const std::exception& e) {
                std::cerr << "SignatureFinder: Problem processing signature update: " << e.what() << std::endl;
            }
        }
    });

    deliverLifecycleAnnouncement(true);
    for (const auto& [deck, metadata] : MetadataFinder::getInstance().getLoadedTracks()) {
        if (deck.getHotCue() == 0) {
            checkIfSignatureReady(deck.getPlayer());
        }
    }
}

void SignatureFinder::stop() {
    if (!isRunning()) {
        return;
    }

    MetadataFinder::getInstance().removeTrackMetadataListener(metadataListener_);
    AnalysisTagFinder::getInstance().removeAnalysisTagListener(rgbWaveformListener_, ".EXT", "PWV5");
    BeatGridFinder::getInstance().removeBeatGridListener(beatGridListener_);

    running_.store(false);
    pendingCv_.notify_all();
    if (queueHandler_.joinable()) {
        queueHandler_.join();
    }
    {
        std::lock_guard<std::mutex> lock(pendingMutex_);
        pendingUpdates_.clear();
    }

    std::vector<int> removedPlayers;
    {
        std::lock_guard<std::mutex> lock(signaturesMutex_);
        for (const auto& [player, signature] : signatures_) {
            removedPlayers.push_back(player);
        }
        signatures_.clear();
    }

    for (int player : removedPlayers) {
        deliverSignatureUpdate(player, {});
    }

    deliverLifecycleAnnouncement(false);
}

void SignatureFinder::clearSignature(int player) {
    bool removed = false;
    {
        std::lock_guard<std::mutex> lock(signaturesMutex_);
        removed = signatures_.erase(player) > 0;
    }
    if (removed) {
        deliverSignatureUpdate(player, {});
    }
}

void SignatureFinder::checkIfSignatureReady(int player) {
    std::lock_guard<std::mutex> lock(pendingMutex_);
    if (pendingUpdates_.size() >= kMaxPendingUpdates) {
        pendingUpdates_.pop_front();
    }
    pendingUpdates_.push_back(player);
    pendingCv_.notify_one();
}

void SignatureFinder::handleUpdate(int player) {
    auto metadata = MetadataFinder::getInstance().getLatestMetadataFor(player);
    if (!metadata) {
        return;
    }

    std::shared_ptr<WaveformDetail> waveform;
    {
        std::lock_guard<std::mutex> lock(waveformsMutex_);
        auto it = rgbWaveforms_.find(player);
        if (it != rgbWaveforms_.end()) {
            waveform = it->second;
        }
    }

    auto beatGrid = BeatGridFinder::getInstance().getLatestBeatGridFor(player);
    if (!waveform || !beatGrid) {
        return;
    }

    std::string signature;
    try {
        signature = computeTrackSignature(metadata->getTitle(),
                                          metadata->getArtist(),
                                          metadata->getDuration(),
                                          *waveform,
                                          *beatGrid);
    } catch (const std::exception& e) {
        std::cerr << "SignatureFinder: Problem computing signature: " << e.what() << std::endl;
        return;
    }

    if (signature.empty()) {
        return;
    }

    {
        std::lock_guard<std::mutex> lock(signaturesMutex_);
        signatures_[player] = signature;
    }
    deliverSignatureUpdate(player, signature);
}

void SignatureFinder::deliverSignatureUpdate(int player, const std::string& signature) {
    auto listeners = getSignatureListeners();
    if (listeners.empty()) {
        return;
    }
    SignatureUpdate update(player, signature);
    for (const auto& listener : listeners) {
        if (!listener) {
            continue;
        }
        try {
            listener->signatureChanged(update);
        } catch (...) {
        }
    }
}

std::shared_ptr<WaveformDetail> SignatureFinder::waveformFromAnalysisTag(const std::shared_ptr<AnalysisTag>& tag) {
    if (!tag) {
        return nullptr;
    }
    const auto payload = tag->getPayload();
    if (payload.size() < 24) {
        return nullptr;
    }
    const auto& typeTag = tag->getTypeTag();
    WaveformStyle style = WaveformStyle::RGB;
    if (typeTag == "PWV5") {
        style = WaveformStyle::RGB;
    } else if (typeTag == "PWV7") {
        style = WaveformStyle::THREE_BAND;
    } else if (typeTag == "PWV3") {
        style = WaveformStyle::BLUE;
    } else {
        return nullptr;
    }

    const size_t bodyOffset = 12;
    const auto lenEntryBytes = static_cast<int>(beatlink::Util::bytesToNumber(payload.data(), bodyOffset, 4));
    const auto lenEntries = static_cast<int>(beatlink::Util::bytesToNumber(payload.data(), bodyOffset + 4, 4));
    if (lenEntryBytes <= 0 || lenEntries <= 0) {
        return nullptr;
    }
    const size_t entriesOffset = bodyOffset + 12;
    const size_t entriesSize = static_cast<size_t>(lenEntryBytes) * static_cast<size_t>(lenEntries);
    if (entriesOffset + entriesSize > payload.size()) {
        return nullptr;
    }

    std::vector<uint8_t> entries(payload.begin() + static_cast<std::ptrdiff_t>(entriesOffset),
                                 payload.begin() + static_cast<std::ptrdiff_t>(entriesOffset + entriesSize));
    return std::make_shared<WaveformDetail>(tag->getDataReference(), std::move(entries), style);
}

} // namespace beatlink::data
