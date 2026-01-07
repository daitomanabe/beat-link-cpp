#include "beatlink/data/ArtFinder.hpp"

#include "beatlink/DeviceFinder.hpp"
#include "beatlink/dbserver/BinaryField.hpp"
#include "beatlink/dbserver/Message.hpp"
#include "beatlink/dbserver/NumberField.hpp"

#include <algorithm>
#include <chrono>
#include <iostream>
#include <optional>
#include <stdexcept>

namespace beatlink::data {

namespace {
constexpr size_t kMaxPendingUpdates = 100;
}

ArtFinder& ArtFinder::getInstance() {
    static ArtFinder instance;
    return instance;
}

ArtFinder::ArtFinder() {
    metadataListener_ = std::make_shared<TrackMetadataCallbacks>(
        [this](const TrackMetadataUpdate& update) {
            enqueueUpdate(update);
        });

    mountListener_ = std::make_shared<MountCallbacks>(
        [](const SlotReference&) {},
        [this](const SlotReference& slot) {
            std::vector<DataReference> artKeys;
            {
                std::lock_guard<std::mutex> lock(artCacheMutex_);
                for (const auto& [reference, art] : artCache_) {
                    if (SlotReference::getSlotReference(reference) == slot) {
                        artKeys.push_back(reference);
                    }
                }
            }
            for (const auto& key : artKeys) {
                removeArtFromCache(key);
            }

            std::vector<DeckReference> deckKeys;
            {
                std::lock_guard<std::mutex> lock(hotCacheMutex_);
                for (const auto& [deck, art] : hotCache_) {
                    if (art && SlotReference::getSlotReference(art->artReference) == slot) {
                        deckKeys.push_back(deck);
                    }
                }
                for (const auto& deck : deckKeys) {
                    hotCache_.erase(deck);
                }
            }
        });

    announcementListener_ = std::make_shared<beatlink::DeviceAnnouncementCallbacks>(
        [](const beatlink::DeviceAnnouncement&) {},
        [this](const beatlink::DeviceAnnouncement& announcement) {
            if (announcement.getDeviceNumber() == 25 && announcement.getDeviceName() == "NXS-GW") {
                return;
            }
            clearArt(announcement);
        });

    lifecycleListener_ = std::make_shared<beatlink::LifecycleCallbacks>(
        [](beatlink::LifecycleParticipant&) {},
        [this](beatlink::LifecycleParticipant&) {
            if (isRunning()) {
                stop();
            }
        });
}

void ArtFinder::setArtCacheSize(int size) {
    if (size < 1) {
        throw std::invalid_argument("size must be at least 1");
    }
    artCacheSize_.store(size);
    std::lock_guard<std::mutex> lock(artCacheMutex_);
    while (static_cast<int>(artCache_.size()) > size) {
        evictFromArtCache();
    }
}

std::unordered_map<DeckReference, std::shared_ptr<AlbumArt>, DeckReferenceHash> ArtFinder::getLoadedArt() {
    ensureRunning();
    std::lock_guard<std::mutex> lock(hotCacheMutex_);
    return hotCache_;
}

std::shared_ptr<AlbumArt> ArtFinder::getLatestArtFor(int player) {
    ensureRunning();
    std::lock_guard<std::mutex> lock(hotCacheMutex_);
    auto it = hotCache_.find(DeckReference(player, 0));
    if (it == hotCache_.end()) {
        return nullptr;
    }
    return it->second;
}

std::shared_ptr<AlbumArt> ArtFinder::getLatestArtFor(const beatlink::DeviceUpdate& update) {
    return getLatestArtFor(update.getDeviceNumber());
}

std::shared_ptr<AlbumArt> ArtFinder::requestArtworkFrom(const DataReference& artReference) {
    ensureRunning();
    auto artwork = findArtInMemoryCaches(artReference);
    if (!artwork) {
        artwork = requestArtworkInternal(artReference, false);
    }
    return artwork;
}

std::shared_ptr<AlbumArt> ArtFinder::requestArtworkFrom(const DataReference& artReference, beatlink::TrackType /*trackType*/) {
    return requestArtworkFrom(artReference);
}

std::shared_ptr<AlbumArt> ArtFinder::requestArtworkInternal(const DataReference& artReference, bool failIfPassive) {
    auto sourceDetails = MetadataFinder::getInstance().getMediaDetailsFor(artReference.getSlotReference());
    if (sourceDetails) {
        auto provided = MetadataFinder::getInstance().getCompositeProvider().getAlbumArt(*sourceDetails, artReference);
        if (provided) {
            addArtToCache(artReference, provided);
            return provided;
        }
    }

    if (MetadataFinder::getInstance().isPassive() && failIfPassive &&
        artReference.getSlot() != beatlink::TrackSourceSlot::COLLECTION) {
        return nullptr;
    }

    try {
        auto artwork = beatlink::dbserver::ConnectionManager::getInstance().invokeWithClientSession(
            artReference.getPlayer(),
            [this, &artReference](beatlink::dbserver::Client& client) {
                return getArtwork(artReference.getRekordboxId(), artReference.getSlotReference(), artReference.getTrackType(), client);
            },
            "requesting artwork");
        if (artwork) {
            addArtToCache(artReference, artwork);
        }
        return artwork;
    } catch (const std::exception& e) {
        std::cerr << "ArtFinder: Problem requesting artwork: " << e.what() << std::endl;
    }
    return nullptr;
}

std::shared_ptr<AlbumArt> ArtFinder::getArtwork(int artworkId,
                                                const SlotReference& slot,
                                                beatlink::TrackType trackType,
                                                beatlink::dbserver::Client& client) {
    beatlink::dbserver::Message response = requestHighResolutionArt_.load()
        ? client.simpleRequest(
            beatlink::dbserver::Message::KnownType::ALBUM_ART_REQ,
            beatlink::dbserver::Message::KnownType::ALBUM_ART,
            {std::make_shared<beatlink::dbserver::NumberField>(
                 client.buildRMST(beatlink::dbserver::Message::MenuIdentifier::DATA, slot.getSlot(), trackType)),
             std::make_shared<beatlink::dbserver::NumberField>(artworkId),
             std::make_shared<beatlink::dbserver::NumberField>(1)})
        : client.simpleRequest(
            beatlink::dbserver::Message::KnownType::ALBUM_ART_REQ,
            beatlink::dbserver::Message::KnownType::ALBUM_ART,
            {std::make_shared<beatlink::dbserver::NumberField>(
                 client.buildRMST(beatlink::dbserver::Message::MenuIdentifier::DATA, slot.getSlot(), trackType)),
             std::make_shared<beatlink::dbserver::NumberField>(artworkId)});

    if (response.arguments.size() <= 3) {
        return nullptr;
    }

    auto binary = std::dynamic_pointer_cast<beatlink::dbserver::BinaryField>(response.arguments.at(3));
    if (!binary) {
        return nullptr;
    }

    return std::make_shared<AlbumArt>(DataReference(slot, artworkId, trackType),
                                      binary->getValueAsArray());
}

std::shared_ptr<AlbumArt> ArtFinder::findArtInMemoryCaches(const DataReference& artReference) {
    {
        std::lock_guard<std::mutex> lock(hotCacheMutex_);
        for (const auto& [deck, art] : hotCache_) {
            if (art && art->artReference == artReference) {
                return art;
            }
        }
    }

    std::lock_guard<std::mutex> lock(artCacheMutex_);
    auto it = artCache_.find(artReference);
    if (it != artCache_.end()) {
        artCacheRecentlyUsed_.insert(artReference);
        return it->second;
    }
    return nullptr;
}

void ArtFinder::clearDeck(const TrackMetadataUpdate& update) {
    bool removed = false;
    {
        std::lock_guard<std::mutex> lock(hotCacheMutex_);
        removed = (hotCache_.erase(DeckReference(update.player, 0)) > 0);
    }
    if (removed) {
        deliverAlbumArtUpdate(update.player, nullptr);
    }
}

void ArtFinder::clearArt(const beatlink::DeviceAnnouncement& announcement) {
    const int player = announcement.getDeviceNumber();
    std::vector<DeckReference> deckKeys;
    {
        std::lock_guard<std::mutex> lock(hotCacheMutex_);
        for (const auto& [deck, art] : hotCache_) {
            if (deck.getPlayer() == player) {
                deckKeys.push_back(deck);
            }
        }
        for (const auto& deck : deckKeys) {
            hotCache_.erase(deck);
        }
    }

    if (std::any_of(deckKeys.begin(), deckKeys.end(), [](const DeckReference& deck) { return deck.getHotCue() == 0; })) {
        deliverAlbumArtUpdate(player, nullptr);
    }

    std::vector<DataReference> artKeys;
    {
        std::lock_guard<std::mutex> lock(artCacheMutex_);
        for (const auto& [reference, art] : artCache_) {
            if (reference.getPlayer() == player) {
                artKeys.push_back(reference);
            }
        }
    }
    for (const auto& key : artKeys) {
        removeArtFromCache(key);
    }
}

void ArtFinder::updateArt(const TrackMetadataUpdate& update, const std::shared_ptr<AlbumArt>& art) {
    {
        std::lock_guard<std::mutex> lock(hotCacheMutex_);
        hotCache_[DeckReference(update.player, 0)] = art;
        if (update.metadata && update.metadata->getCueList()) {
            for (const auto& entry : update.metadata->getCueList()->getEntries()) {
                if (entry.hotCueNumber != 0) {
                    hotCache_[DeckReference(update.player, entry.hotCueNumber)] = art;
                }
            }
        }
    }
    deliverAlbumArtUpdate(update.player, art);
}

void ArtFinder::deliverAlbumArtUpdate(int player, const std::shared_ptr<AlbumArt>& art) {
    AlbumArtUpdate update(player, art);
    auto listeners = getAlbumArtListeners();
    for (const auto& listener : listeners) {
        if (!listener) {
            continue;
        }
        try {
            listener->albumArtChanged(update);
        } catch (...) {
        }
    }
}

void ArtFinder::handleUpdate(const TrackMetadataUpdate& update) {
    if (!update.metadata || update.metadata->getArtworkId() == 0) {
        clearDeck(update);
        return;
    }

    std::shared_ptr<AlbumArt> lastArt;
    {
        std::lock_guard<std::mutex> lock(hotCacheMutex_);
        auto it = hotCache_.find(DeckReference(update.player, 0));
        if (it != hotCache_.end()) {
            lastArt = it->second;
        }
    }

    const auto trackReference = update.metadata->getTrackReference();
    const DataReference artReference(trackReference.getPlayer(),
                                     trackReference.getSlot(),
                                     update.metadata->getArtworkId(),
                                     update.metadata->getTrackType());

    if (!lastArt || lastArt->artReference != artReference) {
        auto cached = findArtInMemoryCaches(artReference);
        if (cached) {
            updateArt(update, cached);
            return;
        }

        bool shouldRequest = false;
        {
            std::lock_guard<std::mutex> lock(activeRequestsMutex_);
            shouldRequest = activeRequests_.insert(update.player).second;
        }

        if (shouldRequest) {
            clearDeck(update);
            std::thread([this, update, artReference]() {
                try {
                    auto art = requestArtworkInternal(artReference, true);
                    if (art) {
                        updateArt(update, art);
                    }
                } catch (const std::exception& e) {
                    std::cerr << "ArtFinder: Problem requesting album art: " << e.what() << std::endl;
                }
                std::lock_guard<std::mutex> lock(activeRequestsMutex_);
                activeRequests_.erase(update.player);
            }).detach();
        }
    }
}

void ArtFinder::enqueueUpdate(const TrackMetadataUpdate& update) {
    std::unique_lock<std::mutex> lock(pendingMutex_);
    if (pendingUpdates_.size() >= kMaxPendingUpdates) {
        std::cerr << "ArtFinder: Discarding metadata update because queue is backed up" << std::endl;
        return;
    }
    pendingUpdates_.push_back(update);
    lock.unlock();
    pendingCv_.notify_one();
}

void ArtFinder::removeArtFromCache(const DataReference& artReference) {
    std::lock_guard<std::mutex> lock(artCacheMutex_);
    artCache_.erase(artReference);
    artCacheRecentlyUsed_.erase(artReference);
    auto it = std::find(artCacheEvictionQueue_.begin(), artCacheEvictionQueue_.end(), artReference);
    if (it != artCacheEvictionQueue_.end()) {
        artCacheEvictionQueue_.erase(it);
    }
}

void ArtFinder::addArtToCache(const DataReference& artReference, const std::shared_ptr<AlbumArt>& art) {
    std::lock_guard<std::mutex> lock(artCacheMutex_);
    while (static_cast<int>(artCache_.size()) >= artCacheSize_.load()) {
        evictFromArtCache();
    }
    artCache_[artReference] = art;
    artCacheEvictionQueue_.push_back(artReference);
}

void ArtFinder::evictFromArtCache() {
    bool evicted = false;
    while (!evicted && !artCacheEvictionQueue_.empty()) {
        DataReference candidate = artCacheEvictionQueue_.front();
        artCacheEvictionQueue_.pop_front();
        if (artCacheRecentlyUsed_.erase(candidate) > 0) {
            artCacheEvictionQueue_.push_back(candidate);
        } else {
            artCache_.erase(candidate);
            evicted = true;
        }
    }
}

void ArtFinder::addAlbumArtListener(const AlbumArtListenerPtr& listener) {
    if (!listener) {
        return;
    }
    std::lock_guard<std::mutex> lock(listenersMutex_);
    if (std::find(artListeners_.begin(), artListeners_.end(), listener) == artListeners_.end()) {
        artListeners_.push_back(listener);
    }
}

void ArtFinder::removeAlbumArtListener(const AlbumArtListenerPtr& listener) {
    if (!listener) {
        return;
    }
    std::lock_guard<std::mutex> lock(listenersMutex_);
    artListeners_.erase(std::remove(artListeners_.begin(), artListeners_.end(), listener),
                        artListeners_.end());
}

std::vector<AlbumArtListenerPtr> ArtFinder::getAlbumArtListeners() const {
    std::lock_guard<std::mutex> lock(listenersMutex_);
    return artListeners_;
}

void ArtFinder::start() {
    if (isRunning()) {
        return;
    }

    beatlink::dbserver::ConnectionManager::getInstance().addLifecycleListener(lifecycleListener_);
    beatlink::dbserver::ConnectionManager::getInstance().start();

    beatlink::DeviceFinder::getInstance().addDeviceAnnouncementListener(announcementListener_);

    MetadataFinder::getInstance().addLifecycleListener(lifecycleListener_);
    MetadataFinder::getInstance().start();
    MetadataFinder::getInstance().addTrackMetadataListener(metadataListener_);
    MetadataFinder::getInstance().addMountListener(mountListener_);

    running_.store(true);
    queueHandler_ = std::thread([this]() {
        while (isRunning()) {
            std::optional<TrackMetadataUpdate> update;
            {
                std::unique_lock<std::mutex> lock(pendingMutex_);
                pendingCv_.wait(lock, [this]() { return !isRunning() || !pendingUpdates_.empty(); });
                if (!isRunning()) {
                    break;
                }
                update = pendingUpdates_.front();
                pendingUpdates_.pop_front();
            }

            if (!update) {
                continue;
            }

            try {
                handleUpdate(*update);
            } catch (const std::exception& e) {
                std::cerr << "ArtFinder: Problem processing metadata update: " << e.what() << std::endl;
            }
        }
    });

    deliverLifecycleAnnouncement(true);

    auto loadedTracks = MetadataFinder::getInstance().getLoadedTracks();
    for (const auto& [deck, metadata] : loadedTracks) {
        if (deck.getHotCue() == 0) {
            handleUpdate(TrackMetadataUpdate(deck.getPlayer(), metadata));
        }
    }
}

void ArtFinder::stop() {
    if (!isRunning()) {
        return;
    }

    MetadataFinder::getInstance().removeTrackMetadataListener(metadataListener_);
    MetadataFinder::getInstance().removeMountListener(mountListener_);
    beatlink::DeviceFinder::getInstance().removeDeviceAnnouncementListener(announcementListener_);
    beatlink::dbserver::ConnectionManager::getInstance().removeLifecycleListener(lifecycleListener_);
    MetadataFinder::getInstance().removeLifecycleListener(lifecycleListener_);

    running_.store(false);
    {
        std::lock_guard<std::mutex> lock(pendingMutex_);
        pendingUpdates_.clear();
    }
    pendingCv_.notify_all();
    if (queueHandler_.joinable()) {
        queueHandler_.join();
    }

    std::vector<DeckReference> decks;
    {
        std::lock_guard<std::mutex> lock(hotCacheMutex_);
        for (const auto& [deck, art] : hotCache_) {
            if (deck.getHotCue() == 0) {
                decks.push_back(deck);
            }
        }
        hotCache_.clear();
    }

    for (const auto& deck : decks) {
        deliverAlbumArtUpdate(deck.getPlayer(), nullptr);
    }

    deliverLifecycleAnnouncement(false);
}

} // namespace beatlink::data
