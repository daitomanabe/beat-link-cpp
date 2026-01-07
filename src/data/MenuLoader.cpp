#include "beatlink/data/MenuLoader.hpp"

#include "beatlink/data/MetadataFinder.hpp"
#include "beatlink/dbserver/Client.hpp"
#include "beatlink/dbserver/ConnectionManager.hpp"
#include "beatlink/dbserver/NumberField.hpp"
#include "beatlink/dbserver/StringField.hpp"

#include <algorithm>
#include <chrono>
#include <cctype>
#include <stdexcept>

namespace beatlink::data {

namespace {
class MenuLockGuard {
public:
    MenuLockGuard(beatlink::dbserver::Client& client, std::chrono::milliseconds timeout)
        : client_(client)
        , locked_(client_.tryLockingForMenuOperations(timeout))
    {
        if (!locked_) {
            throw std::runtime_error("Unable to lock player for menu operations.");
        }
    }

    ~MenuLockGuard() {
        if (locked_) {
            client_.unlockForMenuOperations();
        }
    }

private:
    beatlink::dbserver::Client& client_;
    bool locked_{false};
};

constexpr auto kMenuTimeout = std::chrono::seconds(MetadataFinder::MENU_TIMEOUT_SECONDS);
} // namespace

MenuLoader& MenuLoader::getInstance() {
    static MenuLoader instance;
    return instance;
}

std::vector<beatlink::dbserver::Message> MenuLoader::requestMenuItems(
    const SlotReference& slotReference,
    beatlink::dbserver::Message::KnownType requestType,
    beatlink::TrackType trackType,
    const std::vector<beatlink::dbserver::FieldPtr>& fields,
    const std::string& description,
    bool typedRequest) {
    return beatlink::dbserver::ConnectionManager::getInstance().invokeWithClientSession(
        slotReference.getPlayer(),
        [=](beatlink::dbserver::Client& client) {
            MenuLockGuard guard(client, kMenuTimeout);
            beatlink::dbserver::Message response = typedRequest
                ? client.menuRequestTyped(requestType, beatlink::dbserver::Message::MenuIdentifier::MAIN_MENU,
                                          slotReference.getSlot(), trackType, fields)
                : client.menuRequest(requestType, beatlink::dbserver::Message::MenuIdentifier::MAIN_MENU,
                                     slotReference.getSlot(), fields);
            const auto count = response.getMenuResultsCount();
            if (count == beatlink::dbserver::Message::NO_MENU_RESULTS_AVAILABLE || count == 0) {
                return std::vector<beatlink::dbserver::Message>{};
            }
            return client.renderMenuItems(beatlink::dbserver::Message::MenuIdentifier::MAIN_MENU,
                                          slotReference.getSlot(),
                                          trackType,
                                          response);
        },
        description);
}

std::vector<beatlink::dbserver::Message> MenuLoader::requestRootMenuFrom(const SlotReference& slotReference,
                                                                         int sortOrder) {
    return beatlink::dbserver::ConnectionManager::getInstance().invokeWithClientSession(
        slotReference.getPlayer(),
        [=](beatlink::dbserver::Client& client) {
            MenuLockGuard guard(client, kMenuTimeout);
            auto details = MetadataFinder::getInstance().getMediaDetailsFor(slotReference);
            const auto trackType = details ? details->getMediaType() : beatlink::TrackType::REKORDBOX;
            auto response = client.menuRequestTyped(
                beatlink::dbserver::Message::KnownType::ROOT_MENU_REQ,
                beatlink::dbserver::Message::MenuIdentifier::MAIN_MENU,
                slotReference.getSlot(),
                trackType,
                {std::make_shared<beatlink::dbserver::NumberField>(sortOrder),
                 std::make_shared<beatlink::dbserver::NumberField>(0xffffff)});
            return client.renderMenuItems(beatlink::dbserver::Message::MenuIdentifier::MAIN_MENU,
                                          slotReference.getSlot(),
                                          trackType,
                                          response);
        },
        "requesting root menu");
}

std::vector<beatlink::dbserver::Message> MenuLoader::requestPlaylistMenuFrom(const SlotReference& slotReference,
                                                                             int sortOrder) {
    return MetadataFinder::getInstance().requestPlaylistItemsFrom(slotReference.getPlayer(),
                                                                  slotReference.getSlot(),
                                                                  sortOrder,
                                                                  0,
                                                                  true);
}

std::vector<beatlink::dbserver::Message> MenuLoader::requestHistoryMenuFrom(const SlotReference& slotReference,
                                                                            int sortOrder) {
    return requestMenuItems(slotReference,
                            beatlink::dbserver::Message::KnownType::HISTORY_MENU_REQ,
                            beatlink::TrackType::REKORDBOX,
                            {std::make_shared<beatlink::dbserver::NumberField>(sortOrder)},
                            "requesting history menu",
                            false);
}

std::vector<beatlink::dbserver::Message> MenuLoader::requestHistoryPlaylistFrom(const SlotReference& slotReference,
                                                                                int sortOrder,
                                                                                int historyId) {
    return requestMenuItems(slotReference,
                            beatlink::dbserver::Message::KnownType::TRACK_MENU_FOR_HISTORY_REQ,
                            beatlink::TrackType::REKORDBOX,
                            {std::make_shared<beatlink::dbserver::NumberField>(sortOrder),
                             std::make_shared<beatlink::dbserver::NumberField>(historyId)},
                            "requesting history playlist",
                            false);
}

std::vector<beatlink::dbserver::Message> MenuLoader::requestTrackMenuFrom(const SlotReference& slotReference,
                                                                          int sortOrder) {
    return requestMenuItems(slotReference,
                            beatlink::dbserver::Message::KnownType::TRACK_MENU_REQ,
                            beatlink::TrackType::REKORDBOX,
                            {std::make_shared<beatlink::dbserver::NumberField>(sortOrder)},
                            "requesting track menu",
                            false);
}

std::vector<beatlink::dbserver::Message> MenuLoader::requestArtistMenuFrom(const SlotReference& slotReference,
                                                                           int sortOrder) {
    return requestMenuItems(slotReference,
                            beatlink::dbserver::Message::KnownType::ARTIST_MENU_REQ,
                            beatlink::TrackType::REKORDBOX,
                            {std::make_shared<beatlink::dbserver::NumberField>(sortOrder)},
                            "requesting artist menu",
                            false);
}

std::vector<beatlink::dbserver::Message> MenuLoader::requestArtistAlbumMenuFrom(const SlotReference& slotReference,
                                                                                int sortOrder,
                                                                                int artistId) {
    return requestMenuItems(slotReference,
                            beatlink::dbserver::Message::KnownType::ALBUM_MENU_FOR_ARTIST_REQ,
                            beatlink::TrackType::REKORDBOX,
                            {std::make_shared<beatlink::dbserver::NumberField>(sortOrder),
                             std::make_shared<beatlink::dbserver::NumberField>(artistId)},
                            "requesting artist album menu",
                            false);
}

std::vector<beatlink::dbserver::Message> MenuLoader::requestArtistAlbumTrackMenuFrom(const SlotReference& slotReference,
                                                                                     int sortOrder,
                                                                                     int artistId,
                                                                                     int albumId) {
    return requestMenuItems(slotReference,
                            beatlink::dbserver::Message::KnownType::TRACK_MENU_FOR_ARTIST_AND_ALBUM,
                            beatlink::TrackType::REKORDBOX,
                            {std::make_shared<beatlink::dbserver::NumberField>(sortOrder),
                             std::make_shared<beatlink::dbserver::NumberField>(artistId),
                             std::make_shared<beatlink::dbserver::NumberField>(albumId)},
                            "requesting artist album track menu",
                            false);
}

std::vector<beatlink::dbserver::Message> MenuLoader::requestOriginalArtistMenuFrom(const SlotReference& slotReference,
                                                                                   int sortOrder) {
    return requestMenuItems(slotReference,
                            beatlink::dbserver::Message::KnownType::ORIGINAL_ARTIST_MENU_REQ,
                            beatlink::TrackType::REKORDBOX,
                            {std::make_shared<beatlink::dbserver::NumberField>(sortOrder)},
                            "requesting original artist menu",
                            false);
}

std::vector<beatlink::dbserver::Message> MenuLoader::requestOriginalArtistAlbumMenuFrom(const SlotReference& slotReference,
                                                                                        int sortOrder,
                                                                                        int artistId) {
    return requestMenuItems(slotReference,
                            beatlink::dbserver::Message::KnownType::ALBUM_MENU_FOR_ORIGINAL_ARTIST_REQ,
                            beatlink::TrackType::REKORDBOX,
                            {std::make_shared<beatlink::dbserver::NumberField>(sortOrder),
                             std::make_shared<beatlink::dbserver::NumberField>(artistId)},
                            "requesting original artist album menu",
                            false);
}

std::vector<beatlink::dbserver::Message> MenuLoader::requestOriginalArtistAlbumTrackMenuFrom(const SlotReference& slotReference,
                                                                                             int sortOrder,
                                                                                             int artistId,
                                                                                             int albumId) {
    return requestMenuItems(slotReference,
                            beatlink::dbserver::Message::KnownType::TRACK_MENU_FOR_ORIGINAL_ARTIST_AND_ALBUM,
                            beatlink::TrackType::REKORDBOX,
                            {std::make_shared<beatlink::dbserver::NumberField>(sortOrder),
                             std::make_shared<beatlink::dbserver::NumberField>(artistId),
                             std::make_shared<beatlink::dbserver::NumberField>(albumId)},
                            "requesting original artist album track menu",
                            false);
}

std::vector<beatlink::dbserver::Message> MenuLoader::requestRemixerMenuFrom(const SlotReference& slotReference,
                                                                            int sortOrder) {
    return requestMenuItems(slotReference,
                            beatlink::dbserver::Message::KnownType::REMIXER_MENU_REQ,
                            beatlink::TrackType::REKORDBOX,
                            {std::make_shared<beatlink::dbserver::NumberField>(sortOrder)},
                            "requesting remixer menu",
                            false);
}

std::vector<beatlink::dbserver::Message> MenuLoader::requestRemixerAlbumMenuFrom(const SlotReference& slotReference,
                                                                                 int sortOrder,
                                                                                 int artistId) {
    return requestMenuItems(slotReference,
                            beatlink::dbserver::Message::KnownType::ALBUM_MENU_FOR_REMIXER_REQ,
                            beatlink::TrackType::REKORDBOX,
                            {std::make_shared<beatlink::dbserver::NumberField>(sortOrder),
                             std::make_shared<beatlink::dbserver::NumberField>(artistId)},
                            "requesting remixer album menu",
                            false);
}

std::vector<beatlink::dbserver::Message> MenuLoader::requestRemixerAlbumTrackMenuFrom(const SlotReference& slotReference,
                                                                                      int sortOrder,
                                                                                      int artistId,
                                                                                      int albumId) {
    return requestMenuItems(slotReference,
                            beatlink::dbserver::Message::KnownType::TRACK_MENU_FOR_REMIXER_AND_ALBUM,
                            beatlink::TrackType::REKORDBOX,
                            {std::make_shared<beatlink::dbserver::NumberField>(sortOrder),
                             std::make_shared<beatlink::dbserver::NumberField>(artistId),
                             std::make_shared<beatlink::dbserver::NumberField>(albumId)},
                            "requesting remixer album track menu",
                            false);
}

std::vector<beatlink::dbserver::Message> MenuLoader::requestAlbumTrackMenuFrom(const SlotReference& slotReference,
                                                                               int sortOrder,
                                                                               int albumId) {
    return requestMenuItems(slotReference,
                            beatlink::dbserver::Message::KnownType::TRACK_MENU_FOR_ALBUM_REQ,
                            beatlink::TrackType::REKORDBOX,
                            {std::make_shared<beatlink::dbserver::NumberField>(sortOrder),
                             std::make_shared<beatlink::dbserver::NumberField>(albumId)},
                            "requesting album track menu",
                            false);
}

std::vector<beatlink::dbserver::Message> MenuLoader::requestGenreMenuFrom(const SlotReference& slotReference,
                                                                          int sortOrder) {
    return requestMenuItems(slotReference,
                            beatlink::dbserver::Message::KnownType::GENRE_MENU_REQ,
                            beatlink::TrackType::REKORDBOX,
                            {std::make_shared<beatlink::dbserver::NumberField>(sortOrder)},
                            "requesting genre menu",
                            false);
}

std::vector<beatlink::dbserver::Message> MenuLoader::requestGenreArtistMenuFrom(const SlotReference& slotReference,
                                                                                int sortOrder,
                                                                                int genreId) {
    return requestMenuItems(slotReference,
                            beatlink::dbserver::Message::KnownType::ARTIST_MENU_FOR_GENRE_REQ,
                            beatlink::TrackType::REKORDBOX,
                            {std::make_shared<beatlink::dbserver::NumberField>(sortOrder),
                             std::make_shared<beatlink::dbserver::NumberField>(genreId)},
                            "requesting genre artist menu",
                            false);
}

std::vector<beatlink::dbserver::Message> MenuLoader::requestGenreArtistAlbumMenuFrom(const SlotReference& slotReference,
                                                                                     int sortOrder,
                                                                                     int genreId,
                                                                                     int artistId) {
    return requestMenuItems(slotReference,
                            beatlink::dbserver::Message::KnownType::ALBUM_MENU_FOR_GENRE_AND_ARTIST,
                            beatlink::TrackType::REKORDBOX,
                            {std::make_shared<beatlink::dbserver::NumberField>(sortOrder),
                             std::make_shared<beatlink::dbserver::NumberField>(genreId),
                             std::make_shared<beatlink::dbserver::NumberField>(artistId)},
                            "requesting genre artist album menu",
                            false);
}

std::vector<beatlink::dbserver::Message> MenuLoader::requestGenreArtistAlbumTrackMenuFrom(const SlotReference& slotReference,
                                                                                          int sortOrder,
                                                                                          int genreId,
                                                                                          int artistId,
                                                                                          int albumId) {
    return requestMenuItems(slotReference,
                            beatlink::dbserver::Message::KnownType::TRACK_MENU_FOR_GENRE_ARTIST_AND_ALBUM,
                            beatlink::TrackType::REKORDBOX,
                            {std::make_shared<beatlink::dbserver::NumberField>(sortOrder),
                             std::make_shared<beatlink::dbserver::NumberField>(genreId),
                             std::make_shared<beatlink::dbserver::NumberField>(artistId),
                             std::make_shared<beatlink::dbserver::NumberField>(albumId)},
                            "requesting genre artist album track menu",
                            false);
}

std::vector<beatlink::dbserver::Message> MenuLoader::requestLabelMenuFrom(const SlotReference& slotReference,
                                                                          int sortOrder) {
    return requestMenuItems(slotReference,
                            beatlink::dbserver::Message::KnownType::LABEL_MENU_REQ,
                            beatlink::TrackType::REKORDBOX,
                            {std::make_shared<beatlink::dbserver::NumberField>(sortOrder)},
                            "requesting label menu",
                            false);
}

std::vector<beatlink::dbserver::Message> MenuLoader::requestLabelArtistMenuFrom(const SlotReference& slotReference,
                                                                                int sortOrder,
                                                                                int labelId) {
    return requestMenuItems(slotReference,
                            beatlink::dbserver::Message::KnownType::ARTIST_MENU_FOR_LABEL_REQ,
                            beatlink::TrackType::REKORDBOX,
                            {std::make_shared<beatlink::dbserver::NumberField>(sortOrder),
                             std::make_shared<beatlink::dbserver::NumberField>(labelId)},
                            "requesting label artist menu",
                            false);
}

std::vector<beatlink::dbserver::Message> MenuLoader::requestLabelArtistAlbumMenuFrom(const SlotReference& slotReference,
                                                                                     int sortOrder,
                                                                                     int labelId,
                                                                                     int artistId) {
    return requestMenuItems(slotReference,
                            beatlink::dbserver::Message::KnownType::ALBUM_MENU_FOR_LABEL_AND_ARTIST,
                            beatlink::TrackType::REKORDBOX,
                            {std::make_shared<beatlink::dbserver::NumberField>(sortOrder),
                             std::make_shared<beatlink::dbserver::NumberField>(labelId),
                             std::make_shared<beatlink::dbserver::NumberField>(artistId)},
                            "requesting label artist album menu",
                            false);
}

std::vector<beatlink::dbserver::Message> MenuLoader::requestLabelArtistAlbumTrackMenuFrom(const SlotReference& slotReference,
                                                                                          int sortOrder,
                                                                                          int labelId,
                                                                                          int artistId,
                                                                                          int albumId) {
    return requestMenuItems(slotReference,
                            beatlink::dbserver::Message::KnownType::TRACK_MENU_FOR_LABEL_ARTIST_AND_ALBUM,
                            beatlink::TrackType::REKORDBOX,
                            {std::make_shared<beatlink::dbserver::NumberField>(sortOrder),
                             std::make_shared<beatlink::dbserver::NumberField>(labelId),
                             std::make_shared<beatlink::dbserver::NumberField>(artistId),
                             std::make_shared<beatlink::dbserver::NumberField>(albumId)},
                            "requesting label artist album track menu",
                            false);
}

std::vector<beatlink::dbserver::Message> MenuLoader::requestAlbumMenuFrom(const SlotReference& slotReference,
                                                                          int sortOrder) {
    return requestMenuItems(slotReference,
                            beatlink::dbserver::Message::KnownType::ALBUM_MENU_REQ,
                            beatlink::TrackType::REKORDBOX,
                            {std::make_shared<beatlink::dbserver::NumberField>(sortOrder)},
                            "requesting album menu",
                            false);
}

std::vector<beatlink::dbserver::Message> MenuLoader::requestKeyMenuFrom(const SlotReference& slotReference,
                                                                        int sortOrder) {
    return requestMenuItems(slotReference,
                            beatlink::dbserver::Message::KnownType::KEY_MENU_REQ,
                            beatlink::TrackType::REKORDBOX,
                            {std::make_shared<beatlink::dbserver::NumberField>(sortOrder)},
                            "requesting key menu",
                            false);
}

std::vector<beatlink::dbserver::Message> MenuLoader::requestKeyNeighborMenuFrom(const SlotReference& slotReference,
                                                                                int sortOrder,
                                                                                int keyId) {
    return requestMenuItems(slotReference,
                            beatlink::dbserver::Message::KnownType::NEIGHBOR_MENU_FOR_KEY,
                            beatlink::TrackType::REKORDBOX,
                            {std::make_shared<beatlink::dbserver::NumberField>(sortOrder),
                             std::make_shared<beatlink::dbserver::NumberField>(keyId)},
                            "requesting key neighbor menu",
                            false);
}

std::vector<beatlink::dbserver::Message> MenuLoader::requestTracksByKeyAndDistanceFrom(const SlotReference& slotReference,
                                                                                       int sortOrder,
                                                                                       int keyId,
                                                                                       int distance) {
    return requestMenuItems(slotReference,
                            beatlink::dbserver::Message::KnownType::TRACK_MENU_FOR_KEY_AND_DISTANCE,
                            beatlink::TrackType::REKORDBOX,
                            {std::make_shared<beatlink::dbserver::NumberField>(sortOrder),
                             std::make_shared<beatlink::dbserver::NumberField>(keyId),
                             std::make_shared<beatlink::dbserver::NumberField>(distance)},
                            "requesting tracks by key and distance",
                            false);
}

std::vector<beatlink::dbserver::Message> MenuLoader::requestBpmMenuFrom(const SlotReference& slotReference,
                                                                        int sortOrder) {
    return requestMenuItems(slotReference,
                            beatlink::dbserver::Message::KnownType::BPM_MENU_REQ,
                            beatlink::TrackType::REKORDBOX,
                            {std::make_shared<beatlink::dbserver::NumberField>(sortOrder)},
                            "requesting BPM menu",
                            false);
}

std::vector<beatlink::dbserver::Message> MenuLoader::requestBpmRangeMenuFrom(const SlotReference& slotReference,
                                                                             int sortOrder,
                                                                             int bpm) {
    return requestMenuItems(slotReference,
                            beatlink::dbserver::Message::KnownType::BPM_RANGE_REQ,
                            beatlink::TrackType::REKORDBOX,
                            {std::make_shared<beatlink::dbserver::NumberField>(sortOrder),
                             std::make_shared<beatlink::dbserver::NumberField>(bpm)},
                            "requesting BPM range menu",
                            false);
}

std::vector<beatlink::dbserver::Message> MenuLoader::requestTracksByBpmRangeFrom(const SlotReference& slotReference,
                                                                                 int sortOrder,
                                                                                 int bpm,
                                                                                 int range) {
    return requestMenuItems(slotReference,
                            beatlink::dbserver::Message::KnownType::TRACK_MENU_FOR_BPM_AND_DISTANCE,
                            beatlink::TrackType::REKORDBOX,
                            {std::make_shared<beatlink::dbserver::NumberField>(sortOrder),
                             std::make_shared<beatlink::dbserver::NumberField>(bpm),
                             std::make_shared<beatlink::dbserver::NumberField>(range)},
                            "requesting tracks by BPM range",
                            false);
}

std::vector<beatlink::dbserver::Message> MenuLoader::requestRatingMenuFrom(const SlotReference& slotReference,
                                                                           int sortOrder) {
    return requestMenuItems(slotReference,
                            beatlink::dbserver::Message::KnownType::RATING_MENU_REQ,
                            beatlink::TrackType::REKORDBOX,
                            {std::make_shared<beatlink::dbserver::NumberField>(sortOrder)},
                            "requesting rating menu",
                            false);
}

std::vector<beatlink::dbserver::Message> MenuLoader::requestTracksByRatingFrom(const SlotReference& slotReference,
                                                                               int sortOrder,
                                                                               int rating) {
    return requestMenuItems(slotReference,
                            beatlink::dbserver::Message::KnownType::TRACK_MENU_FOR_RATING_REQ,
                            beatlink::TrackType::REKORDBOX,
                            {std::make_shared<beatlink::dbserver::NumberField>(sortOrder),
                             std::make_shared<beatlink::dbserver::NumberField>(rating)},
                            "requesting tracks by rating",
                            false);
}

std::vector<beatlink::dbserver::Message> MenuLoader::requestColorMenuFrom(const SlotReference& slotReference,
                                                                          int sortOrder) {
    return requestMenuItems(slotReference,
                            beatlink::dbserver::Message::KnownType::COLOR_MENU_REQ,
                            beatlink::TrackType::REKORDBOX,
                            {std::make_shared<beatlink::dbserver::NumberField>(sortOrder)},
                            "requesting color menu",
                            false);
}

std::vector<beatlink::dbserver::Message> MenuLoader::requestTracksByColorFrom(const SlotReference& slotReference,
                                                                              int sortOrder,
                                                                              int color) {
    return requestMenuItems(slotReference,
                            beatlink::dbserver::Message::KnownType::TRACK_MENU_FOR_COLOR_REQ,
                            beatlink::TrackType::REKORDBOX,
                            {std::make_shared<beatlink::dbserver::NumberField>(sortOrder),
                             std::make_shared<beatlink::dbserver::NumberField>(color)},
                            "requesting tracks by color",
                            false);
}

std::vector<beatlink::dbserver::Message> MenuLoader::requestTimeMenuFrom(const SlotReference& slotReference,
                                                                         int sortOrder) {
    return requestMenuItems(slotReference,
                            beatlink::dbserver::Message::KnownType::TIME_MENU_REQ,
                            beatlink::TrackType::REKORDBOX,
                            {std::make_shared<beatlink::dbserver::NumberField>(sortOrder)},
                            "requesting time menu",
                            false);
}

std::vector<beatlink::dbserver::Message> MenuLoader::requestTracksByTimeFrom(const SlotReference& slotReference,
                                                                             int sortOrder,
                                                                             int time) {
    return requestMenuItems(slotReference,
                            beatlink::dbserver::Message::KnownType::TRACK_MENU_FOR_TIME_REQ,
                            beatlink::TrackType::REKORDBOX,
                            {std::make_shared<beatlink::dbserver::NumberField>(sortOrder),
                             std::make_shared<beatlink::dbserver::NumberField>(time)},
                            "requesting tracks by time",
                            false);
}

std::vector<beatlink::dbserver::Message> MenuLoader::requestBitRateMenuFrom(const SlotReference& slotReference,
                                                                            int sortOrder) {
    return requestMenuItems(slotReference,
                            beatlink::dbserver::Message::KnownType::BIT_RATE_MENU_REQ,
                            beatlink::TrackType::REKORDBOX,
                            {std::make_shared<beatlink::dbserver::NumberField>(sortOrder)},
                            "requesting bit rate menu",
                            false);
}

std::vector<beatlink::dbserver::Message> MenuLoader::requestTracksByBitRateFrom(const SlotReference& slotReference,
                                                                                int sortOrder,
                                                                                int bitRate) {
    return requestMenuItems(slotReference,
                            beatlink::dbserver::Message::KnownType::TRACK_MENU_FOR_BIT_RATE_REQ,
                            beatlink::TrackType::REKORDBOX,
                            {std::make_shared<beatlink::dbserver::NumberField>(sortOrder),
                             std::make_shared<beatlink::dbserver::NumberField>(bitRate)},
                            "requesting tracks by bit rate",
                            false);
}

std::vector<beatlink::dbserver::Message> MenuLoader::requestYearMenuFrom(const SlotReference& slotReference,
                                                                         int sortOrder) {
    return requestMenuItems(slotReference,
                            beatlink::dbserver::Message::KnownType::YEAR_MENU_REQ,
                            beatlink::TrackType::REKORDBOX,
                            {std::make_shared<beatlink::dbserver::NumberField>(sortOrder)},
                            "requesting year menu",
                            false);
}

std::vector<beatlink::dbserver::Message> MenuLoader::requestYearsByDecadeFrom(const SlotReference& slotReference,
                                                                              int sortOrder,
                                                                              int decade) {
    return requestMenuItems(slotReference,
                            beatlink::dbserver::Message::KnownType::YEAR_MENU_FOR_DECADE_REQ,
                            beatlink::TrackType::REKORDBOX,
                            {std::make_shared<beatlink::dbserver::NumberField>(sortOrder),
                             std::make_shared<beatlink::dbserver::NumberField>(decade)},
                            "requesting decade menu",
                            false);
}

std::vector<beatlink::dbserver::Message> MenuLoader::requestTracksByDecadeAndYear(const SlotReference& slotReference,
                                                                                  int sortOrder,
                                                                                  int decade,
                                                                                  int year) {
    return requestMenuItems(slotReference,
                            beatlink::dbserver::Message::KnownType::TRACK_MENU_FOR_DECADE_YEAR_REQ,
                            beatlink::TrackType::REKORDBOX,
                            {std::make_shared<beatlink::dbserver::NumberField>(sortOrder),
                             std::make_shared<beatlink::dbserver::NumberField>(decade),
                             std::make_shared<beatlink::dbserver::NumberField>(year)},
                            "requesting tracks by decade and year",
                            false);
}

std::vector<beatlink::dbserver::Message> MenuLoader::requestFilenameMenuFrom(const SlotReference& slotReference,
                                                                             int sortOrder) {
    return requestMenuItems(slotReference,
                            beatlink::dbserver::Message::KnownType::FILENAME_MENU_REQ,
                            beatlink::TrackType::REKORDBOX,
                            {std::make_shared<beatlink::dbserver::NumberField>(sortOrder)},
                            "requesting filename menu",
                            false);
}

std::vector<beatlink::dbserver::Message> MenuLoader::requestFolderMenuFrom(const SlotReference& slotReference,
                                                                           int sortOrder,
                                                                           int folderId) {
    return requestMenuItems(slotReference,
                            beatlink::dbserver::Message::KnownType::FOLDER_MENU_REQ,
                            beatlink::TrackType::UNANALYZED,
                            {std::make_shared<beatlink::dbserver::NumberField>(sortOrder),
                             std::make_shared<beatlink::dbserver::NumberField>(folderId),
                             std::make_shared<beatlink::dbserver::NumberField>(0xffffff)},
                            "requesting folder menu",
                            true);
}

std::vector<beatlink::dbserver::Message> MenuLoader::requestSearchResultsFrom(int player,
                                                                              beatlink::TrackSourceSlot slot,
                                                                              int sortOrder,
                                                                              const std::string& text,
                                                                              int* count) {
    return beatlink::dbserver::ConnectionManager::getInstance().invokeWithClientSession(
        player,
        [this, slot, sortOrder, text, count](beatlink::dbserver::Client& client) {
            return getSearchItems(slot, sortOrder, text, count, client);
        },
        "performing search");
}

std::vector<beatlink::dbserver::Message> MenuLoader::requestMoreSearchResultsFrom(int player,
                                                                                  beatlink::TrackSourceSlot slot,
                                                                                  int sortOrder,
                                                                                  const std::string& text,
                                                                                  int offset,
                                                                                  int count) {
    return beatlink::dbserver::ConnectionManager::getInstance().invokeWithClientSession(
        player,
        [this, slot, sortOrder, text, offset, count](beatlink::dbserver::Client& client) {
            return getMoreSearchItems(slot, sortOrder, text, offset, count, client);
        },
        "performing search");
}

std::vector<beatlink::dbserver::Message> MenuLoader::getSearchItems(beatlink::TrackSourceSlot slot,
                                                                    int sortOrder,
                                                                    const std::string& text,
                                                                    int* count,
                                                                    beatlink::dbserver::Client& client) {
    MenuLockGuard guard(client, kMenuTimeout);
    std::string query = text.empty() ? std::string{} : text;
    std::transform(query.begin(), query.end(), query.begin(),
                   [](unsigned char ch) { return static_cast<char>(std::toupper(ch)); });
    auto response = client.menuRequest(beatlink::dbserver::Message::KnownType::SEARCH_MENU,
                                       beatlink::dbserver::Message::MenuIdentifier::MAIN_MENU,
                                       slot,
                                       {std::make_shared<beatlink::dbserver::NumberField>(sortOrder),
                                        std::make_shared<beatlink::dbserver::NumberField>(query.size()),
                                        std::make_shared<beatlink::dbserver::StringField>(query),
                                        std::make_shared<beatlink::dbserver::NumberField>(0)});

    const auto actualCount = static_cast<int>(response.getMenuResultsCount());
    if (actualCount <= 0 || actualCount == static_cast<int>(beatlink::dbserver::Message::NO_MENU_RESULTS_AVAILABLE)) {
        if (count) {
            *count = 0;
        }
        return {};
    }

    if (!count) {
        return client.renderMenuItems(beatlink::dbserver::Message::MenuIdentifier::MAIN_MENU,
                                      slot,
                                      beatlink::TrackType::REKORDBOX,
                                      response);
    }

    const int desiredCount = std::min(*count, actualCount);
    *count = actualCount;
    if (desiredCount < 1) {
        return {};
    }
    return client.renderMenuItems(beatlink::dbserver::Message::MenuIdentifier::MAIN_MENU,
                                  slot,
                                  beatlink::TrackType::REKORDBOX,
                                  0,
                                  desiredCount);
}

std::vector<beatlink::dbserver::Message> MenuLoader::getMoreSearchItems(beatlink::TrackSourceSlot slot,
                                                                        int sortOrder,
                                                                        const std::string& text,
                                                                        int offset,
                                                                        int count,
                                                                        beatlink::dbserver::Client& client) {
    MenuLockGuard guard(client, kMenuTimeout);
    std::string query = text;
    std::transform(query.begin(), query.end(), query.begin(),
                   [](unsigned char ch) { return static_cast<char>(std::toupper(ch)); });
    auto response = client.menuRequest(beatlink::dbserver::Message::KnownType::SEARCH_MENU,
                                       beatlink::dbserver::Message::MenuIdentifier::MAIN_MENU,
                                       slot,
                                       {std::make_shared<beatlink::dbserver::NumberField>(sortOrder),
                                        std::make_shared<beatlink::dbserver::NumberField>(query.size()),
                                        std::make_shared<beatlink::dbserver::StringField>(query),
                                        std::make_shared<beatlink::dbserver::NumberField>(0)});
    const int actualCount = static_cast<int>(response.getMenuResultsCount());
    if (offset + count > actualCount) {
        throw std::invalid_argument("Cannot request items past the end of the menu.");
    }

    return client.renderMenuItems(beatlink::dbserver::Message::MenuIdentifier::MAIN_MENU,
                                  slot,
                                  beatlink::TrackType::REKORDBOX,
                                  offset,
                                  count);
}

} // namespace beatlink::data
