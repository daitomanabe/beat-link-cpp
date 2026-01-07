#include "beatlink/data/WaveformFinder.hpp"

#include "beatlink/DeviceFinder.hpp"
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

WaveformFinder& WaveformFinder::getInstance() {
    static WaveformFinder instance;
    return instance;
}

WaveformFinder::WaveformFinder() {
    metadataListener_ = std::make_shared<TrackMetadataCallbacks>(
        [this](const TrackMetadataUpdate& update) {
            enqueueUpdate(update);
        });

    mountListener_ = std::make_shared<MountCallbacks>(
        [](const SlotReference&) {},
        [this](const SlotReference& slot) {
            std::vector<DeckReference> previewRemove;
            {
                std::lock_guard<std::mutex> lock(previewCacheMutex_);
                for (const auto& [deck, preview] : previewHotCache_) {
                    if (preview && SlotReference::getSlotReference(preview->dataReference) == slot) {
                        previewRemove.push_back(deck);
                    }
                }
                for (const auto& deck : previewRemove) {
                    previewHotCache_.erase(deck);
                }
            }

            std::vector<DeckReference> detailRemove;
            {
                std::lock_guard<std::mutex> lock(detailCacheMutex_);
                for (const auto& [deck, detail] : detailHotCache_) {
                    if (detail && SlotReference::getSlotReference(detail->dataReference) == slot) {
                        detailRemove.push_back(deck);
                    }
                }
                for (const auto& deck : detailRemove) {
                    detailHotCache_.erase(deck);
                }
            }
        });

    announcementListener_ = std::make_shared<beatlink::DeviceAnnouncementCallbacks>(
        [](const beatlink::DeviceAnnouncement&) {},
        [this](const beatlink::DeviceAnnouncement& announcement) {
            if (announcement.getDeviceNumber() == 25 && announcement.getDeviceName() == "NXS-GW") {
                return;
            }
            clearWaveforms(announcement);
        });

    lifecycleListener_ = std::make_shared<beatlink::LifecycleCallbacks>(
        [](beatlink::LifecycleParticipant&) {},
        [this](beatlink::LifecycleParticipant&) {
            if (isRunning()) {
                stop();
            }
        });
}

void WaveformFinder::setFindDetails(bool findDetails) {
    const bool oldValue = findDetails_.exchange(findDetails);
    if (oldValue == findDetails) {
        return;
    }

    if (findDetails) {
        primeCache();
        return;
    }

    std::vector<DeckReference> detailDecks;
    {
        std::lock_guard<std::mutex> lock(detailCacheMutex_);
        for (const auto& [deck, detail] : detailHotCache_) {
            if (deck.getHotCue() == 0) {
                detailDecks.push_back(deck);
            }
        }
        detailHotCache_.clear();
    }

    for (const auto& deck : detailDecks) {
        deliverWaveformDetailUpdate(deck.getPlayer(), nullptr);
    }
}

void WaveformFinder::setPreferredStyle(WaveformStyle style) {
    const auto oldStyle = preferredStyle_.exchange(style);
    if (oldStyle != style) {
        clearAllWaveforms();
        primeCache();
    }
}

void WaveformFinder::setColorPreferred(bool preferColor) {
    setPreferredStyle(preferColor ? WaveformStyle::RGB : WaveformStyle::BLUE);
}

std::unordered_map<DeckReference, std::shared_ptr<WaveformPreview>, DeckReferenceHash>
WaveformFinder::getLoadedPreviews() {
    ensureRunning();
    std::lock_guard<std::mutex> lock(previewCacheMutex_);
    return previewHotCache_;
}

std::unordered_map<DeckReference, std::shared_ptr<WaveformDetail>, DeckReferenceHash>
WaveformFinder::getLoadedDetails() {
    ensureRunning();
    if (!isFindingDetails()) {
        throw std::runtime_error("WaveformFinder is not configured to find waveform details");
    }
    std::lock_guard<std::mutex> lock(detailCacheMutex_);
    return detailHotCache_;
}

std::shared_ptr<WaveformPreview> WaveformFinder::getLatestPreviewFor(int player) {
    ensureRunning();
    std::lock_guard<std::mutex> lock(previewCacheMutex_);
    auto it = previewHotCache_.find(DeckReference(player, 0));
    if (it == previewHotCache_.end()) {
        return nullptr;
    }
    return it->second;
}

std::shared_ptr<WaveformPreview> WaveformFinder::getLatestPreviewFor(const beatlink::DeviceUpdate& update) {
    return getLatestPreviewFor(update.getDeviceNumber());
}

std::shared_ptr<WaveformDetail> WaveformFinder::getLatestDetailFor(int player) {
    ensureRunning();
    std::lock_guard<std::mutex> lock(detailCacheMutex_);
    auto it = detailHotCache_.find(DeckReference(player, 0));
    if (it == detailHotCache_.end()) {
        return nullptr;
    }
    return it->second;
}

std::shared_ptr<WaveformDetail> WaveformFinder::getLatestDetailFor(const beatlink::DeviceUpdate& update) {
    return getLatestDetailFor(update.getDeviceNumber());
}

std::shared_ptr<WaveformPreview> WaveformFinder::requestWaveformPreviewFrom(const DataReference& dataReference) {
    ensureRunning();
    {
        std::lock_guard<std::mutex> lock(previewCacheMutex_);
        for (const auto& [deck, preview] : previewHotCache_) {
            if (preview && preview->dataReference == dataReference) {
                return preview;
            }
        }
    }
    return requestPreviewInternal(dataReference, nullptr);
}

std::shared_ptr<WaveformDetail> WaveformFinder::requestWaveformDetailFrom(const DataReference& dataReference) {
    ensureRunning();
    {
        std::lock_guard<std::mutex> lock(detailCacheMutex_);
        for (const auto& [deck, detail] : detailHotCache_) {
            if (detail && detail->dataReference == dataReference) {
                return detail;
            }
        }
    }
    return requestDetailInternal(dataReference, nullptr);
}

std::shared_ptr<WaveformPreview> WaveformFinder::requestPreviewInternal(const DataReference& trackReference,
                                                                        const TrackMetadataUpdate* fromUpdate) {
    auto sourceDetails = MetadataFinder::getInstance().getMediaDetailsFor(trackReference.getSlotReference());
    if (sourceDetails) {
        auto provided = MetadataFinder::getInstance().getCompositeProvider().getWaveformPreview(*sourceDetails, trackReference);
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
                return getWaveformPreview(trackReference.getRekordboxId(),
                                          trackReference.getSlotReference(),
                                          trackReference.getTrackType(),
                                          fromUpdate,
                                          client);
            },
            "requesting waveform preview");
    } catch (const std::exception& e) {
        std::cerr << "WaveformFinder: Problem requesting waveform preview: " << e.what() << std::endl;
    }
    return nullptr;
}

std::shared_ptr<WaveformDetail> WaveformFinder::requestDetailInternal(const DataReference& trackReference,
                                                                      const TrackMetadataUpdate* fromUpdate) {
    auto sourceDetails = MetadataFinder::getInstance().getMediaDetailsFor(trackReference.getSlotReference());
    if (sourceDetails) {
        auto provided = MetadataFinder::getInstance().getCompositeProvider().getWaveformDetail(*sourceDetails, trackReference);
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
                return getWaveformDetail(trackReference.getRekordboxId(),
                                         trackReference.getSlotReference(),
                                         trackReference.getTrackType(),
                                         fromUpdate,
                                         client);
            },
            "requesting waveform detail");
    } catch (const std::exception& e) {
        std::cerr << "WaveformFinder: Problem requesting waveform detail: " << e.what() << std::endl;
    }
    return nullptr;
}

std::shared_ptr<WaveformPreview> WaveformFinder::getWaveformPreview(int rekordboxId,
                                                                    const SlotReference& slot,
                                                                    beatlink::TrackType trackType,
                                                                    const TrackMetadataUpdate* fromUpdate,
                                                                    beatlink::dbserver::Client& client) {
    const auto idField = std::make_shared<beatlink::dbserver::NumberField>(rekordboxId);

    if (getPreferredStyle() == WaveformStyle::RGB) {
        try {
            auto response = client.simpleRequest(
                beatlink::dbserver::Message::KnownType::ANLZ_TAG_REQ,
                beatlink::dbserver::Message::KnownType::ANLZ_TAG,
                {std::make_shared<beatlink::dbserver::NumberField>(
                     client.buildRMST(beatlink::dbserver::Message::MenuIdentifier::MAIN_MENU, slot.getSlot(), trackType)),
                 idField,
                 std::make_shared<beatlink::dbserver::NumberField>(beatlink::dbserver::Message::ANLZ_FILE_TAG_COLOR_WAVEFORM_PREVIEW),
                 std::make_shared<beatlink::dbserver::NumberField>(beatlink::dbserver::Message::ALNZ_FILE_TYPE_EXT)});
            if (response.knownType && *response.knownType != beatlink::dbserver::Message::KnownType::UNAVAILABLE &&
                response.arguments.size() > 3 && response.arguments[3]->getSize() > 0) {
                return std::make_shared<WaveformPreview>(DataReference(slot, rekordboxId, trackType), response, WaveformStyle::RGB);
            }
            if (retryUnanalyzedTrack(fromUpdate, retrying_, metadataListener_, "waveform")) {
                return nullptr;
            }
        } catch (const std::exception&) {
            if (retryUnanalyzedTrack(fromUpdate, retrying_, metadataListener_, "waveform")) {
                return nullptr;
            }
        }
    } else if (getPreferredStyle() == WaveformStyle::THREE_BAND) {
        try {
            auto response = client.simpleRequest(
                beatlink::dbserver::Message::KnownType::ANLZ_TAG_REQ,
                beatlink::dbserver::Message::KnownType::ANLZ_TAG,
                {std::make_shared<beatlink::dbserver::NumberField>(
                     client.buildRMST(beatlink::dbserver::Message::MenuIdentifier::MAIN_MENU, slot.getSlot(), trackType)),
                 idField,
                 std::make_shared<beatlink::dbserver::NumberField>(beatlink::dbserver::Message::ANLZ_FILE_TAG_3BAND_WAVEFORM_PREVIEW),
                 std::make_shared<beatlink::dbserver::NumberField>(beatlink::dbserver::Message::ALNZ_FILE_TYPE_2EX)});
            if (response.knownType && *response.knownType != beatlink::dbserver::Message::KnownType::UNAVAILABLE &&
                response.arguments.size() > 3 && response.arguments[3]->getSize() > 0) {
                return std::make_shared<WaveformPreview>(DataReference(slot, rekordboxId, trackType), response, WaveformStyle::THREE_BAND);
            }
            if (retryUnanalyzedTrack(fromUpdate, retrying_, metadataListener_, "waveform")) {
                return nullptr;
            }
        } catch (const std::exception&) {
            if (retryUnanalyzedTrack(fromUpdate, retrying_, metadataListener_, "waveform")) {
                return nullptr;
            }
        }
    }

    auto response = client.simpleRequest(
        beatlink::dbserver::Message::KnownType::WAVE_PREVIEW_REQ,
        beatlink::dbserver::Message::KnownType::WAVE_PREVIEW,
        {std::make_shared<beatlink::dbserver::NumberField>(
             client.buildRMST(beatlink::dbserver::Message::MenuIdentifier::DATA, slot.getSlot(), trackType)),
         std::make_shared<beatlink::dbserver::NumberField>(beatlink::dbserver::NumberField::WORD_1),
         idField,
         std::make_shared<beatlink::dbserver::NumberField>(beatlink::dbserver::NumberField::WORD_0)});

    if (response.knownType && *response.knownType != beatlink::dbserver::Message::KnownType::UNAVAILABLE &&
        response.arguments.size() > 3 && response.arguments[3]->getSize() > 0) {
        return std::make_shared<WaveformPreview>(DataReference(slot, rekordboxId, trackType), response, WaveformStyle::BLUE);
    }

    retryUnanalyzedTrack(fromUpdate, retrying_, metadataListener_, "waveform");
    return nullptr;
}

std::shared_ptr<WaveformDetail> WaveformFinder::getWaveformDetail(int rekordboxId,
                                                                  const SlotReference& slot,
                                                                  beatlink::TrackType trackType,
                                                                  const TrackMetadataUpdate* fromUpdate,
                                                                  beatlink::dbserver::Client& client) {
    const auto idField = std::make_shared<beatlink::dbserver::NumberField>(rekordboxId);

    if (getPreferredStyle() == WaveformStyle::RGB) {
        try {
            auto response = client.simpleRequest(
                beatlink::dbserver::Message::KnownType::ANLZ_TAG_REQ,
                beatlink::dbserver::Message::KnownType::ANLZ_TAG,
                {std::make_shared<beatlink::dbserver::NumberField>(
                     client.buildRMST(beatlink::dbserver::Message::MenuIdentifier::MAIN_MENU, slot.getSlot(), trackType)),
                 idField,
                 std::make_shared<beatlink::dbserver::NumberField>(beatlink::dbserver::Message::ANLZ_FILE_TAG_COLOR_WAVEFORM_DETAIL),
                 std::make_shared<beatlink::dbserver::NumberField>(beatlink::dbserver::Message::ALNZ_FILE_TYPE_EXT)});
            if (response.knownType && *response.knownType != beatlink::dbserver::Message::KnownType::UNAVAILABLE &&
                response.arguments.size() > 3 && response.arguments[3]->getSize() > 0) {
                return std::make_shared<WaveformDetail>(DataReference(slot, rekordboxId, trackType), response, WaveformStyle::RGB);
            }
            if (retryUnanalyzedTrack(fromUpdate, retrying_, metadataListener_, "waveform")) {
                return nullptr;
            }
        } catch (const std::exception&) {
            if (retryUnanalyzedTrack(fromUpdate, retrying_, metadataListener_, "waveform")) {
                return nullptr;
            }
        }
    } else if (getPreferredStyle() == WaveformStyle::THREE_BAND) {
        try {
            auto response = client.simpleRequest(
                beatlink::dbserver::Message::KnownType::ANLZ_TAG_REQ,
                beatlink::dbserver::Message::KnownType::ANLZ_TAG,
                {std::make_shared<beatlink::dbserver::NumberField>(
                     client.buildRMST(beatlink::dbserver::Message::MenuIdentifier::MAIN_MENU, slot.getSlot(), trackType)),
                 idField,
                 std::make_shared<beatlink::dbserver::NumberField>(beatlink::dbserver::Message::ANLZ_FILE_TAG_3BAND_WAVEFORM_DETAIL),
                 std::make_shared<beatlink::dbserver::NumberField>(beatlink::dbserver::Message::ALNZ_FILE_TYPE_2EX)});
            if (response.knownType && *response.knownType != beatlink::dbserver::Message::KnownType::UNAVAILABLE &&
                response.arguments.size() > 3 && response.arguments[3]->getSize() > 0) {
                return std::make_shared<WaveformDetail>(DataReference(slot, rekordboxId, trackType), response, WaveformStyle::THREE_BAND);
            }
            if (retryUnanalyzedTrack(fromUpdate, retrying_, metadataListener_, "waveform")) {
                return nullptr;
            }
        } catch (const std::exception&) {
            if (retryUnanalyzedTrack(fromUpdate, retrying_, metadataListener_, "waveform")) {
                return nullptr;
            }
        }
    }

    auto response = client.simpleRequest(
        beatlink::dbserver::Message::KnownType::WAVE_DETAIL_REQ,
        beatlink::dbserver::Message::KnownType::WAVE_DETAIL,
        {std::make_shared<beatlink::dbserver::NumberField>(
             client.buildRMST(beatlink::dbserver::Message::MenuIdentifier::DATA, slot.getSlot(), trackType)),
         idField,
         std::make_shared<beatlink::dbserver::NumberField>(beatlink::dbserver::NumberField::WORD_0)});

    if (response.knownType && *response.knownType != beatlink::dbserver::Message::KnownType::UNAVAILABLE &&
        response.arguments.size() > 3 && response.arguments[3]->getSize() > 0) {
        return std::make_shared<WaveformDetail>(DataReference(slot, rekordboxId, trackType), response, WaveformStyle::BLUE);
    }

    retryUnanalyzedTrack(fromUpdate, retrying_, metadataListener_, "waveform");
    return nullptr;
}

void WaveformFinder::clearDeckPreview(const TrackMetadataUpdate& update) {
    bool removed = false;
    {
        std::lock_guard<std::mutex> lock(previewCacheMutex_);
        removed = (previewHotCache_.erase(DeckReference(update.player, 0)) > 0);
    }
    if (removed) {
        deliverWaveformPreviewUpdate(update.player, nullptr);
    }
}

void WaveformFinder::clearDeckDetail(const TrackMetadataUpdate& update) {
    bool removed = false;
    {
        std::lock_guard<std::mutex> lock(detailCacheMutex_);
        removed = (detailHotCache_.erase(DeckReference(update.player, 0)) > 0);
    }
    if (removed) {
        deliverWaveformDetailUpdate(update.player, nullptr);
    }
}

void WaveformFinder::clearDeck(const TrackMetadataUpdate& update) {
    clearDeckPreview(update);
    clearDeckDetail(update);
}

void WaveformFinder::clearWaveforms(const beatlink::DeviceAnnouncement& announcement) {
    const int player = announcement.getDeviceNumber();
    std::vector<DeckReference> previewDecks;
    {
        std::lock_guard<std::mutex> lock(previewCacheMutex_);
        for (const auto& [deck, preview] : previewHotCache_) {
            if (deck.getPlayer() == player) {
                previewDecks.push_back(deck);
            }
        }
        for (const auto& deck : previewDecks) {
            previewHotCache_.erase(deck);
        }
    }

    if (std::any_of(previewDecks.begin(), previewDecks.end(), [](const DeckReference& deck) { return deck.getHotCue() == 0; })) {
        deliverWaveformPreviewUpdate(player, nullptr);
    }

    std::vector<DeckReference> detailDecks;
    {
        std::lock_guard<std::mutex> lock(detailCacheMutex_);
        for (const auto& [deck, detail] : detailHotCache_) {
            if (deck.getPlayer() == player) {
                detailDecks.push_back(deck);
            }
        }
        for (const auto& deck : detailDecks) {
            detailHotCache_.erase(deck);
        }
    }

    if (std::any_of(detailDecks.begin(), detailDecks.end(), [](const DeckReference& deck) { return deck.getHotCue() == 0; })) {
        deliverWaveformDetailUpdate(player, nullptr);
    }
}

void WaveformFinder::updatePreview(const TrackMetadataUpdate& update, const std::shared_ptr<WaveformPreview>& preview) {
    {
        std::lock_guard<std::mutex> lock(previewCacheMutex_);
        previewHotCache_[DeckReference(update.player, 0)] = preview;
        if (update.metadata && update.metadata->getCueList()) {
            for (const auto& entry : update.metadata->getCueList()->getEntries()) {
                if (entry.hotCueNumber != 0) {
                    previewHotCache_[DeckReference(update.player, entry.hotCueNumber)] = preview;
                }
            }
        }
    }
    deliverWaveformPreviewUpdate(update.player, preview);
}

void WaveformFinder::updateDetail(const TrackMetadataUpdate& update, const std::shared_ptr<WaveformDetail>& detail) {
    {
        std::lock_guard<std::mutex> lock(detailCacheMutex_);
        detailHotCache_[DeckReference(update.player, 0)] = detail;
        if (update.metadata && update.metadata->getCueList()) {
            for (const auto& entry : update.metadata->getCueList()->getEntries()) {
                if (entry.hotCueNumber != 0) {
                    detailHotCache_[DeckReference(update.player, entry.hotCueNumber)] = detail;
                }
            }
        }
    }
    deliverWaveformDetailUpdate(update.player, detail);
}

void WaveformFinder::deliverWaveformPreviewUpdate(int player,
                                                  const std::shared_ptr<WaveformPreview>& preview) {
    WaveformPreviewUpdate update(player, preview);
    auto listeners = getWaveformListeners();
    for (const auto& listener : listeners) {
        if (!listener) {
            continue;
        }
        try {
            listener->previewChanged(update);
        } catch (...) {
        }
    }
}

void WaveformFinder::deliverWaveformDetailUpdate(int player,
                                                 const std::shared_ptr<WaveformDetail>& detail) {
    WaveformDetailUpdate update(player, detail);
    auto listeners = getWaveformListeners();
    for (const auto& listener : listeners) {
        if (!listener) {
            continue;
        }
        try {
            listener->detailChanged(update);
        } catch (...) {
        }
    }
}

void WaveformFinder::handleUpdate(const TrackMetadataUpdate& update) {
    if (!update.metadata) {
        clearDeck(update);
        return;
    }

    bool foundInCache = false;
    std::shared_ptr<WaveformPreview> lastPreview;
    {
        std::lock_guard<std::mutex> lock(previewCacheMutex_);
        auto it = previewHotCache_.find(DeckReference(update.player, 0));
        if (it != previewHotCache_.end()) {
            lastPreview = it->second;
        }
    }

    if (!lastPreview || lastPreview->dataReference != update.metadata->getTrackReference() ||
        update.metadata->getTrackType() == beatlink::TrackType::UNANALYZED) {
        {
            std::lock_guard<std::mutex> lock(previewCacheMutex_);
            for (const auto& [deck, preview] : previewHotCache_) {
                if (deck.getHotCue() != 0 && preview && preview->dataReference == update.metadata->getTrackReference()) {
                    updatePreview(update, preview);
                    foundInCache = true;
                    break;
                }
            }
        }

        bool shouldRequest = false;
        {
            std::lock_guard<std::mutex> lock(activePreviewMutex_);
            shouldRequest = !foundInCache && activePreviewRequests_.insert(update.player).second;
        }

        if (shouldRequest) {
            clearDeckPreview(update);
            std::thread([this, update, lastPreview]() {
                try {
                    auto preview = requestPreviewInternal(update.metadata->getTrackReference(), &update);
                    if (preview) {
                        updatePreview(update, preview);
                        if (!lastPreview || *preview != *lastPreview) {
                            retryUnanalyzedTrack(&update, retrying_, metadataListener_, "waveform");
                        }
                    }
                } catch (const std::exception& e) {
                    std::cerr << "WaveformFinder: Problem requesting waveform preview: " << e.what() << std::endl;
                }
                std::lock_guard<std::mutex> lock(activePreviewMutex_);
                activePreviewRequests_.erase(update.player);
            }).detach();
        }
    }

    foundInCache = false;
    std::shared_ptr<WaveformDetail> lastDetail;
    {
        std::lock_guard<std::mutex> lock(detailCacheMutex_);
        auto it = detailHotCache_.find(DeckReference(update.player, 0));
        if (it != detailHotCache_.end()) {
            lastDetail = it->second;
        }
    }

    if (isFindingDetails() && (!lastDetail || lastDetail->dataReference != update.metadata->getTrackReference() ||
                               update.metadata->getTrackType() == beatlink::TrackType::UNANALYZED)) {
        {
            std::lock_guard<std::mutex> lock(detailCacheMutex_);
            for (const auto& [deck, detail] : detailHotCache_) {
                if (deck.getHotCue() != 0 && detail && detail->dataReference == update.metadata->getTrackReference()) {
                    updateDetail(update, detail);
                    foundInCache = true;
                    break;
                }
            }
        }

        bool shouldRequest = false;
        {
            std::lock_guard<std::mutex> lock(activeDetailMutex_);
            shouldRequest = !foundInCache && activeDetailRequests_.insert(update.player).second;
        }

        if (shouldRequest) {
            clearDeckDetail(update);
            std::thread([this, update, lastDetail]() {
                try {
                    auto detail = requestDetailInternal(update.metadata->getTrackReference(), &update);
                    if (detail) {
                        updateDetail(update, detail);
                        if (!lastDetail || *detail != *lastDetail) {
                            retryUnanalyzedTrack(&update, retrying_, metadataListener_, "waveform");
                        }
                    }
                } catch (const std::exception& e) {
                    std::cerr << "WaveformFinder: Problem requesting waveform detail: " << e.what() << std::endl;
                }
                std::lock_guard<std::mutex> lock(activeDetailMutex_);
                activeDetailRequests_.erase(update.player);
            }).detach();
        }
    }
}

void WaveformFinder::enqueueUpdate(const TrackMetadataUpdate& update) {
    std::unique_lock<std::mutex> lock(pendingMutex_);
    if (pendingUpdates_.size() >= kMaxPendingUpdates) {
        std::cerr << "WaveformFinder: Discarding metadata update because queue is backed up" << std::endl;
        return;
    }
    pendingUpdates_.push_back(update);
    lock.unlock();
    pendingCv_.notify_one();
}

void WaveformFinder::primeCache() {
    if (!isRunning()) {
        return;
    }
    auto loadedTracks = MetadataFinder::getInstance().getLoadedTracks();
    for (const auto& [deck, metadata] : loadedTracks) {
        if (deck.getHotCue() == 0) {
            handleUpdate(TrackMetadataUpdate(deck.getPlayer(), metadata));
        }
    }
}

void WaveformFinder::clearAllWaveforms() {
    std::vector<DeckReference> previewDecks;
    {
        std::lock_guard<std::mutex> lock(previewCacheMutex_);
        for (const auto& [deck, preview] : previewHotCache_) {
            if (deck.getHotCue() == 0) {
                previewDecks.push_back(deck);
            }
        }
        previewHotCache_.clear();
    }

    for (const auto& deck : previewDecks) {
        deliverWaveformPreviewUpdate(deck.getPlayer(), nullptr);
    }

    std::vector<DeckReference> detailDecks;
    {
        std::lock_guard<std::mutex> lock(detailCacheMutex_);
        for (const auto& [deck, detail] : detailHotCache_) {
            if (deck.getHotCue() == 0) {
                detailDecks.push_back(deck);
            }
        }
        detailHotCache_.clear();
    }

    for (const auto& deck : detailDecks) {
        deliverWaveformDetailUpdate(deck.getPlayer(), nullptr);
    }
}

void WaveformFinder::addWaveformListener(const WaveformListenerPtr& listener) {
    if (!listener) {
        return;
    }
    std::lock_guard<std::mutex> lock(listenersMutex_);
    if (std::find(waveformListeners_.begin(), waveformListeners_.end(), listener) == waveformListeners_.end()) {
        waveformListeners_.push_back(listener);
    }
}

void WaveformFinder::removeWaveformListener(const WaveformListenerPtr& listener) {
    if (!listener) {
        return;
    }
    std::lock_guard<std::mutex> lock(listenersMutex_);
    waveformListeners_.erase(std::remove(waveformListeners_.begin(), waveformListeners_.end(), listener),
                             waveformListeners_.end());
}

std::vector<WaveformListenerPtr> WaveformFinder::getWaveformListeners() const {
    std::lock_guard<std::mutex> lock(listenersMutex_);
    return waveformListeners_;
}

void WaveformFinder::start() {
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
                std::cerr << "WaveformFinder: Problem processing metadata update: " << e.what() << std::endl;
            }
        }
    });

    deliverLifecycleAnnouncement(true);
    primeCache();
}

void WaveformFinder::stop() {
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

    clearAllWaveforms();
    deliverLifecycleAnnouncement(false);
}

bool WaveformFinder::retryUnanalyzedTrack(const TrackMetadataUpdate* fromUpdate,
                                          std::atomic<bool>& retryFlag,
                                          const TrackMetadataListenerPtr& retryListener,
                                          const std::string& description) {
    (void)description;
    if (!fromUpdate || !fromUpdate->metadata || fromUpdate->metadata->getTrackType() != beatlink::TrackType::UNANALYZED) {
        return false;
    }

    auto currentMetadata = MetadataFinder::getInstance().getLatestMetadataFor(fromUpdate->player);
    if (!currentMetadata || *currentMetadata != *fromUpdate->metadata) {
        return false;
    }

    const int64_t nowNanos = std::chrono::duration_cast<std::chrono::nanoseconds>(
                                 std::chrono::steady_clock::now().time_since_epoch())
                                 .count();
    if (nowNanos - fromUpdate->metadata->getTimestampNanos() > MAXIMUM_ANALYSIS_WAIT_NANOS) {
        return false;
    }

    if (!retryFlag.exchange(true)) {
        TrackMetadataUpdate retryUpdate = *fromUpdate;
        std::thread([retryFlagPtr = &retryFlag, retryListener, retryUpdate]() {
            std::this_thread::sleep_for(std::chrono::milliseconds(ANALYSIS_UPDATE_INTERVAL_MS));
            retryFlagPtr->store(false);
            if (!retryListener) {
                return;
            }
            auto current = MetadataFinder::getInstance().getLatestMetadataFor(retryUpdate.player);
            if (current && retryUpdate.metadata && *current == *retryUpdate.metadata) {
                try {
                    retryListener->metadataChanged(retryUpdate);
                } catch (...) {
                }
            }
        }).detach();
        return true;
    }
    return true;
}

} // namespace beatlink::data
