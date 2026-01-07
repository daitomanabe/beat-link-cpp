#pragma once

#include <memory>
#include <optional>
#include <vector>

#include "AlbumArt.hpp"
#include "AnalysisTag.hpp"
#include "BeatGrid.hpp"
#include "CueList.hpp"
#include "DataReference.hpp"
#include "TrackMetadata.hpp"
#include "WaveformDetail.hpp"
#include "WaveformPreview.hpp"
#include "beatlink/MediaDetails.hpp"

namespace beatlink::data {

class MetadataProvider {
public:
    virtual ~MetadataProvider() = default;

    virtual std::vector<beatlink::MediaDetails> supportedMedia() const = 0;

    virtual std::shared_ptr<TrackMetadata> getTrackMetadata(const beatlink::MediaDetails& sourceMedia,
                                                            const DataReference& track) = 0;

    virtual std::shared_ptr<AlbumArt> getAlbumArt(const beatlink::MediaDetails& sourceMedia,
                                                  const DataReference& art) = 0;

    virtual std::shared_ptr<BeatGrid> getBeatGrid(const beatlink::MediaDetails& sourceMedia,
                                                  const DataReference& track) = 0;

    virtual std::shared_ptr<CueList> getCueList(const beatlink::MediaDetails& sourceMedia,
                                                const DataReference& track) = 0;

    virtual std::shared_ptr<WaveformPreview> getWaveformPreview(const beatlink::MediaDetails& sourceMedia,
                                                                const DataReference& track) = 0;

    virtual std::shared_ptr<WaveformDetail> getWaveformDetail(const beatlink::MediaDetails& sourceMedia,
                                                              const DataReference& track) = 0;

    virtual std::shared_ptr<AnalysisTag> getAnalysisSection(const beatlink::MediaDetails& sourceMedia,
                                                            const DataReference& track,
                                                            const std::string& fileExtension,
                                                            const std::string& typeTag) = 0;
};

using MetadataProviderPtr = std::shared_ptr<MetadataProvider>;

} // namespace beatlink::data
