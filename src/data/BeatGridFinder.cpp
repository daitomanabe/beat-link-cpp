#include "beatlink/data/BeatGridFinder.hpp"

#include "beatlink/DeviceFinder.hpp"
#include "beatlink/data/WaveformFinder.hpp"
#include "beatlink/dbserver/Message.hpp"
#include "beatlink/dbserver/NumberField.hpp"

#include <algorithm>
#include <chrono>
#include <iostream>
#include <optional>

namespace beatlink::data {

namespace {
constexpr size_t kMaxPendingUpdates = 100;
}

BeatGridFinder& BeatGridFinder::getInstance() {
    static BeatGridFinder instance;
    return instance;
}

BeatGridFinder::BeatGridFinder() {
    metadataListener_ = std::make_shared<TrackMetadataCallbacks>(
        [this](const TrackMetadataUpdate& update) {
            enqueueUpdate(update);
        });

    mountListener_ = std::make_shared<MountCallbacks>(
        [](const SlotReference&) {},
        [this](const SlotReference& slot) {
            std::vector<DeckReference> toRemove;
            {
                std::lock_guard<std::mutex> lock(hotCacheMutex_);
                for (const auto& [deck, beatGrid] : hotCache_) {
                    if (beatGrid && SlotReference::getSlotReference(beatGrid->dataReference) == slot) {
                        toRemove.push_back(deck);
                    }
                }
                for (const auto& deck : toRemove) {
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
            clearBeatGrids(announcement);
        });

    lifecycleListener_ = std::make_shared<beatlink::LifecycleCallbacks>(
        [](beatlink::LifecycleParticipant&) {},
        [this](beatlink::LifecycleParticipant&) {
            if (isRunning()) {
                stop();
            }
        });
}

std::unordered_map<DeckReference, std::shared_ptr<BeatGrid>, DeckReferenceHash>
BeatGridFinder::getLoadedBeatGrids() {
    ensureRunning();
    std::lock_guard<std::mutex> lock(hotCacheMutex_);
    return hotCache_;
}

std::shared_ptr<BeatGrid> BeatGridFinder::getLatestBeatGridFor(int player) {
    ensureRunning();
    std::lock_guard<std::mutex> lock(hotCacheMutex_);
    auto it = hotCache_.find(DeckReference(player, 0));
    if (it == hotCache_.end()) {
        return nullptr;
    }
    return it->second;
}

std::shared_ptr<BeatGrid> BeatGridFinder::getLatestBeatGridFor(const beatlink::DeviceUpdate& update) {
    auto result = getLatestBeatGridFor(update.getDeviceNumber());
    auto cdj = dynamic_cast<const beatlink::CdjStatus*>(&update);
    if (result && cdj && result->dataReference.getRekordboxId() != cdj->getRekordboxId()) {
        return nullptr;
    }
    return result;
}

std::shared_ptr<BeatGrid> BeatGridFinder::requestBeatGridFrom(const DataReference& track) {
    {
        std::lock_guard<std::mutex> lock(hotCacheMutex_);
        for (const auto& [deck, beatGrid] : hotCache_) {
            if (beatGrid && beatGrid->dataReference == track) {
                return beatGrid;
            }
        }
    }
    return requestBeatGridInternal(track, nullptr);
}

std::shared_ptr<BeatGrid> BeatGridFinder::requestBeatGridInternal(const DataReference& trackReference,
                                                                  const TrackMetadataUpdate* fromUpdate) {
    auto sourceDetails = MetadataFinder::getInstance().getMediaDetailsFor(trackReference.getSlotReference());
    if (sourceDetails) {
        auto provided = MetadataFinder::getInstance().getCompositeProvider().getBeatGrid(*sourceDetails, trackReference);
        if (provided) {
            return provided;
        }
    }

    if (MetadataFinder::getInstance().isPassive() && fromUpdate &&
        trackReference.getSlot() != beatlink::TrackSourceSlot::COLLECTION) {
        return nullptr;
    }

    try {
        return beatlink::dbserver::ConnectionManager::getInstance().invokeWithClientSession(
            trackReference.getPlayer(),
            [this, &trackReference, fromUpdate](beatlink::dbserver::Client& client) {
                return getBeatGrid(trackReference.getRekordboxId(),
                                   trackReference.getSlotReference(),
                                   trackReference.getTrackType(),
                                   fromUpdate,
                                   client);
            },
            "requesting beat grid");
    } catch (const std::exception& e) {
        std::cerr << "BeatGridFinder: Problem requesting beat grid: " << e.what() << std::endl;
    }
    return nullptr;
}

std::shared_ptr<BeatGrid> BeatGridFinder::getBeatGrid(int rekordboxId,
                                                      const SlotReference& slot,
                                                      beatlink::TrackType trackType,
                                                      const TrackMetadataUpdate* fromUpdate,
                                                      beatlink::dbserver::Client& client) {
    auto response = client.simpleRequest(
        beatlink::dbserver::Message::KnownType::BEAT_GRID_REQ,
        std::nullopt,
        {std::make_shared<beatlink::dbserver::NumberField>(
             client.buildRMST(beatlink::dbserver::Message::MenuIdentifier::DATA, slot.getSlot(), trackType)),
         std::make_shared<beatlink::dbserver::NumberField>(rekordboxId)});

    if (response.knownType && *response.knownType == beatlink::dbserver::Message::KnownType::BEAT_GRID) {
        return std::make_shared<BeatGrid>(DataReference(slot, rekordboxId, trackType), response);
    }

    WaveformFinder::retryUnanalyzedTrack(fromUpdate, retrying_, metadataListener_, "beat grid");
    std::cerr << "BeatGridFinder: Unexpected response type when requesting beat grid" << std::endl;
    return nullptr;
}

void BeatGridFinder::clearDeck(const TrackMetadataUpdate& update) {
    bool removed = false;
    {
        std::lock_guard<std::mutex> lock(hotCacheMutex_);
        removed = (hotCache_.erase(DeckReference(update.player, 0)) > 0);
    }
    if (removed) {
        deliverBeatGridUpdate(update.player, nullptr);
    }
}

void BeatGridFinder::clearBeatGrids(const beatlink::DeviceAnnouncement& announcement) {
    const int player = announcement.getDeviceNumber();
    std::vector<DeckReference> toRemove;
    {
        std::lock_guard<std::mutex> lock(hotCacheMutex_);
        for (const auto& [deck, beatGrid] : hotCache_) {
            if (deck.getPlayer() == player) {
                toRemove.push_back(deck);
            }
        }
        for (const auto& deck : toRemove) {
            hotCache_.erase(deck);
        }
    }

    if (std::any_of(toRemove.begin(), toRemove.end(), [](const DeckReference& deck) { return deck.getHotCue() == 0; })) {
        deliverBeatGridUpdate(player, nullptr);
    }
}

void BeatGridFinder::updateBeatGrid(const TrackMetadataUpdate& update, const std::shared_ptr<BeatGrid>& beatGrid) {
    {
        std::lock_guard<std::mutex> lock(hotCacheMutex_);
        hotCache_[DeckReference(update.player, 0)] = beatGrid;
        if (update.metadata && update.metadata->getCueList()) {
            for (const auto& entry : update.metadata->getCueList()->getEntries()) {
                if (entry.hotCueNumber != 0) {
                    hotCache_[DeckReference(update.player, entry.hotCueNumber)] = beatGrid;
                }
            }
        }
    }
    deliverBeatGridUpdate(update.player, beatGrid);
}

void BeatGridFinder::deliverBeatGridUpdate(int player, const std::shared_ptr<BeatGrid>& beatGrid) {
    BeatGridUpdate update(player, beatGrid);
    auto listeners = getBeatGridListeners();
    for (const auto& listener : listeners) {
        if (!listener) {
            continue;
        }
        try {
            listener->beatGridChanged(update);
        } catch (...) {
        }
    }
}

void BeatGridFinder::handleUpdate(const TrackMetadataUpdate& update) {
    if (!update.metadata) {
        clearDeck(update);
        return;
    }

    std::shared_ptr<BeatGrid> lastBeatGrid;
    {
        std::lock_guard<std::mutex> lock(hotCacheMutex_);
        auto it = hotCache_.find(DeckReference(update.player, 0));
        if (it != hotCache_.end()) {
            lastBeatGrid = it->second;
        }
    }

    if (!lastBeatGrid || lastBeatGrid->dataReference != update.metadata->getTrackReference()) {
        {
            std::lock_guard<std::mutex> lock(hotCacheMutex_);
            for (const auto& [deck, beatGrid] : hotCache_) {
                if (beatGrid && beatGrid->dataReference == update.metadata->getTrackReference()) {
                    updateBeatGrid(update, beatGrid);
                    return;
                }
            }
        }

        bool shouldRequest = false;
        {
            std::lock_guard<std::mutex> lock(activeRequestsMutex_);
            shouldRequest = activeRequests_.insert(update.player).second;
        }

        if (shouldRequest) {
            clearDeck(update);
            std::thread([this, update]() {
                try {
                    auto grid = requestBeatGridInternal(update.metadata->getTrackReference(), &update);
                    if (grid && grid->getBeatCount() > 0) {
                        updateBeatGrid(update, grid);
                    } else {
                        WaveformFinder::retryUnanalyzedTrack(&update, retrying_, metadataListener_, "beat grid");
                    }
                } catch (const std::exception& e) {
                    std::cerr << "BeatGridFinder: Problem requesting beat grid: " << e.what() << std::endl;
                    WaveformFinder::retryUnanalyzedTrack(&update, retrying_, metadataListener_, "beat grid");
                }
                std::lock_guard<std::mutex> lock(activeRequestsMutex_);
                activeRequests_.erase(update.player);
            }).detach();
        }
    }
}

void BeatGridFinder::enqueueUpdate(const TrackMetadataUpdate& update) {
    std::unique_lock<std::mutex> lock(pendingMutex_);
    if (pendingUpdates_.size() >= kMaxPendingUpdates) {
        std::cerr << "BeatGridFinder: Discarding metadata update because queue is backed up" << std::endl;
        return;
    }
    pendingUpdates_.push_back(update);
    lock.unlock();
    pendingCv_.notify_one();
}

void BeatGridFinder::addBeatGridListener(const BeatGridListenerPtr& listener) {
    if (!listener) {
        return;
    }
    std::lock_guard<std::mutex> lock(listenersMutex_);
    if (std::find(beatGridListeners_.begin(), beatGridListeners_.end(), listener) == beatGridListeners_.end()) {
        beatGridListeners_.push_back(listener);
    }
}

void BeatGridFinder::removeBeatGridListener(const BeatGridListenerPtr& listener) {
    if (!listener) {
        return;
    }
    std::lock_guard<std::mutex> lock(listenersMutex_);
    beatGridListeners_.erase(std::remove(beatGridListeners_.begin(), beatGridListeners_.end(), listener),
                             beatGridListeners_.end());
}

std::vector<BeatGridListenerPtr> BeatGridFinder::getBeatGridListeners() const {
    std::lock_guard<std::mutex> lock(listenersMutex_);
    return beatGridListeners_;
}

void BeatGridFinder::start() {
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
                std::cerr << "BeatGridFinder: Problem processing metadata update: " << e.what() << std::endl;
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

void BeatGridFinder::stop() {
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
        for (const auto& [deck, beatGrid] : hotCache_) {
            if (deck.getHotCue() == 0) {
                decks.push_back(deck);
            }
        }
        hotCache_.clear();
    }

    for (const auto& deck : decks) {
        deliverBeatGridUpdate(deck.getPlayer(), nullptr);
    }

    deliverLifecycleAnnouncement(false);
}

} // namespace beatlink::data
