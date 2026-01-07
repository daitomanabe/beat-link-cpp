#include "beatlink/data/MetadataFinder.hpp"

#include "beatlink/DeviceFinder.hpp"
#include "beatlink/VirtualCdj.hpp"
#include "beatlink/data/OpusProvider.hpp"
#include "beatlink/dbserver/BinaryField.hpp"
#include "beatlink/dbserver/Message.hpp"
#include "beatlink/dbserver/NumberField.hpp"

#include <algorithm>
#include <chrono>
#include <iostream>
#include <stdexcept>

namespace beatlink::data {

namespace {
constexpr size_t kMaxPendingUpdates = 100;

struct MenuLockGuard {
    explicit MenuLockGuard(beatlink::dbserver::Client& client)
        : client_(client)
    {
    }

    ~MenuLockGuard() {
        client_.unlockForMenuOperations();
    }

    beatlink::dbserver::Client& client_;
};
}

MetadataFinder& MetadataFinder::getInstance() {
    static MetadataFinder instance;
    return instance;
}

MetadataFinder::MetadataFinder() {
    updateListener_ = std::make_shared<beatlink::DeviceUpdateCallbacks>(
        [this](const beatlink::DeviceUpdate& update) {
            auto cdj = dynamic_cast<const beatlink::CdjStatus*>(&update);
            if (!cdj) {
                return;
            }
            enqueueUpdate(*cdj);
        });

    announcementListener_ = std::make_shared<beatlink::DeviceAnnouncementCallbacks>(
        [this](const beatlink::DeviceAnnouncement& announcement) {
            if (announcement.getDeviceNumber() == 25 && announcement.getDeviceName() == "NXS-GW") {
                return;
            }
            if ((((announcement.getDeviceNumber() > 0x0f) && announcement.getDeviceNumber() < 0x20) ||
                 announcement.getDeviceNumber() > 40) &&
                announcement.getDeviceName().rfind("rekordbox", 0) == 0) {
                recordMount(SlotReference::getSlotReference(announcement.getDeviceNumber(),
                                                            beatlink::TrackSourceSlot::COLLECTION));
            }
        },
        [this](const beatlink::DeviceAnnouncement& announcement) {
            if (announcement.getDeviceNumber() == 25 && announcement.getDeviceName() == "NXS-GW") {
                return;
            }
            clearMetadata(announcement);
            if (announcement.getDeviceNumber() < 0x10) {
                removeMount(SlotReference::getSlotReference(announcement.getDeviceNumber(), beatlink::TrackSourceSlot::CD_SLOT));
                removeMount(SlotReference::getSlotReference(announcement.getDeviceNumber(), beatlink::TrackSourceSlot::USB_SLOT));
                removeMount(SlotReference::getSlotReference(announcement.getDeviceNumber(), beatlink::TrackSourceSlot::SD_SLOT));
            } else if ((((announcement.getDeviceNumber() > 0x0f) && announcement.getDeviceNumber() < 0x20) ||
                        announcement.getDeviceNumber() > 40) &&
                       announcement.getDeviceName().rfind("rekordbox", 0) == 0) {
                removeMount(SlotReference::getSlotReference(announcement.getDeviceNumber(),
                                                            beatlink::TrackSourceSlot::COLLECTION));
            }
        });

    lifecycleListener_ = std::make_shared<beatlink::LifecycleCallbacks>(
        [](beatlink::LifecycleParticipant&) {},
        [this](beatlink::LifecycleParticipant&) {
            if (isRunning()) {
                stop();
            }
        });

    mediaDetailsListener_ = std::make_shared<beatlink::MediaDetailsCallbacks>(
        [this](const beatlink::MediaDetails& details) {
            {
                std::lock_guard<std::mutex> lock(mediaDetailsMutex_);
                mediaDetails_.insert_or_assign(details.getSlotReference(), details);
            }

            bool mounted = false;
            {
                std::lock_guard<std::mutex> lock(mediaMountsMutex_);
                mounted = (mediaMounts_.find(details.getSlotReference()) != mediaMounts_.end());
            }

            if (!mounted) {
                std::cerr << "MetadataFinder: Discarding media details for unmounted slot "
                          << details.getSlotReference().toString() << std::endl;
                std::lock_guard<std::mutex> lock(mediaDetailsMutex_);
                mediaDetails_.erase(details.getSlotReference());
                return;
            }

            for (const auto& listener : getMountListeners()) {
                if (!listener) {
                    continue;
                }
                try {
                    listener->detailsAvailable(details);
                } catch (...) {
                }
            }
        });

    beatlink::VirtualCdj::getInstance().addMediaDetailsListener(mediaDetailsListener_);
}

std::shared_ptr<TrackMetadata> MetadataFinder::requestMetadataFrom(const beatlink::CdjStatus& status) {
    if (status.getTrackSourceSlot() == beatlink::TrackSourceSlot::NO_TRACK || status.getRekordboxId() == 0) {
        return nullptr;
    }
    DataReference track(status.getTrackSourcePlayer(), status.getTrackSourceSlot(),
                        status.getRekordboxId(), status.getTrackType());
    return requestMetadataFrom(track);
}

std::shared_ptr<TrackMetadata> MetadataFinder::requestMetadataFrom(const DataReference& track) {
    return requestMetadataInternal(track, false);
}

std::shared_ptr<TrackMetadata> MetadataFinder::requestMetadataFrom(const DataReference& track,
                                                                   beatlink::TrackType /*trackType*/) {
    return requestMetadataInternal(track, false);
}

std::shared_ptr<TrackMetadata> MetadataFinder::requestMetadataInternal(const DataReference& track,
                                                                       bool failIfPassive) {
    auto mediaDetails = getMediaDetailsFor(track.getSlotReference());
    if (mediaDetails) {
        auto provided = compositeProvider_.getTrackMetadata(*mediaDetails, track);
        if (provided) {
            return provided;
        }
    }

    if (passive_.load() && failIfPassive && track.getSlot() != beatlink::TrackSourceSlot::COLLECTION) {
        return nullptr;
    }

    auto announcement = beatlink::DeviceFinder::getInstance().getLatestAnnouncementFrom(track.getPlayer());
    if (announcement && announcement->isOpusQuad()) {
        return nullptr;
    }

    try {
        return beatlink::dbserver::ConnectionManager::getInstance().invokeWithClientSession(
            track.getPlayer(),
            [this, &track](beatlink::dbserver::Client& client) {
                return queryMetadata(track, client);
            },
            "requesting metadata");
    } catch (const std::exception& e) {
        std::cerr << "MetadataFinder: Problem requesting metadata: " << e.what() << std::endl;
    }
    return nullptr;
}

std::shared_ptr<TrackMetadata> MetadataFinder::queryMetadata(const DataReference& track,
                                                             beatlink::dbserver::Client& client) {
    if (!client.tryLockingForMenuOperations(std::chrono::seconds(MENU_TIMEOUT_SECONDS))) {
        throw std::runtime_error("Unable to lock the player for menu operations");
    }
    MenuLockGuard guard(client);

    const auto requestType = (track.getTrackType() == beatlink::TrackType::REKORDBOX)
                                 ? beatlink::dbserver::Message::KnownType::REKORDBOX_METADATA_REQ
                                 : beatlink::dbserver::Message::KnownType::UNANALYZED_METADATA_REQ;

    auto response = client.menuRequestTyped(requestType,
                                            beatlink::dbserver::Message::MenuIdentifier::MAIN_MENU,
                                            track.getSlot(),
                                            track.getTrackType(),
                                            {std::make_shared<beatlink::dbserver::NumberField>(track.getRekordboxId())});

    const auto count = response.getMenuResultsCount();
    if (count == beatlink::dbserver::Message::NO_MENU_RESULTS_AVAILABLE) {
        return nullptr;
    }

    auto items = client.renderMenuItems(beatlink::dbserver::Message::MenuIdentifier::MAIN_MENU,
                                        track.getSlot(),
                                        track.getTrackType(),
                                        response);

    std::shared_ptr<CueList> cueList;
    if (track.getTrackType() == beatlink::TrackType::REKORDBOX) {
        cueList = getCueList(track.getRekordboxId(), track.getSlot(), client);
    }

    return std::make_shared<TrackMetadata>(track, track.getTrackType(), std::move(items), cueList);
}

std::shared_ptr<CueList> MetadataFinder::getCueList(int rekordboxId, beatlink::TrackSourceSlot slot,
                                                    beatlink::dbserver::Client& client) {
    auto response = client.simpleRequest(
        beatlink::dbserver::Message::KnownType::CUE_LIST_EXT_REQ,
        std::nullopt,
        {std::make_shared<beatlink::dbserver::NumberField>(client.buildRMST(
             beatlink::dbserver::Message::MenuIdentifier::DATA, slot)),
         std::make_shared<beatlink::dbserver::NumberField>(rekordboxId),
         std::make_shared<beatlink::dbserver::NumberField>(beatlink::dbserver::NumberField::WORD_0)});

    if (response.knownType && *response.knownType == beatlink::dbserver::Message::KnownType::CUE_LIST_EXT) {
        return std::make_shared<CueList>(response);
    }

    response = client.simpleRequest(
        beatlink::dbserver::Message::KnownType::CUE_LIST_REQ,
        std::nullopt,
        {std::make_shared<beatlink::dbserver::NumberField>(client.buildRMST(
             beatlink::dbserver::Message::MenuIdentifier::DATA, slot)),
         std::make_shared<beatlink::dbserver::NumberField>(rekordboxId)});

    if (response.knownType && *response.knownType == beatlink::dbserver::Message::KnownType::CUE_LIST) {
        return std::make_shared<CueList>(response);
    }

    std::cerr << "MetadataFinder: Unexpected response type when requesting cue list" << std::endl;
    return nullptr;
}

std::vector<beatlink::dbserver::Message> MetadataFinder::getFullTrackList(beatlink::TrackSourceSlot slot,
                                                                          beatlink::dbserver::Client& client,
                                                                          int sortOrder) {
    if (!client.tryLockingForMenuOperations(std::chrono::seconds(MENU_TIMEOUT_SECONDS))) {
        throw std::runtime_error("Unable to lock the player for menu operations");
    }
    MenuLockGuard guard(client);

    auto response = client.menuRequest(beatlink::dbserver::Message::KnownType::TRACK_MENU_REQ,
                                       beatlink::dbserver::Message::MenuIdentifier::MAIN_MENU,
                                       slot,
                                       {std::make_shared<beatlink::dbserver::NumberField>(sortOrder)});

    const auto count = response.getMenuResultsCount();
    if (count == beatlink::dbserver::Message::NO_MENU_RESULTS_AVAILABLE || count == 0) {
        return {};
    }

    return client.renderMenuItems(beatlink::dbserver::Message::MenuIdentifier::MAIN_MENU,
                                  slot,
                                  beatlink::TrackType::REKORDBOX,
                                  response);
}

std::vector<beatlink::dbserver::Message> MetadataFinder::getPlaylistItems(beatlink::TrackSourceSlot slot,
                                                                          int sortOrder,
                                                                          int playlistOrFolderId,
                                                                          bool folder,
                                                                          beatlink::dbserver::Client& client) {
    if (!client.tryLockingForMenuOperations(std::chrono::seconds(MENU_TIMEOUT_SECONDS))) {
        throw std::runtime_error("Unable to lock player for menu operations");
    }
    MenuLockGuard guard(client);

    auto response = client.menuRequest(beatlink::dbserver::Message::KnownType::PLAYLIST_REQ,
                                       beatlink::dbserver::Message::MenuIdentifier::MAIN_MENU,
                                       slot,
                                       {std::make_shared<beatlink::dbserver::NumberField>(sortOrder),
                                        std::make_shared<beatlink::dbserver::NumberField>(playlistOrFolderId),
                                        std::make_shared<beatlink::dbserver::NumberField>(folder ? 1 : 0)});

    const auto count = response.getMenuResultsCount();
    if (count == beatlink::dbserver::Message::NO_MENU_RESULTS_AVAILABLE || count == 0) {
        return {};
    }

    return client.renderMenuItems(beatlink::dbserver::Message::MenuIdentifier::MAIN_MENU,
                                  slot,
                                  beatlink::TrackType::REKORDBOX,
                                  response);
}

std::vector<beatlink::dbserver::Message> MetadataFinder::requestPlaylistItemsFrom(int player,
                                                                                  beatlink::TrackSourceSlot slot,
                                                                                  int sortOrder,
                                                                                  int playlistOrFolderId,
                                                                                  bool folder) {
    return beatlink::dbserver::ConnectionManager::getInstance().invokeWithClientSession(
        player,
        [this, slot, sortOrder, playlistOrFolderId, folder](beatlink::dbserver::Client& client) {
            return getPlaylistItems(slot, sortOrder, playlistOrFolderId, folder, client);
        },
        "requesting playlist information");
}

std::unordered_map<DeckReference, std::shared_ptr<TrackMetadata>, DeckReferenceHash>
MetadataFinder::getLoadedTracks() {
    ensureRunning();
    std::lock_guard<std::mutex> lock(hotCacheMutex_);
    return hotCache_;
}

std::shared_ptr<TrackMetadata> MetadataFinder::getLatestMetadataFor(int player) {
    ensureRunning();
    std::lock_guard<std::mutex> lock(hotCacheMutex_);
    auto it = hotCache_.find(DeckReference(player, 0));
    if (it == hotCache_.end()) {
        return nullptr;
    }
    return it->second;
}

std::shared_ptr<TrackMetadata> MetadataFinder::getLatestMetadataFor(const beatlink::DeviceUpdate& update) {
    return getLatestMetadataFor(update.getDeviceNumber());
}

std::unordered_set<SlotReference, SlotReferenceHash> MetadataFinder::getMountedMediaSlots() const {
    std::lock_guard<std::mutex> lock(mediaMountsMutex_);
    return mediaMounts_;
}

std::vector<beatlink::MediaDetails> MetadataFinder::getMountedMediaDetails() const {
    std::lock_guard<std::mutex> lock(mediaDetailsMutex_);
    std::vector<beatlink::MediaDetails> results;
    results.reserve(mediaDetails_.size());
    for (const auto& [slot, details] : mediaDetails_) {
        results.push_back(details);
    }
    return results;
}

std::optional<beatlink::MediaDetails> MetadataFinder::getMediaDetailsFor(const SlotReference& slot) const {
    std::lock_guard<std::mutex> lock(mediaDetailsMutex_);
    auto it = mediaDetails_.find(slot);
    if (it == mediaDetails_.end()) {
        return std::nullopt;
    }
    return it->second;
}

void MetadataFinder::addMountListener(const MountListenerPtr& listener) {
    if (!listener) {
        return;
    }
    std::lock_guard<std::mutex> lock(mountListenersMutex_);
    if (std::find(mountListeners_.begin(), mountListeners_.end(), listener) == mountListeners_.end()) {
        mountListeners_.push_back(listener);
    }
}

void MetadataFinder::removeMountListener(const MountListenerPtr& listener) {
    if (!listener) {
        return;
    }
    std::lock_guard<std::mutex> lock(mountListenersMutex_);
    mountListeners_.erase(std::remove(mountListeners_.begin(), mountListeners_.end(), listener),
                          mountListeners_.end());
}

std::vector<MountListenerPtr> MetadataFinder::getMountListeners() const {
    std::lock_guard<std::mutex> lock(mountListenersMutex_);
    return mountListeners_;
}

void MetadataFinder::addTrackMetadataListener(const TrackMetadataListenerPtr& listener) {
    if (!listener) {
        return;
    }
    std::lock_guard<std::mutex> lock(trackListenersMutex_);
    if (std::find(trackListeners_.begin(), trackListeners_.end(), listener) == trackListeners_.end()) {
        trackListeners_.push_back(listener);
    }
}

void MetadataFinder::removeTrackMetadataListener(const TrackMetadataListenerPtr& listener) {
    if (!listener) {
        return;
    }
    std::lock_guard<std::mutex> lock(trackListenersMutex_);
    trackListeners_.erase(std::remove(trackListeners_.begin(), trackListeners_.end(), listener),
                          trackListeners_.end());
}

std::vector<TrackMetadataListenerPtr> MetadataFinder::getTrackMetadataListeners() const {
    std::lock_guard<std::mutex> lock(trackListenersMutex_);
    return trackListeners_;
}

void MetadataFinder::addMetadataProvider(const MetadataProviderPtr& provider) {
    if (!provider) {
        return;
    }

    const auto supportedMedia = provider->supportedMedia();
    std::lock_guard<std::mutex> lock(metadataProvidersMutex_);

    if (supportedMedia.empty()) {
        auto& providers = metadataProviders_[""];
        if (std::find(providers.begin(), providers.end(), provider) == providers.end()) {
            providers.push_back(provider);
        }
        return;
    }

    for (const auto& details : supportedMedia) {
        auto& providers = metadataProviders_[details.hashKey()];
        if (std::find(providers.begin(), providers.end(), provider) == providers.end()) {
            providers.push_back(provider);
        }
    }
}

void MetadataFinder::removeMetadataProvider(const MetadataProviderPtr& provider) {
    if (!provider) {
        return;
    }
    std::lock_guard<std::mutex> lock(metadataProvidersMutex_);
    for (auto& [key, providers] : metadataProviders_) {
        providers.erase(std::remove(providers.begin(), providers.end(), provider), providers.end());
    }
}

std::vector<MetadataProviderPtr> MetadataFinder::getMetadataProviders(const beatlink::MediaDetails* sourceMedia) const {
    std::lock_guard<std::mutex> lock(metadataProvidersMutex_);
    const std::string key = sourceMedia ? sourceMedia->hashKey() : std::string();
    auto it = metadataProviders_.find(key);
    if (it == metadataProviders_.end()) {
        return {};
    }
    return it->second;
}

MetadataProvider& MetadataFinder::getCompositeProvider() {
    return compositeProvider_;
}

std::shared_ptr<TrackMetadata> MetadataFinder::CompositeProvider::getTrackMetadata(
    const beatlink::MediaDetails& sourceMedia,
    const DataReference& track) {
    for (const auto& provider : owner_.getMetadataProviders(&sourceMedia)) {
        if (!provider) {
            continue;
        }
        auto result = provider->getTrackMetadata(sourceMedia, track);
        if (result) {
            return result;
        }
    }
    for (const auto& provider : owner_.getMetadataProviders(nullptr)) {
        if (!provider) {
            continue;
        }
        auto result = provider->getTrackMetadata(sourceMedia, track);
        if (result) {
            return result;
        }
    }
    return nullptr;
}

std::shared_ptr<AlbumArt> MetadataFinder::CompositeProvider::getAlbumArt(
    const beatlink::MediaDetails& sourceMedia,
    const DataReference& art) {
    for (const auto& provider : owner_.getMetadataProviders(&sourceMedia)) {
        if (!provider) {
            continue;
        }
        auto result = provider->getAlbumArt(sourceMedia, art);
        if (result) {
            return result;
        }
    }
    for (const auto& provider : owner_.getMetadataProviders(nullptr)) {
        if (!provider) {
            continue;
        }
        auto result = provider->getAlbumArt(sourceMedia, art);
        if (result) {
            return result;
        }
    }
    return nullptr;
}

std::shared_ptr<BeatGrid> MetadataFinder::CompositeProvider::getBeatGrid(
    const beatlink::MediaDetails& sourceMedia,
    const DataReference& track) {
    for (const auto& provider : owner_.getMetadataProviders(&sourceMedia)) {
        if (!provider) {
            continue;
        }
        auto result = provider->getBeatGrid(sourceMedia, track);
        if (result) {
            return result;
        }
    }
    for (const auto& provider : owner_.getMetadataProviders(nullptr)) {
        if (!provider) {
            continue;
        }
        auto result = provider->getBeatGrid(sourceMedia, track);
        if (result) {
            return result;
        }
    }
    return nullptr;
}

std::shared_ptr<CueList> MetadataFinder::CompositeProvider::getCueList(
    const beatlink::MediaDetails& sourceMedia,
    const DataReference& track) {
    for (const auto& provider : owner_.getMetadataProviders(&sourceMedia)) {
        if (!provider) {
            continue;
        }
        auto result = provider->getCueList(sourceMedia, track);
        if (result) {
            return result;
        }
    }
    for (const auto& provider : owner_.getMetadataProviders(nullptr)) {
        if (!provider) {
            continue;
        }
        auto result = provider->getCueList(sourceMedia, track);
        if (result) {
            return result;
        }
    }
    return nullptr;
}

std::shared_ptr<WaveformPreview> MetadataFinder::CompositeProvider::getWaveformPreview(
    const beatlink::MediaDetails& sourceMedia,
    const DataReference& track) {
    for (const auto& provider : owner_.getMetadataProviders(&sourceMedia)) {
        if (!provider) {
            continue;
        }
        auto result = provider->getWaveformPreview(sourceMedia, track);
        if (result) {
            return result;
        }
    }
    for (const auto& provider : owner_.getMetadataProviders(nullptr)) {
        if (!provider) {
            continue;
        }
        auto result = provider->getWaveformPreview(sourceMedia, track);
        if (result) {
            return result;
        }
    }
    return nullptr;
}

std::shared_ptr<WaveformDetail> MetadataFinder::CompositeProvider::getWaveformDetail(
    const beatlink::MediaDetails& sourceMedia,
    const DataReference& track) {
    for (const auto& provider : owner_.getMetadataProviders(&sourceMedia)) {
        if (!provider) {
            continue;
        }
        auto result = provider->getWaveformDetail(sourceMedia, track);
        if (result) {
            return result;
        }
    }
    for (const auto& provider : owner_.getMetadataProviders(nullptr)) {
        if (!provider) {
            continue;
        }
        auto result = provider->getWaveformDetail(sourceMedia, track);
        if (result) {
            return result;
        }
    }
    return nullptr;
}

std::shared_ptr<AnalysisTag> MetadataFinder::CompositeProvider::getAnalysisSection(
    const beatlink::MediaDetails& sourceMedia,
    const DataReference& track,
    const std::string& fileExtension,
    const std::string& typeTag) {
    for (const auto& provider : owner_.getMetadataProviders(&sourceMedia)) {
        if (!provider) {
            continue;
        }
        auto result = provider->getAnalysisSection(sourceMedia, track, fileExtension, typeTag);
        if (result) {
            return result;
        }
    }
    for (const auto& provider : owner_.getMetadataProviders(nullptr)) {
        if (!provider) {
            continue;
        }
        auto result = provider->getAnalysisSection(sourceMedia, track, fileExtension, typeTag);
        if (result) {
            return result;
        }
    }
    return nullptr;
}

void MetadataFinder::clearDeck(const beatlink::CdjStatus& update) {
    std::shared_ptr<TrackMetadata> removed;
    {
        std::lock_guard<std::mutex> lock(hotCacheMutex_);
        auto it = hotCache_.find(DeckReference(update.getDeviceNumber(), 0));
        if (it != hotCache_.end()) {
            removed = it->second;
            hotCache_.erase(it);
        }
    }
    if (removed) {
        deliverTrackMetadataUpdate(update.getDeviceNumber(), nullptr);
    }
}

void MetadataFinder::clearMetadata(const beatlink::DeviceAnnouncement& announcement) {
    const int player = announcement.getDeviceNumber();
    std::vector<int> deckHotCues;

    {
        std::lock_guard<std::mutex> lock(hotCacheMutex_);
        for (const auto& [deck, metadata] : hotCache_) {
            if (deck.getPlayer() == player) {
                deckHotCues.push_back(deck.getHotCue());
            }
        }
        for (const auto hotCue : deckHotCues) {
            hotCache_.erase(DeckReference(player, hotCue));
        }
    }

    if (std::find(deckHotCues.begin(), deckHotCues.end(), 0) != deckHotCues.end()) {
        deliverTrackMetadataUpdate(player, nullptr);
    }
}

void MetadataFinder::updateMetadata(const beatlink::CdjStatus& update,
                                    const std::shared_ptr<TrackMetadata>& data) {
    {
        std::lock_guard<std::mutex> lock(hotCacheMutex_);
        hotCache_[DeckReference(update.getDeviceNumber(), 0)] = data;
        if (data && data->getCueList()) {
            for (const auto& entry : data->getCueList()->getEntries()) {
                if (entry.hotCueNumber != 0) {
                    hotCache_[DeckReference(update.getDeviceNumber(), entry.hotCueNumber)] = data;
                }
            }
        }
    }
    deliverTrackMetadataUpdate(update.getDeviceNumber(), data);
}

void MetadataFinder::flushHotCacheSlot(const SlotReference& slot) {
    std::vector<DeckReference> toRemove;
    {
        std::lock_guard<std::mutex> lock(hotCacheMutex_);
        for (const auto& [deck, metadata] : hotCache_) {
            if (metadata && SlotReference::getSlotReference(metadata->getTrackReference()) == slot) {
                toRemove.push_back(deck);
            }
        }
        for (const auto& deck : toRemove) {
            hotCache_.erase(deck);
        }
    }
}

void MetadataFinder::recordMount(const SlotReference& slot) {
    bool inserted = false;
    {
        std::lock_guard<std::mutex> lock(mediaMountsMutex_);
        inserted = mediaMounts_.insert(slot).second;
    }
    if (inserted) {
        deliverMountUpdate(slot, true);
    }

    bool haveDetails = false;
    {
        std::lock_guard<std::mutex> lock(mediaDetailsMutex_);
        haveDetails = (mediaDetails_.find(slot) != mediaDetails_.end());
    }

    if (!haveDetails && !beatlink::VirtualCdj::getInstance().inOpusQuadCompatibilityMode()) {
        try {
            beatlink::VirtualCdj::getInstance().sendMediaQuery(slot.getPlayer(), slot.getSlot());
        } catch (const std::exception& e) {
            std::cerr << "MetadataFinder: Problem requesting media details for " << slot.toString()
                      << ": " << e.what() << std::endl;
        }
    }

    if (OpusProvider::getInstance().isRunning()) {
        OpusProvider::getInstance().pollAndSendMediaDetails(slot.getPlayer());
    }
}

void MetadataFinder::removeMount(const SlotReference& slot) {
    {
        std::lock_guard<std::mutex> lock(mediaDetailsMutex_);
        mediaDetails_.erase(slot);
    }
    bool removed = false;
    {
        std::lock_guard<std::mutex> lock(mediaMountsMutex_);
        removed = mediaMounts_.erase(slot) > 0;
    }
    if (removed) {
        deliverMountUpdate(slot, false);
    }
}

void MetadataFinder::deliverMountUpdate(const SlotReference& slot, bool mounted) {
    auto listeners = getMountListeners();
    for (const auto& listener : listeners) {
        if (!listener) {
            continue;
        }
        try {
            if (mounted) {
                listener->mediaMounted(slot);
            } else {
                listener->mediaUnmounted(slot);
            }
        } catch (...) {
        }
    }
}

void MetadataFinder::deliverTrackMetadataUpdate(int player,
                                                const std::shared_ptr<TrackMetadata>& metadata) {
    TrackMetadataUpdate update(player, metadata);
    auto listeners = getTrackMetadataListeners();
    for (const auto& listener : listeners) {
        if (!listener) {
            continue;
        }
        try {
            listener->metadataChanged(update);
        } catch (...) {
        }
    }
}

void MetadataFinder::handleUpdate(const beatlink::CdjStatus& update) {
    if (update.isLocalUsbEmpty()) {
        const auto slot = SlotReference::getSlotReference(update.getDeviceNumber(), beatlink::TrackSourceSlot::USB_SLOT);
        flushHotCacheSlot(slot);
        removeMount(slot);
    } else if (update.isLocalUsbLoaded()) {
        recordMount(SlotReference::getSlotReference(update.getDeviceNumber(), beatlink::TrackSourceSlot::USB_SLOT));
    }

    if (update.isLocalSdEmpty()) {
        const auto slot = SlotReference::getSlotReference(update.getDeviceNumber(), beatlink::TrackSourceSlot::SD_SLOT);
        flushHotCacheSlot(slot);
        removeMount(slot);
    } else if (update.isLocalSdLoaded()) {
        recordMount(SlotReference::getSlotReference(update.getDeviceNumber(), beatlink::TrackSourceSlot::SD_SLOT));
    }

    if (update.isDiscSlotEmpty()) {
        removeMount(SlotReference::getSlotReference(update.getDeviceNumber(), beatlink::TrackSourceSlot::CD_SLOT));
    } else {
        recordMount(SlotReference::getSlotReference(update.getDeviceNumber(), beatlink::TrackSourceSlot::CD_SLOT));
    }

    if (update.getTrackType() == beatlink::TrackType::UNKNOWN ||
        update.getTrackType() == beatlink::TrackType::NO_TRACK ||
        update.getTrackSourceSlot() == beatlink::TrackSourceSlot::NO_TRACK ||
        update.getTrackSourceSlot() == beatlink::TrackSourceSlot::UNKNOWN ||
        update.getRekordboxId() == 0) {
        clearDeck(update);
        return;
    }

    const DataReference trackReference(update.getTrackSourcePlayer(), update.getTrackSourceSlot(),
                                       update.getRekordboxId(), update.getTrackType());

    std::shared_ptr<TrackMetadata> lastMetadata;
    {
        std::lock_guard<std::mutex> lock(hotCacheMutex_);
        auto it = hotCache_.find(DeckReference(update.getDeviceNumber(), 0));
        if (it != hotCache_.end()) {
            lastMetadata = it->second;
        }
    }

    if (!lastMetadata || lastMetadata->getTrackReference() != trackReference) {
        {
            std::lock_guard<std::mutex> lock(hotCacheMutex_);
            for (const auto& [deck, metadata] : hotCache_) {
                if (metadata && metadata->getTrackReference() == trackReference) {
                    updateMetadata(update, metadata);
                    return;
                }
            }
        }

        if (beatlink::dbserver::ConnectionManager::getInstance().getPlayerDBServerPort(trackReference.getPlayer()) > 0 ||
            beatlink::VirtualCdj::getInstance().inOpusQuadCompatibilityMode()) {
            bool shouldRequest = false;
            {
                std::lock_guard<std::mutex> lock(activeRequestsMutex_);
                shouldRequest = activeRequests_.insert(trackReference.getPlayer()).second;
            }

            if (shouldRequest) {
                clearDeck(update);
                std::thread([this, update, trackReference]() {
                    try {
                        auto data = requestMetadataInternal(trackReference, true);
                        if (data) {
                            updateMetadata(update, data);
                        }
                    } catch (const std::exception& e) {
                        std::cerr << "MetadataFinder: Problem requesting track metadata: " << e.what() << std::endl;
                    }
                    std::lock_guard<std::mutex> lock(activeRequestsMutex_);
                    activeRequests_.erase(trackReference.getPlayer());
                }).detach();
            }
        }
    }
}

void MetadataFinder::enqueueUpdate(const beatlink::CdjStatus& update) {
    std::unique_lock<std::mutex> lock(pendingMutex_);
    if (pendingUpdates_.size() >= kMaxPendingUpdates) {
        std::cerr << "MetadataFinder: Discarding CDJ update because queue is backed up" << std::endl;
        return;
    }
    pendingUpdates_.push_back(update);
    lock.unlock();
    pendingCv_.notify_one();
}

void MetadataFinder::start() {
    if (isRunning()) {
        return;
    }

    beatlink::dbserver::ConnectionManager::getInstance().addLifecycleListener(lifecycleListener_);
    beatlink::dbserver::ConnectionManager::getInstance().start();

    beatlink::DeviceFinder::getInstance().start();
    beatlink::DeviceFinder::getInstance().addDeviceAnnouncementListener(announcementListener_);

    beatlink::VirtualCdj::getInstance().addLifecycleListener(lifecycleListener_);
    beatlink::VirtualCdj::getInstance().start();
    beatlink::VirtualCdj::getInstance().addUpdateListener(updateListener_);

    running_.store(true);
    queueHandler_ = std::thread([this]() {
        while (isRunning()) {
            std::optional<beatlink::CdjStatus> update;
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
                std::cerr << "MetadataFinder: Problem handling CDJ status update: " << e.what() << std::endl;
            }
        }
    });

    deliverLifecycleAnnouncement(true);

    const auto devices = beatlink::DeviceFinder::getInstance().getCurrentDevices();
    for (const auto& device : devices) {
        if (announcementListener_) {
            announcementListener_->deviceFound(device);
        }
    }
}

void MetadataFinder::stop() {
    if (!isRunning()) {
        return;
    }

    beatlink::VirtualCdj::getInstance().removeUpdateListener(updateListener_);
    beatlink::DeviceFinder::getInstance().removeDeviceAnnouncementListener(announcementListener_);
    beatlink::dbserver::ConnectionManager::getInstance().removeLifecycleListener(lifecycleListener_);
    beatlink::VirtualCdj::getInstance().removeLifecycleListener(lifecycleListener_);

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
        for (const auto& [deck, metadata] : hotCache_) {
            if (deck.getHotCue() == 0) {
                decks.push_back(deck);
            }
        }
        hotCache_.clear();
    }

    for (const auto& deck : decks) {
        deliverTrackMetadataUpdate(deck.getPlayer(), nullptr);
    }

    deliverLifecycleAnnouncement(false);
}

} // namespace beatlink::data
