#include "beatlink/data/AnalysisTagFinder.hpp"

#include "beatlink/DeviceFinder.hpp"
#include "beatlink/dbserver/BinaryField.hpp"
#include "beatlink/dbserver/Message.hpp"

#include <algorithm>
#include <chrono>
#include <iostream>
#include <optional>

namespace beatlink::data {

namespace {
constexpr size_t kMaxPendingUpdates = 100;
constexpr size_t kAnalysisTagLengthOffset = 4;
} // namespace

AnalysisTagFinder& AnalysisTagFinder::getInstance() {
    static AnalysisTagFinder instance;
    return instance;
}

AnalysisTagFinder::AnalysisTagFinder() {
    metadataListener_ = std::make_shared<TrackMetadataCallbacks>(
        [this](const TrackMetadataUpdate& update) { enqueueUpdate(update); });

    mountListener_ = std::make_shared<MountCallbacks>(
        [](const SlotReference&) {},
        [this](const SlotReference& slot) {
            std::vector<std::pair<DeckReference, std::string>> toRemove;
            std::unordered_map<DeckReference, std::unordered_map<std::string, CacheEntry>, DeckReferenceHash> cacheCopy;
            {
                std::lock_guard<std::mutex> lock(hotCacheMutex_);
                cacheCopy = hotCache_;
            }

            for (const auto& [deck, tagMap] : cacheCopy) {
                for (const auto& [tagKey, entry] : tagMap) {
                    if (SlotReference::getSlotReference(entry.dataReference) == slot) {
                        toRemove.emplace_back(deck, tagKey);
                    }
                }
            }

            if (toRemove.empty()) {
                return;
            }

            std::unordered_map<int, std::vector<CacheEntry>> removedByPlayer;
            {
                std::lock_guard<std::mutex> lock(hotCacheMutex_);
                for (const auto& [deck, tagKey] : toRemove) {
                    auto deckIt = hotCache_.find(deck);
                    if (deckIt == hotCache_.end()) {
                        continue;
                    }
                    auto tagIt = deckIt->second.find(tagKey);
                    if (tagIt == deckIt->second.end()) {
                        continue;
                    }
                    removedByPlayer[deck.getPlayer()].push_back(tagIt->second);
                    deckIt->second.erase(tagIt);
                }
            }

            for (const auto& [player, removed] : removedByPlayer) {
                for (const auto& entry : removed) {
                    deliverAnalysisTagUpdate(player, entry.fileExtension, entry.typeTag, nullptr);
                }
            }
        });

    announcementListener_ = std::make_shared<beatlink::DeviceAnnouncementCallbacks>(
        [](const beatlink::DeviceAnnouncement&) {},
        [this](const beatlink::DeviceAnnouncement& announcement) {
            if (announcement.getDeviceNumber() == 25 && announcement.getDeviceName() == "NXS-GW") {
                return;
            }
            clearTags(announcement);
        });

    lifecycleListener_ = std::make_shared<beatlink::LifecycleCallbacks>(
        [](beatlink::LifecycleParticipant&) {},
        [this](beatlink::LifecycleParticipant&) {
            if (isRunning()) {
                stop();
            }
        });
}

std::unordered_map<DeckReference, std::unordered_map<std::string, AnalysisTagFinder::CacheEntry>, DeckReferenceHash>
AnalysisTagFinder::getLoadedAnalysisTags() {
    ensureRunning();
    std::lock_guard<std::mutex> lock(hotCacheMutex_);
    return hotCache_;
}

std::shared_ptr<AnalysisTag> AnalysisTagFinder::getLatestTrackAnalysisFor(
    int player,
    const std::string& fileExtension,
    const std::string& typeTag) {
    ensureRunning();
    std::lock_guard<std::mutex> lock(hotCacheMutex_);
    auto deckIt = hotCache_.find(DeckReference(player, 0));
    if (deckIt == hotCache_.end()) {
        return nullptr;
    }
    const std::string tagKey = typeTag + fileExtension;
    auto tagIt = deckIt->second.find(tagKey);
    if (tagIt == deckIt->second.end()) {
        return nullptr;
    }
    return tagIt->second.analysisTag;
}

std::shared_ptr<AnalysisTag> AnalysisTagFinder::getLatestTrackAnalysisFor(
    const beatlink::DeviceUpdate& update,
    const std::string& fileExtension,
    const std::string& typeTag) {
    return getLatestTrackAnalysisFor(update.getDeviceNumber(), fileExtension, typeTag);
}

std::shared_ptr<AnalysisTag> AnalysisTagFinder::requestAnalysisTagFrom(
    const DataReference& trackReference,
    const std::string& fileExtension,
    const std::string& typeTag) {
    ensureRunning();
    const std::string tagKey = typeTag + fileExtension;
    {
        std::lock_guard<std::mutex> lock(hotCacheMutex_);
        for (const auto& [deck, tagMap] : hotCache_) {
            auto it = tagMap.find(tagKey);
            if (it != tagMap.end() && it->second.dataReference == trackReference) {
                return it->second.analysisTag;
            }
        }
    }
    return requestAnalysisTagInternal(trackReference, fileExtension, typeTag, false);
}

beatlink::dbserver::NumberField AnalysisTagFinder::stringToProtocolNumber(const std::string& value) {
    uint32_t fourcc = 0;
    for (int i = 3; i >= 0; --i) {
        fourcc *= 256;
        if (i < static_cast<int>(value.size())) {
            fourcc += static_cast<uint8_t>(value[i]);
        }
    }
    return beatlink::dbserver::NumberField(fourcc);
}

void AnalysisTagFinder::addAnalysisTagListener(const AnalysisTagListenerPtr& listener,
                                               const std::string& fileExtension,
                                               const std::string& typeTag) {
    if (!listener) {
        return;
    }
    const std::string tagKey = typeTag + fileExtension;
    bool trackingNewTag = false;
    {
        std::lock_guard<std::mutex> lock(listenersMutex_);
        auto& listeners = analysisTagListeners_[tagKey];
        trackingNewTag = listeners.empty();
        listeners.push_back(listener);
    }
    if (trackingNewTag) {
        primeCache();
    }
}

void AnalysisTagFinder::removeAnalysisTagListener(const AnalysisTagListenerPtr& listener,
                                                  const std::string& fileExtension,
                                                  const std::string& typeTag) {
    if (!listener) {
        return;
    }
    const std::string tagKey = typeTag + fileExtension;
    std::lock_guard<std::mutex> lock(listenersMutex_);
    auto it = analysisTagListeners_.find(tagKey);
    if (it == analysisTagListeners_.end()) {
        return;
    }
    auto& listeners = it->second;
    listeners.erase(std::remove(listeners.begin(), listeners.end(), listener), listeners.end());
    if (listeners.empty()) {
        analysisTagListeners_.erase(it);
    }
}

std::unordered_map<std::string, std::vector<AnalysisTagListenerPtr>> AnalysisTagFinder::getTagListeners() const {
    std::lock_guard<std::mutex> lock(listenersMutex_);
    return analysisTagListeners_;
}

void AnalysisTagFinder::start() {
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
            std::unique_lock<std::mutex> lock(pendingMutex_);
            pendingCv_.wait(lock, [this]() { return !pendingUpdates_.empty() || !isRunning(); });
            if (!isRunning()) {
                break;
            }
            TrackMetadataUpdate update = std::move(pendingUpdates_.front());
            pendingUpdates_.pop_front();
            lock.unlock();

            try {
                handleUpdate(update);
            } catch (const std::exception& e) {
                std::cerr << "AnalysisTagFinder: Problem processing metadata update: " << e.what() << std::endl;
            }
        }
    });

    deliverLifecycleAnnouncement(true);
    primeCache();
}

void AnalysisTagFinder::stop() {
    if (!isRunning()) {
        return;
    }

    MetadataFinder::getInstance().removeTrackMetadataListener(metadataListener_);
    running_.store(false);
    pendingCv_.notify_all();
    if (queueHandler_.joinable()) {
        queueHandler_.join();
    }
    {
        std::lock_guard<std::mutex> lock(pendingMutex_);
        pendingUpdates_.clear();
    }

    std::unordered_map<DeckReference, std::unordered_map<std::string, CacheEntry>, DeckReferenceHash> dyingCache;
    {
        std::lock_guard<std::mutex> lock(hotCacheMutex_);
        dyingCache = std::move(hotCache_);
        hotCache_.clear();
    }

    for (const auto& [deck, tags] : dyingCache) {
        if (deck.getHotCue() == 0) {
            deliverTagLossUpdate(deck.getPlayer(), tags);
        }
    }

    deliverLifecycleAnnouncement(false);
}

std::shared_ptr<AnalysisTag> AnalysisTagFinder::requestAnalysisTagInternal(
    const DataReference& trackReference,
    const std::string& fileExtension,
    const std::string& typeTag,
    bool failIfPassive) {
    auto sourceDetails = MetadataFinder::getInstance().getMediaDetailsFor(trackReference.getSlotReference());
    if (sourceDetails) {
        auto provided = MetadataFinder::getInstance().getCompositeProvider().getAnalysisSection(
            *sourceDetails, trackReference, fileExtension, typeTag);
        if (provided) {
            return provided;
        }
    }

    if (MetadataFinder::getInstance().isPassive() && failIfPassive &&
        trackReference.getSlot() != beatlink::TrackSourceSlot::COLLECTION) {
        return nullptr;
    }

    auto announcement = beatlink::DeviceFinder::getInstance().getLatestAnnouncementFrom(trackReference.getPlayer());
    if (announcement && announcement->isOpusQuad()) {
        return nullptr;
    }

    try {
        return beatlink::dbserver::ConnectionManager::getInstance().invokeWithClientSession(
            trackReference.getPlayer(),
            [this, &trackReference, &fileExtension, &typeTag](beatlink::dbserver::Client& client) {
                return getTagViaDbServer(trackReference,
                                         fileExtension,
                                         typeTag,
                                         client);
            },
            "requesting analysis tag");
    } catch (const std::exception& e) {
        std::cerr << "AnalysisTagFinder: Problem requesting analysis tag: " << e.what() << std::endl;
    }
    return nullptr;
}

std::shared_ptr<AnalysisTag> AnalysisTagFinder::getTagViaDbServer(
    const DataReference& trackReference,
    const std::string& fileExtension,
    const std::string& typeTag,
    beatlink::dbserver::Client& client) {
    const std::string extension = (!fileExtension.empty() && fileExtension[0] == '.')
        ? fileExtension.substr(1)
        : fileExtension;
    auto idField = std::make_shared<beatlink::dbserver::NumberField>(trackReference.getRekordboxId());
    auto tagField = std::make_shared<beatlink::dbserver::NumberField>(stringToProtocolNumber(typeTag).getValue());
    auto fileField = std::make_shared<beatlink::dbserver::NumberField>(stringToProtocolNumber(extension).getValue());

    auto response = client.simpleRequest(
        beatlink::dbserver::Message::KnownType::ANLZ_TAG_REQ,
        beatlink::dbserver::Message::KnownType::ANLZ_TAG,
        {std::make_shared<beatlink::dbserver::NumberField>(
             client.buildRMST(beatlink::dbserver::Message::MenuIdentifier::MAIN_MENU,
                              trackReference.getSlotReference().getSlot())),
         idField,
         tagField,
         fileField});

    if (response.knownType && *response.knownType == beatlink::dbserver::Message::KnownType::ANLZ_TAG) {
        if (response.arguments.size() > 3) {
            auto binary = std::dynamic_pointer_cast<beatlink::dbserver::BinaryField>(response.arguments.at(3));
            if (binary) {
                auto bytes = binary->getValueAsArray();
                if (bytes.size() > kAnalysisTagLengthOffset) {
                    std::vector<uint8_t> payload(bytes.begin() + static_cast<std::ptrdiff_t>(kAnalysisTagLengthOffset),
                                                 bytes.end());
                    return std::make_shared<AnalysisTag>(trackReference,
                                                         fileExtension,
                                                         typeTag,
                                                         std::move(payload));
                }
            }
        }
    }

    return nullptr;
}

void AnalysisTagFinder::enqueueUpdate(const TrackMetadataUpdate& update) {
    std::lock_guard<std::mutex> lock(pendingMutex_);
    if (pendingUpdates_.size() >= kMaxPendingUpdates) {
        pendingUpdates_.pop_front();
    }
    pendingUpdates_.push_back(update);
    pendingCv_.notify_one();
}

void AnalysisTagFinder::handleUpdate(const TrackMetadataUpdate& update) {
    if (!update.metadata || update.metadata->getTrackType() != beatlink::TrackType::REKORDBOX) {
        clearDeckTags(update);
        return;
    }

    std::unordered_map<std::string, std::vector<AnalysisTagListenerPtr>> listenersSnapshot = getTagListeners();
    for (const auto& [tagKey, listeners] : listenersSnapshot) {
        if (listeners.empty()) {
            continue;
        }
        const auto dotPos = tagKey.find('.');
        if (dotPos == std::string::npos) {
            continue;
        }
        const std::string typeTag = tagKey.substr(0, dotPos);
        const std::string fileExtension = tagKey.substr(dotPos);

        CacheEntry lastEntry(update.metadata->getTrackReference(), fileExtension, typeTag, nullptr);
        bool hasLast = false;
        {
            std::lock_guard<std::mutex> lock(hotCacheMutex_);
            auto deckIt = hotCache_.find(DeckReference(update.player, 0));
            if (deckIt != hotCache_.end()) {
                auto tagIt = deckIt->second.find(tagKey);
                if (tagIt != deckIt->second.end()) {
                    lastEntry = tagIt->second;
                    hasLast = true;
                }
            }
        }

        if (!hasLast || lastEntry.dataReference != update.metadata->getTrackReference()) {
            bool foundInCache = false;
            {
                std::lock_guard<std::mutex> lock(hotCacheMutex_);
                for (const auto& [deck, tagMap] : hotCache_) {
                    auto tagIt = tagMap.find(tagKey);
                    if (tagIt != tagMap.end() &&
                        tagIt->second.dataReference == update.metadata->getTrackReference()) {
                        updateAnalysisTag(update, fileExtension, typeTag, tagIt->second.analysisTag);
                        foundInCache = true;
                        break;
                    }
                }
            }

            const std::string activeKey = std::to_string(update.player) + ":" + tagKey;
            bool shouldRequest = false;
            {
                std::lock_guard<std::mutex> lock(activeRequestsMutex_);
                if (!foundInCache && activeRequests_.insert(activeKey).second) {
                    shouldRequest = true;
                }
            }

            if (shouldRequest) {
                clearDeckTags(update);
                std::thread([this, update, fileExtension, typeTag, activeKey]() {
                    try {
                        auto tag = requestAnalysisTagInternal(update.metadata->getTrackReference(),
                                                              fileExtension,
                                                              typeTag,
                                                              true);
                        if (tag) {
                            updateAnalysisTag(update, fileExtension, typeTag, tag);
                        }
                    } catch (const std::exception& e) {
                        std::cerr << "AnalysisTagFinder: Problem requesting tag " << typeTag
                                  << " in " << fileExtension << ": " << e.what() << std::endl;
                    }
                    std::lock_guard<std::mutex> lock(activeRequestsMutex_);
                    activeRequests_.erase(activeKey);
                }).detach();
            }
        }
    }
}

void AnalysisTagFinder::clearDeckTags(const TrackMetadataUpdate& update) {
    std::unordered_map<std::string, CacheEntry> removed;
    {
        std::lock_guard<std::mutex> lock(hotCacheMutex_);
        auto it = hotCache_.find(DeckReference(update.player, 0));
        if (it != hotCache_.end()) {
            removed = std::move(it->second);
            hotCache_.erase(it);
        }
    }
    if (!removed.empty()) {
        deliverTagLossUpdate(update.player, removed);
    }
}

void AnalysisTagFinder::clearTags(const beatlink::DeviceAnnouncement& announcement) {
    const int player = announcement.getDeviceNumber();
    std::vector<std::pair<DeckReference, std::unordered_map<std::string, CacheEntry>>> removed;
    {
        std::lock_guard<std::mutex> lock(hotCacheMutex_);
        for (auto it = hotCache_.begin(); it != hotCache_.end(); ) {
            if (it->first.getPlayer() != player) {
                ++it;
                continue;
            }
            removed.emplace_back(it->first, std::move(it->second));
            it = hotCache_.erase(it);
        }
    }

    for (const auto& [deck, tags] : removed) {
        if (deck.getHotCue() == 0) {
            deliverTagLossUpdate(deck.getPlayer(), tags);
        }
    }
}

void AnalysisTagFinder::updateAnalysisTag(const TrackMetadataUpdate& update,
                                          const std::string& fileExtension,
                                          const std::string& typeTag,
                                          const std::shared_ptr<AnalysisTag>& tag) {
    CacheEntry entry(update.metadata->getTrackReference(), fileExtension, typeTag, tag);
    const std::string tagKey = typeTag + fileExtension;
    {
        std::lock_guard<std::mutex> lock(hotCacheMutex_);
        auto& deckCache = hotCache_[DeckReference(update.player, 0)];
        deckCache.insert_or_assign(tagKey, entry);
        if (update.metadata->getCueList()) {
            for (const auto& cue : update.metadata->getCueList()->getEntries()) {
                if (cue.hotCueNumber != 0) {
                    auto& cueCache = hotCache_[DeckReference(update.player, cue.hotCueNumber)];
                    cueCache.insert_or_assign(tagKey, entry);
                }
            }
        }
    }
    deliverAnalysisTagUpdate(update.player, fileExtension, typeTag, tag);
}

void AnalysisTagFinder::deliverAnalysisTagUpdate(int player,
                                                 const std::string& fileExtension,
                                                 const std::string& typeTag,
                                                 const std::shared_ptr<AnalysisTag>& tag) {
    const std::string tagKey = typeTag + fileExtension;
    std::vector<AnalysisTagListenerPtr> listeners;
    {
        std::lock_guard<std::mutex> lock(listenersMutex_);
        auto it = analysisTagListeners_.find(tagKey);
        if (it != analysisTagListeners_.end()) {
            listeners = it->second;
        }
    }
    if (listeners.empty()) {
        return;
    }
    AnalysisTagUpdate update(player, fileExtension, typeTag, tag);
    for (const auto& listener : listeners) {
        if (!listener) {
            continue;
        }
        try {
            listener->analysisChanged(update);
        } catch (...) {
        }
    }
}

void AnalysisTagFinder::deliverTagLossUpdate(int player,
                                             const std::unordered_map<std::string, CacheEntry>& tags) {
    for (const auto& [tagKey, entry] : tags) {
        deliverAnalysisTagUpdate(player, entry.fileExtension, entry.typeTag, nullptr);
    }
}

void AnalysisTagFinder::primeCache() {
    if (!isRunning()) {
        return;
    }
    auto loaded = MetadataFinder::getInstance().getLoadedTracks();
    for (const auto& [deck, metadata] : loaded) {
        if (deck.getHotCue() == 0 && metadata) {
            handleUpdate(TrackMetadataUpdate(deck.getPlayer(), metadata));
        }
    }
}

} // namespace beatlink::data
