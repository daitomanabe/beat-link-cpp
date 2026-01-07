#pragma once

#include <string>
#include <vector>

#include "SlotReference.hpp"
#include "beatlink/CdjStatus.hpp"
#include "beatlink/dbserver/Client.hpp"
#include "beatlink/dbserver/Message.hpp"

namespace beatlink::data {

class MenuLoader {
public:
    static MenuLoader& getInstance();

    std::vector<beatlink::dbserver::Message> requestRootMenuFrom(const SlotReference& slotReference, int sortOrder);
    std::vector<beatlink::dbserver::Message> requestPlaylistMenuFrom(const SlotReference& slotReference, int sortOrder);
    std::vector<beatlink::dbserver::Message> requestHistoryMenuFrom(const SlotReference& slotReference, int sortOrder);
    std::vector<beatlink::dbserver::Message> requestHistoryPlaylistFrom(const SlotReference& slotReference, int sortOrder, int historyId);
    std::vector<beatlink::dbserver::Message> requestTrackMenuFrom(const SlotReference& slotReference, int sortOrder);
    std::vector<beatlink::dbserver::Message> requestArtistMenuFrom(const SlotReference& slotReference, int sortOrder);
    std::vector<beatlink::dbserver::Message> requestArtistAlbumMenuFrom(const SlotReference& slotReference, int sortOrder, int artistId);
    std::vector<beatlink::dbserver::Message> requestArtistAlbumTrackMenuFrom(const SlotReference& slotReference, int sortOrder, int artistId, int albumId);
    std::vector<beatlink::dbserver::Message> requestOriginalArtistMenuFrom(const SlotReference& slotReference, int sortOrder);
    std::vector<beatlink::dbserver::Message> requestOriginalArtistAlbumMenuFrom(const SlotReference& slotReference, int sortOrder, int artistId);
    std::vector<beatlink::dbserver::Message> requestOriginalArtistAlbumTrackMenuFrom(const SlotReference& slotReference, int sortOrder, int artistId, int albumId);
    std::vector<beatlink::dbserver::Message> requestRemixerMenuFrom(const SlotReference& slotReference, int sortOrder);
    std::vector<beatlink::dbserver::Message> requestRemixerAlbumMenuFrom(const SlotReference& slotReference, int sortOrder, int artistId);
    std::vector<beatlink::dbserver::Message> requestRemixerAlbumTrackMenuFrom(const SlotReference& slotReference, int sortOrder, int artistId, int albumId);
    std::vector<beatlink::dbserver::Message> requestAlbumTrackMenuFrom(const SlotReference& slotReference, int sortOrder, int albumId);
    std::vector<beatlink::dbserver::Message> requestGenreMenuFrom(const SlotReference& slotReference, int sortOrder);
    std::vector<beatlink::dbserver::Message> requestGenreArtistMenuFrom(const SlotReference& slotReference, int sortOrder, int genreId);
    std::vector<beatlink::dbserver::Message> requestGenreArtistAlbumMenuFrom(const SlotReference& slotReference, int sortOrder, int genreId, int artistId);
    std::vector<beatlink::dbserver::Message> requestGenreArtistAlbumTrackMenuFrom(const SlotReference& slotReference, int sortOrder, int genreId, int artistId, int albumId);
    std::vector<beatlink::dbserver::Message> requestLabelMenuFrom(const SlotReference& slotReference, int sortOrder);
    std::vector<beatlink::dbserver::Message> requestLabelArtistMenuFrom(const SlotReference& slotReference, int sortOrder, int labelId);
    std::vector<beatlink::dbserver::Message> requestLabelArtistAlbumMenuFrom(const SlotReference& slotReference, int sortOrder, int labelId, int artistId);
    std::vector<beatlink::dbserver::Message> requestLabelArtistAlbumTrackMenuFrom(const SlotReference& slotReference, int sortOrder, int labelId, int artistId, int albumId);
    std::vector<beatlink::dbserver::Message> requestAlbumMenuFrom(const SlotReference& slotReference, int sortOrder);
    std::vector<beatlink::dbserver::Message> requestKeyMenuFrom(const SlotReference& slotReference, int sortOrder);
    std::vector<beatlink::dbserver::Message> requestKeyNeighborMenuFrom(const SlotReference& slotReference, int sortOrder, int keyId);
    std::vector<beatlink::dbserver::Message> requestTracksByKeyAndDistanceFrom(const SlotReference& slotReference, int sortOrder, int keyId, int distance);
    std::vector<beatlink::dbserver::Message> requestBpmMenuFrom(const SlotReference& slotReference, int sortOrder);
    std::vector<beatlink::dbserver::Message> requestBpmRangeMenuFrom(const SlotReference& slotReference, int sortOrder, int bpm);
    std::vector<beatlink::dbserver::Message> requestTracksByBpmRangeFrom(const SlotReference& slotReference, int sortOrder, int bpm, int range);
    std::vector<beatlink::dbserver::Message> requestRatingMenuFrom(const SlotReference& slotReference, int sortOrder);
    std::vector<beatlink::dbserver::Message> requestTracksByRatingFrom(const SlotReference& slotReference, int sortOrder, int rating);
    std::vector<beatlink::dbserver::Message> requestColorMenuFrom(const SlotReference& slotReference, int sortOrder);
    std::vector<beatlink::dbserver::Message> requestTracksByColorFrom(const SlotReference& slotReference, int sortOrder, int color);
    std::vector<beatlink::dbserver::Message> requestTimeMenuFrom(const SlotReference& slotReference, int sortOrder);
    std::vector<beatlink::dbserver::Message> requestTracksByTimeFrom(const SlotReference& slotReference, int sortOrder, int time);
    std::vector<beatlink::dbserver::Message> requestBitRateMenuFrom(const SlotReference& slotReference, int sortOrder);
    std::vector<beatlink::dbserver::Message> requestTracksByBitRateFrom(const SlotReference& slotReference, int sortOrder, int bitRate);
    std::vector<beatlink::dbserver::Message> requestYearMenuFrom(const SlotReference& slotReference, int sortOrder);
    std::vector<beatlink::dbserver::Message> requestYearsByDecadeFrom(const SlotReference& slotReference, int sortOrder, int decade);
    std::vector<beatlink::dbserver::Message> requestTracksByDecadeAndYear(const SlotReference& slotReference, int sortOrder, int decade, int year);
    std::vector<beatlink::dbserver::Message> requestFilenameMenuFrom(const SlotReference& slotReference, int sortOrder);
    std::vector<beatlink::dbserver::Message> requestFolderMenuFrom(const SlotReference& slotReference, int sortOrder, int folderId);

    std::vector<beatlink::dbserver::Message> requestSearchResultsFrom(int player,
                                                                      beatlink::TrackSourceSlot slot,
                                                                      int sortOrder,
                                                                      const std::string& text,
                                                                      int* count);

    std::vector<beatlink::dbserver::Message> requestMoreSearchResultsFrom(int player,
                                                                          beatlink::TrackSourceSlot slot,
                                                                          int sortOrder,
                                                                          const std::string& text,
                                                                          int offset,
                                                                          int count);

private:
    MenuLoader() = default;

    std::vector<beatlink::dbserver::Message> requestMenuItems(const SlotReference& slotReference,
                                                              beatlink::dbserver::Message::KnownType requestType,
                                                              beatlink::TrackType trackType,
                                                              const std::vector<beatlink::dbserver::FieldPtr>& fields,
                                                              const std::string& description,
                                                              bool typedRequest);

    std::vector<beatlink::dbserver::Message> getSearchItems(beatlink::TrackSourceSlot slot,
                                                            int sortOrder,
                                                            const std::string& text,
                                                            int* count,
                                                            beatlink::dbserver::Client& client);

    std::vector<beatlink::dbserver::Message> getMoreSearchItems(beatlink::TrackSourceSlot slot,
                                                                int sortOrder,
                                                                const std::string& text,
                                                                int offset,
                                                                int count,
                                                                beatlink::dbserver::Client& client);
};

} // namespace beatlink::data
