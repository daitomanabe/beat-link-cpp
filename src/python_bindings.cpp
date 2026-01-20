/**
 * Beat Link Python Bindings (nanobind)
 *
 * Build with:
 *   cmake .. -DBEATLINK_BUILD_PYTHON=ON
 *   make beatlink_py
 *
 * Usage in Python:
 *   import beatlink_py as bl
 *   print(bl.describe_api())
 *   bl.start_device_finder()
 *   bl.add_beat_listener(lambda beat: print(beat.bpm))
 */

#include <Python.h>
#include <nanobind/nanobind.h>
#include <nanobind/stl/string.h>
#include <nanobind/stl/vector.h>
#include <nanobind/stl/optional.h>
#include <nanobind/stl/function.h>
#include <memory>
#include <mutex>
#include <optional>
#include <sstream>
#include <iomanip>

#include <beatlink/BeatLink.hpp>

/**
 * Thread-safe Python callback wrapper.
 * Ensures GIL is held for both invocation and destruction.
 */
class SafePyCallback {
public:
    explicit SafePyCallback(nanobind::callable cb) {
        // Store as object to handle reference counting properly
        callback_ = std::make_shared<nanobind::object>(std::move(cb));
    }

    template<typename... Args>
    void operator()(Args&&... args) const {
        // Check if Python is still initialized before acquiring GIL
        if (!Py_IsInitialized()) {
            return;
        }

        try {
            nanobind::gil_scoped_acquire acquire;
            if (callback_ && callback_->is_valid()) {
                (*callback_)(std::forward<Args>(args)...);
            }
        } catch (...) {
            // Ignore all errors during callback
        }
    }

    ~SafePyCallback() {
        // Only clean up if Python is still running
        if (!Py_IsInitialized()) {
            // Python already shut down, don't touch the callback
            callback_.reset();
            return;
        }

        // Ensure GIL is held when destroying the Python object
        if (callback_ && callback_.use_count() == 1) {
            try {
                nanobind::gil_scoped_acquire acquire;
                callback_.reset();
            } catch (...) {
                // Ignore errors during cleanup
                callback_.reset();
            }
        }
    }

    // Copy constructor
    SafePyCallback(const SafePyCallback& other) : callback_(other.callback_) {}

    // Move constructor
    SafePyCallback(SafePyCallback&& other) noexcept : callback_(std::move(other.callback_)) {}

    // Copy assignment
    SafePyCallback& operator=(const SafePyCallback& other) {
        if (this != &other) {
            callback_ = other.callback_;
        }
        return *this;
    }

    // Move assignment
    SafePyCallback& operator=(SafePyCallback&& other) noexcept {
        callback_ = std::move(other.callback_);
        return *this;
    }

private:
    std::shared_ptr<nanobind::object> callback_;
};

namespace nb = nanobind;
using namespace beatlink;

namespace {

// Listener management
std::mutex g_listeners_mutex;
std::vector<beatlink::data::TrackPositionListenerPtr> g_track_position_listeners;
std::vector<beatlink::data::TrackMetadataListenerPtr> g_metadata_listeners;
std::vector<beatlink::data::BeatGridListenerPtr> g_beatgrid_listeners;
std::vector<beatlink::data::WaveformListenerPtr> g_waveform_listeners;
std::vector<beatlink::data::AlbumArtListenerPtr> g_albumart_listeners;
std::vector<beatlink::data::SignatureListenerPtr> g_signature_listeners;

// Aliases for backwards compatibility
std::mutex& g_track_position_mutex = g_listeners_mutex;

// Listener storage for beat and device listeners (for removal)
std::vector<beatlink::BeatListenerPtr> g_beat_listeners;
std::vector<beatlink::DeviceAnnouncementListenerPtr> g_device_found_listeners;
std::vector<beatlink::DeviceAnnouncementListenerPtr> g_device_lost_listeners;

}

/**
 * Python-friendly Beat wrapper.
 * Provides easy access to beat information.
 */
struct PyBeat {
    int device_number;
    std::string device_name;
    double bpm;
    double effective_bpm;
    double pitch_percent;
    int beat_in_bar;
    int64_t next_beat_ms;
    int64_t next_bar_ms;
};

/**
 * Python-friendly Device wrapper.
 */
struct PyDevice {
    int device_number;
    std::string device_name;
    std::string address;
    bool is_opus_quad;
};

/**
 * Python-friendly TrackMetadata wrapper.
 */
struct PyTrackMetadata {
    int rekordbox_id;
    int track_type;
    int slot;
    std::string title;
    std::string artist;
    std::string album;
    std::string key;
    std::string genre;
    std::string label;
    int duration;
    int tempo;
    int rating;
    int year;
};

/**
 * Track metadata update wrapper.
 */
struct PyTrackMetadataUpdate {
    int player;
    std::optional<PyTrackMetadata> metadata;
};

/**
 * Python-friendly TrackPosition wrapper.
 */
struct PyTrackPosition {
    int64_t timestamp;
    int64_t milliseconds;
    int beat_number;
    int beat_in_bar;
    bool playing;
    bool definitive;
    bool precise;
    bool from_beat;
    double pitch_multiplier;
    bool reverse;
};

/**
 * Track position update wrapper.
 */
struct PyTrackPositionUpdate {
    int player;
    std::optional<PyTrackPosition> position;
};

/**
 * Signature update wrapper.
 */
struct PySignature {
    int player;
    std::string signature;
};

/**
 * Waveform data wrapper.
 */
struct PyWaveformPreview {
    std::vector<uint8_t> data;
    int segment_count;
    int max_height;
    bool is_color;
};

struct PyWaveformDetail {
    std::vector<uint8_t> data;
    int frame_count;
    bool is_color;
};

/**
 * Album art wrapper.
 */
struct PyAlbumArt {
    std::vector<uint8_t> raw_bytes;
    std::string format;
    int width;
    int height;
};

/**
 * Cue entry wrapper.
 */
struct PyCueEntry {
    int hot_cue_number;
    bool is_loop;
    int64_t cue_position;
    int64_t cue_time_ms;
    int64_t loop_position;
    int64_t loop_time_ms;
    std::string comment;
    int color_id;
    std::string color_name;
};

/**
 * Cue list wrapper.
 */
struct PyCueList {
    std::vector<PyCueEntry> entries;
    int hot_cue_count;
    int memory_point_count;
    int loop_count;
};

/**
 * CDJ status wrapper.
 */
struct PyCdjStatus {
    int device_number;
    std::string device_name;
    int track_source_player;
    int track_source_slot;
    int track_type;
    int rekordbox_id;
    int track_number;
    bool is_playing;
    bool is_paused;
    bool is_cued;
    bool is_on_air;
    bool is_synced;
    bool is_tempo_master;
    bool is_playing_forwards;
    double effective_tempo;
    int pitch;
    int beat_number;
    int beat_within_bar;
};

/**
 * Player settings wrapper.
 */
struct PyPlayerSettings {
    int device_number;
    bool auto_cue;
    bool quantize;
    int tempo_range;
    int jog_mode;
    int needle_lock;
    int time_mode;
};

/**
 * BeatGrid entry wrapper.
 */
struct PyBeatGridEntry {
    int beat_number;
    int beat_within_bar;
    int bpm;  // BPM * 100
    int64_t time_ms;
};

/**
 * BeatGrid wrapper.
 */
struct PyBeatGrid {
    std::vector<PyBeatGridEntry> beats;
    int beat_count;
};

/**
 * Convert Beat to Python-friendly struct.
 */
PyBeat beatToPyBeat(const Beat& beat) {
    return PyBeat{
        .device_number = beat.getDeviceNumber(),
        .device_name = beat.getDeviceName(),
        .bpm = SafetyCurtain::sanitizeBpm(beat.getBpm() / 100.0),
        .effective_bpm = SafetyCurtain::sanitizeBpm(beat.getEffectiveTempo()),
        .pitch_percent = SafetyCurtain::sanitizePitchPercent(
            Util::pitchToPercentage(beat.getPitch())
        ),
        .beat_in_bar = SafetyCurtain::sanitizeBeat(beat.getBeatWithinBar()),
        .next_beat_ms = beat.getNextBeat(),
        .next_bar_ms = beat.getNextBar()
    };
}

/**
 * Convert DeviceAnnouncement to Python-friendly struct.
 */
PyDevice deviceToPyDevice(const DeviceAnnouncement& device) {
    return PyDevice{
        .device_number = device.getDeviceNumber(),
        .device_name = device.getDeviceName(),
        .address = device.getAddress().to_string(),
        .is_opus_quad = device.isOpusQuad()
    };
}

/**
 * Convert TrackMetadata to Python-friendly struct.
 */
PyTrackMetadata trackMetadataToPy(const data::TrackMetadata& metadata) {
    const auto& reference = metadata.getTrackReference();
    PyTrackMetadata result{
        .rekordbox_id = reference.getRekordboxId(),
        .track_type = static_cast<int>(reference.getTrackType()),
        .slot = static_cast<int>(reference.getSlot()),
        .title = metadata.getTitle(),
        .artist = metadata.getArtist() ? metadata.getArtist()->getLabel() : std::string(),
        .album = metadata.getAlbum() ? metadata.getAlbum()->getLabel() : std::string(),
        .key = metadata.getKey() ? metadata.getKey()->getLabel() : std::string(),
        .genre = metadata.getGenre() ? metadata.getGenre()->getLabel() : std::string(),
        .label = metadata.getLabel() ? metadata.getLabel()->getLabel() : std::string(),
        .duration = metadata.getDuration(),
        .tempo = metadata.getTempo(),
        .rating = metadata.getRating(),
        .year = metadata.getYear()
    };
    return result;
}

PyTrackMetadataUpdate trackMetadataUpdateToPy(const data::TrackMetadataUpdate& update) {
    PyTrackMetadataUpdate result;
    result.player = update.player;
    if (update.metadata) {
        result.metadata = trackMetadataToPy(*update.metadata);
    } else {
        result.metadata = std::nullopt;
    }
    return result;
}

/**
 * Convert TrackPositionUpdate to Python-friendly struct.
 */
PyTrackPosition trackPositionToPy(const data::TrackPositionUpdate& update) {
    return PyTrackPosition{
        .timestamp = update.timestamp,
        .milliseconds = update.milliseconds,
        .beat_number = update.beatNumber,
        .beat_in_bar = update.getBeatWithinBar(),
        .playing = update.playing,
        .definitive = update.definitive,
        .precise = update.precise,
        .from_beat = update.fromBeat,
        .pitch_multiplier = update.pitch,
        .reverse = update.reverse
    };
}

PyTrackPositionUpdate trackPositionUpdateToPy(int player,
                                              const std::shared_ptr<data::TrackPositionUpdate>& update) {
    PyTrackPositionUpdate result;
    result.player = player;
    if (update) {
        result.position = trackPositionToPy(*update);
    } else {
        result.position = std::nullopt;
    }
    return result;
}

PySignature signatureToPy(const data::SignatureUpdate& update) {
    return PySignature{
        .player = update.player,
        .signature = update.signature
    };
}

/**
 * Convert WaveformPreview to Python-friendly struct.
 */
PyWaveformPreview waveformPreviewToPy(const data::WaveformPreview& preview) {
    auto data = preview.getData();
    return PyWaveformPreview{
        .data = std::vector<uint8_t>(data.begin(), data.end()),
        .segment_count = preview.getSegmentCount(),
        .max_height = preview.getMaxHeight(),
        .is_color = preview.isColor()
    };
}

/**
 * Convert WaveformDetail to Python-friendly struct.
 */
PyWaveformDetail waveformDetailToPy(const data::WaveformDetail& detail) {
    auto data = detail.getData();
    return PyWaveformDetail{
        .data = std::vector<uint8_t>(data.begin(), data.end()),
        .frame_count = detail.getFrameCount(),
        .is_color = detail.isColor()
    };
}

/**
 * Convert AlbumArt to Python-friendly struct.
 */
PyAlbumArt albumArtToPy(const data::AlbumArt& art) {
    auto dims = art.getDimensions();
    return PyAlbumArt{
        .raw_bytes = art.getRawBytesCopy(),
        .format = art.getImageFormat(),
        .width = dims.first,
        .height = dims.second
    };
}

/**
 * Convert CueList::Entry to Python-friendly struct.
 */
PyCueEntry cueEntryToPy(const data::CueList::Entry& entry) {
    // Get color name from colorId using ColorItem's helper
    std::string colorName = data::ColorItem::colorNameForId(entry.colorId);

    return PyCueEntry{
        .hot_cue_number = entry.hotCueNumber,
        .is_loop = entry.isLoop,
        .cue_position = entry.cuePosition,
        .cue_time_ms = entry.cueTime,
        .loop_position = entry.loopPosition,
        .loop_time_ms = entry.loopTime,
        .comment = entry.comment,
        .color_id = entry.colorId,
        .color_name = colorName
    };
}

/**
 * Convert CueList to Python-friendly struct.
 */
PyCueList cueListToPy(const data::CueList& cueList) {
    std::vector<PyCueEntry> entries;
    entries.reserve(cueList.getEntries().size());

    int loopCount = 0;
    for (const auto& entry : cueList.getEntries()) {
        entries.push_back(cueEntryToPy(entry));
        if (entry.isLoop) {
            ++loopCount;
        }
    }

    return PyCueList{
        .entries = std::move(entries),
        .hot_cue_count = cueList.getHotCueCount(),
        .memory_point_count = cueList.getMemoryPointCount(),
        .loop_count = loopCount
    };
}

/**
 * Convert CdjStatus to Python-friendly struct.
 */
PyCdjStatus cdjStatusToPy(const CdjStatus& status) {
    auto playState = status.getPlayState();
    return PyCdjStatus{
        .device_number = status.getDeviceNumber(),
        .device_name = status.getDeviceName(),
        .track_source_player = status.getTrackSourcePlayer(),
        .track_source_slot = static_cast<int>(status.getTrackSourceSlot()),
        .track_type = static_cast<int>(status.getTrackType()),
        .rekordbox_id = status.getRekordboxId(),
        .track_number = status.getTrackNumber(),
        .is_playing = status.isPlaying(),
        .is_paused = (playState == PlayState::PAUSED),
        .is_cued = status.isAtCue(),
        .is_on_air = status.isOnAir(),
        .is_synced = status.isSynced(),
        .is_tempo_master = status.isTempoMaster(),
        .is_playing_forwards = status.isPlayingForwards(),
        .effective_tempo = status.getEffectiveTempo(),
        .pitch = status.getPitch(),
        .beat_number = status.getBeatNumber(),
        .beat_within_bar = status.getBeatWithinBar()
    };
}

/**
 * Convert BeatGrid to Python-friendly struct.
 */
PyBeatGrid beatGridToPy(const data::BeatGrid& grid) {
    std::vector<PyBeatGridEntry> beats;
    const int beatCount = grid.getBeatCount();
    beats.reserve(beatCount);

    for (int i = 1; i <= beatCount; ++i) {
        beats.push_back(PyBeatGridEntry{
            .beat_number = i,
            .beat_within_bar = grid.getBeatWithinBar(i),
            .bpm = grid.getBpm(i),
            .time_ms = grid.getTimeWithinTrack(i)
        });
    }

    return PyBeatGrid{
        .beats = std::move(beats),
        .beat_count = beatCount
    };
}

NB_MODULE(beatlink_py, m) {
    m.doc() = "Beat Link C++ Python bindings - Pioneer DJ Link protocol library";

    // Version info
    m.attr("__version__") = Version::STRING;

    // API introspection (critical for AI agents)
    m.def("describe_api", &describe_api_json,
          "Get the API schema as JSON for AI agent introspection");

    // PyBeat struct
    nb::class_<PyBeat>(m, "Beat")
        .def_ro("device_number", &PyBeat::device_number, "Device number (1-4)")
        .def_ro("device_name", &PyBeat::device_name, "Device name (e.g., 'CDJ-3000')")
        .def_ro("bpm", &PyBeat::bpm, "Track BPM")
        .def_ro("effective_bpm", &PyBeat::effective_bpm, "Effective BPM (with pitch)")
        .def_ro("pitch_percent", &PyBeat::pitch_percent, "Pitch adjustment in percent")
        .def_ro("beat_in_bar", &PyBeat::beat_in_bar, "Beat position in bar (1-4)")
        .def_ro("next_beat_ms", &PyBeat::next_beat_ms, "Time until next beat (ms)")
        .def_ro("next_bar_ms", &PyBeat::next_bar_ms, "Time until next bar (ms)")
        .def("__repr__", [](const PyBeat& b) {
            std::ostringstream oss;
            oss << "<Beat device=" << b.device_number
                << " bpm=" << std::fixed << std::setprecision(1) << b.effective_bpm
                << " beat=" << b.beat_in_bar << "/4>";
            return oss.str();
        });

    // PyDevice struct
    nb::class_<PyDevice>(m, "Device")
        .def_ro("device_number", &PyDevice::device_number, "Device number (1-4)")
        .def_ro("device_name", &PyDevice::device_name, "Device name")
        .def_ro("address", &PyDevice::address, "IP address")
        .def_ro("is_opus_quad", &PyDevice::is_opus_quad, "Is this an Opus Quad")
        .def("__repr__", [](const PyDevice& d) {
            std::ostringstream oss;
            oss << "<Device #" << d.device_number
                << " '" << d.device_name << "' @ " << d.address << ">";
            return oss.str();
        });

    nb::class_<PyTrackMetadata>(m, "TrackMetadata")
        .def_ro("rekordbox_id", &PyTrackMetadata::rekordbox_id, "Rekordbox track ID")
        .def_ro("track_type", &PyTrackMetadata::track_type, "Track type enum value")
        .def_ro("slot", &PyTrackMetadata::slot, "Track source slot enum value")
        .def_ro("title", &PyTrackMetadata::title, "Track title")
        .def_ro("artist", &PyTrackMetadata::artist, "Artist")
        .def_ro("album", &PyTrackMetadata::album, "Album")
        .def_ro("key", &PyTrackMetadata::key, "Musical key")
        .def_ro("genre", &PyTrackMetadata::genre, "Genre")
        .def_ro("label", &PyTrackMetadata::label, "Label")
        .def_ro("duration", &PyTrackMetadata::duration, "Duration (seconds)")
        .def_ro("tempo", &PyTrackMetadata::tempo, "Tempo (BPM x100)")
        .def_ro("rating", &PyTrackMetadata::rating, "Rating")
        .def_ro("year", &PyTrackMetadata::year, "Year")
        .def("__repr__", [](const PyTrackMetadata& mdata) {
            std::ostringstream oss;
            oss << "<TrackMetadata '" << mdata.title << "'>";
            return oss.str();
        });

    nb::class_<PyTrackMetadataUpdate>(m, "TrackMetadataUpdate")
        .def_ro("player", &PyTrackMetadataUpdate::player, "Player number")
        .def_ro("metadata", &PyTrackMetadataUpdate::metadata, "Track metadata or None")
        .def("__repr__", [](const PyTrackMetadataUpdate& update) {
            std::ostringstream oss;
            oss << "<TrackMetadataUpdate player=" << update.player;
            if (update.metadata) {
                oss << " title='" << update.metadata->title << "'";
            } else {
                oss << " (cleared)";
            }
            oss << ">";
            return oss.str();
        });

    nb::class_<PyTrackPosition>(m, "TrackPosition")
        .def_ro("timestamp", &PyTrackPosition::timestamp, "Timestamp (ns)")
        .def_ro("milliseconds", &PyTrackPosition::milliseconds, "Position in track (ms)")
        .def_ro("beat_number", &PyTrackPosition::beat_number, "Beat number in grid")
        .def_ro("beat_in_bar", &PyTrackPosition::beat_in_bar, "Beat position within bar")
        .def_ro("playing", &PyTrackPosition::playing, "Is playing")
        .def_ro("definitive", &PyTrackPosition::definitive, "Is definitive position")
        .def_ro("precise", &PyTrackPosition::precise, "From precise position packet")
        .def_ro("from_beat", &PyTrackPosition::from_beat, "Derived from beat packet")
        .def_ro("pitch_multiplier", &PyTrackPosition::pitch_multiplier, "Pitch multiplier")
        .def_ro("reverse", &PyTrackPosition::reverse, "Playing backwards")
        .def("__repr__", [](const PyTrackPosition& pos) {
            std::ostringstream oss;
            oss << "<TrackPosition ms=" << pos.milliseconds << " beat=" << pos.beat_number << ">";
            return oss.str();
        });

    nb::class_<PyTrackPositionUpdate>(m, "TrackPositionUpdate")
        .def_ro("player", &PyTrackPositionUpdate::player, "Player number")
        .def_ro("position", &PyTrackPositionUpdate::position, "Track position or None")
        .def("__repr__", [](const PyTrackPositionUpdate& update) {
            std::ostringstream oss;
            oss << "<TrackPositionUpdate player=" << update.player;
            if (update.position) {
                oss << " ms=" << update.position->milliseconds;
            } else {
                oss << " (cleared)";
            }
            oss << ">";
            return oss.str();
        });

    nb::class_<PySignature>(m, "Signature")
        .def_ro("player", &PySignature::player, "Player number")
        .def_ro("signature", &PySignature::signature, "Time signature")
        .def("__repr__", [](const PySignature& sig) {
            std::ostringstream oss;
            oss << "<Signature player=" << sig.player << " '" << sig.signature << "'>";
            return oss.str();
        });

    // WaveformPreview binding
    nb::class_<PyWaveformPreview>(m, "WaveformPreview")
        .def_ro("data", &PyWaveformPreview::data, "Waveform height data")
        .def_ro("segment_count", &PyWaveformPreview::segment_count, "Number of segments")
        .def_ro("max_height", &PyWaveformPreview::max_height, "Maximum height value")
        .def_ro("is_color", &PyWaveformPreview::is_color, "Has color information")
        .def("__repr__", [](const PyWaveformPreview& w) {
            std::ostringstream oss;
            oss << "<WaveformPreview segments=" << w.segment_count << " color=" << w.is_color << ">";
            return oss.str();
        });

    // WaveformDetail binding
    nb::class_<PyWaveformDetail>(m, "WaveformDetail")
        .def_ro("data", &PyWaveformDetail::data, "Detailed waveform data")
        .def_ro("frame_count", &PyWaveformDetail::frame_count, "Number of frames")
        .def_ro("is_color", &PyWaveformDetail::is_color, "Has color information")
        .def("__repr__", [](const PyWaveformDetail& w) {
            std::ostringstream oss;
            oss << "<WaveformDetail frames=" << w.frame_count << " color=" << w.is_color << ">";
            return oss.str();
        });

    // AlbumArt binding
    nb::class_<PyAlbumArt>(m, "AlbumArt")
        .def_ro("raw_bytes", &PyAlbumArt::raw_bytes, "Raw image bytes (JPEG/PNG)")
        .def_ro("format", &PyAlbumArt::format, "Image format (jpeg/png)")
        .def_ro("width", &PyAlbumArt::width, "Image width")
        .def_ro("height", &PyAlbumArt::height, "Image height")
        .def("__repr__", [](const PyAlbumArt& a) {
            std::ostringstream oss;
            oss << "<AlbumArt " << a.width << "x" << a.height << " " << a.format << ">";
            return oss.str();
        });

    // CueEntry binding
    nb::class_<PyCueEntry>(m, "CueEntry")
        .def_ro("hot_cue_number", &PyCueEntry::hot_cue_number, "Hot cue number (0=memory point)")
        .def_ro("is_loop", &PyCueEntry::is_loop, "Is this a loop")
        .def_ro("cue_position", &PyCueEntry::cue_position, "Cue position (half-frames)")
        .def_ro("cue_time_ms", &PyCueEntry::cue_time_ms, "Cue time (milliseconds)")
        .def_ro("loop_position", &PyCueEntry::loop_position, "Loop end position")
        .def_ro("loop_time_ms", &PyCueEntry::loop_time_ms, "Loop end time (milliseconds)")
        .def_ro("comment", &PyCueEntry::comment, "Cue comment")
        .def_ro("color_id", &PyCueEntry::color_id, "Color ID")
        .def_ro("color_name", &PyCueEntry::color_name, "Color name")
        .def("__repr__", [](const PyCueEntry& c) {
            std::ostringstream oss;
            if (c.hot_cue_number > 0) {
                oss << "<HotCue #" << c.hot_cue_number;
            } else {
                oss << "<MemoryPoint";
            }
            oss << " @ " << c.cue_time_ms << "ms>";
            return oss.str();
        });

    // CueList binding
    nb::class_<PyCueList>(m, "CueList")
        .def_ro("entries", &PyCueList::entries, "List of cue entries")
        .def_ro("hot_cue_count", &PyCueList::hot_cue_count, "Number of hot cues")
        .def_ro("memory_point_count", &PyCueList::memory_point_count, "Number of memory points")
        .def_ro("loop_count", &PyCueList::loop_count, "Number of loops")
        .def("__repr__", [](const PyCueList& c) {
            std::ostringstream oss;
            oss << "<CueList hot_cues=" << c.hot_cue_count
                << " memory=" << c.memory_point_count << " loops=" << c.loop_count << ">";
            return oss.str();
        });

    // CdjStatus binding
    nb::class_<PyCdjStatus>(m, "CdjStatus")
        .def_ro("device_number", &PyCdjStatus::device_number, "Device number")
        .def_ro("device_name", &PyCdjStatus::device_name, "Device name")
        .def_ro("track_source_player", &PyCdjStatus::track_source_player, "Track source player")
        .def_ro("track_source_slot", &PyCdjStatus::track_source_slot, "Track source slot")
        .def_ro("track_type", &PyCdjStatus::track_type, "Track type")
        .def_ro("rekordbox_id", &PyCdjStatus::rekordbox_id, "Rekordbox track ID")
        .def_ro("track_number", &PyCdjStatus::track_number, "Track number in playlist")
        .def_ro("is_playing", &PyCdjStatus::is_playing, "Is playing")
        .def_ro("is_paused", &PyCdjStatus::is_paused, "Is paused")
        .def_ro("is_cued", &PyCdjStatus::is_cued, "Is at cue point")
        .def_ro("is_on_air", &PyCdjStatus::is_on_air, "Is on air (fader up)")
        .def_ro("is_synced", &PyCdjStatus::is_synced, "Is synced")
        .def_ro("is_tempo_master", &PyCdjStatus::is_tempo_master, "Is tempo master")
        .def_ro("is_playing_forwards", &PyCdjStatus::is_playing_forwards, "Playing forwards")
        .def_ro("effective_tempo", &PyCdjStatus::effective_tempo, "Effective tempo (BPM)")
        .def_ro("pitch", &PyCdjStatus::pitch, "Raw pitch value")
        .def_ro("beat_number", &PyCdjStatus::beat_number, "Beat number in track")
        .def_ro("beat_within_bar", &PyCdjStatus::beat_within_bar, "Beat position in bar")
        .def("__repr__", [](const PyCdjStatus& s) {
            std::ostringstream oss;
            oss << "<CdjStatus #" << s.device_number << " ";
            if (s.is_playing) oss << "playing ";
            oss << std::fixed << std::setprecision(1) << s.effective_tempo << " BPM>";
            return oss.str();
        });

    // BeatGridEntry binding
    nb::class_<PyBeatGridEntry>(m, "BeatGridEntry")
        .def_ro("beat_number", &PyBeatGridEntry::beat_number, "Beat number (1-indexed)")
        .def_ro("beat_within_bar", &PyBeatGridEntry::beat_within_bar, "Beat position within bar (1-4)")
        .def_ro("bpm", &PyBeatGridEntry::bpm, "Tempo at this beat (BPM * 100)")
        .def_ro("time_ms", &PyBeatGridEntry::time_ms, "Time position in milliseconds")
        .def("__repr__", [](const PyBeatGridEntry& e) {
            std::ostringstream oss;
            oss << "<BeatGridEntry #" << e.beat_number << " bar=" << e.beat_within_bar
                << " bpm=" << std::fixed << std::setprecision(2) << (e.bpm / 100.0)
                << " time=" << e.time_ms << "ms>";
            return oss.str();
        });

    // BeatGrid binding
    nb::class_<PyBeatGrid>(m, "BeatGrid")
        .def_ro("beats", &PyBeatGrid::beats, "List of beat entries")
        .def_ro("beat_count", &PyBeatGrid::beat_count, "Total number of beats")
        .def("__repr__", [](const PyBeatGrid& g) {
            std::ostringstream oss;
            oss << "<BeatGrid beats=" << g.beat_count << ">";
            return oss.str();
        });

    // DeviceFinder functions
    m.def("start_device_finder", []() {
        return DeviceFinder::getInstance().start();
    }, "Start discovering DJ Link devices");

    m.def("stop_device_finder", []() {
        DeviceFinder::getInstance().stop();
    }, "Stop device discovery");

    m.def("is_device_finder_running", []() {
        return DeviceFinder::getInstance().isRunning();
    }, "Check if device finder is running");

    m.def("get_devices", []() {
        std::vector<PyDevice> result;
        auto devices = DeviceFinder::getInstance().getCurrentDevices();
        for (const auto& d : devices) {
            result.push_back(deviceToPyDevice(d));
        }
        return result;
    }, "Get list of currently discovered devices");

    // BeatFinder functions
    m.def("start_beat_finder", []() {
        return BeatFinder::getInstance().start();
    }, "Start receiving beat packets");

    m.def("stop_beat_finder", []() {
        BeatFinder::getInstance().stop();
    }, "Stop receiving beat packets");

    m.def("is_beat_finder_running", []() {
        return BeatFinder::getInstance().isRunning();
    }, "Check if beat finder is running");

    // VirtualCdj functions
    m.def("start_virtual_cdj", []() {
        return VirtualCdj::getInstance().start();
    }, "Start receiving device updates");

    m.def("stop_virtual_cdj", []() {
        VirtualCdj::getInstance().stop();
    }, "Stop receiving device updates");

    m.def("is_virtual_cdj_running", []() {
        return VirtualCdj::getInstance().isRunning();
    }, "Check if VirtualCdj is running");

    // MetadataFinder functions
    m.def("start_metadata_finder", []() {
        data::MetadataFinder::getInstance().start();
        return data::MetadataFinder::getInstance().isRunning();
    }, "Start metadata finder");

    m.def("stop_metadata_finder", []() {
        data::MetadataFinder::getInstance().stop();
    }, "Stop metadata finder");

    m.def("is_metadata_finder_running", []() {
        return data::MetadataFinder::getInstance().isRunning();
    }, "Check if metadata finder is running");

    m.def("get_track_metadata", [](int player) -> std::optional<PyTrackMetadata> {
        auto metadata = data::MetadataFinder::getInstance().getLatestMetadataFor(player);
        if (!metadata) {
            return std::nullopt;
        }
        return trackMetadataToPy(*metadata);
    }, "Get latest track metadata for a player");

    // BeatGridFinder functions
    m.def("start_beat_grid_finder", []() {
        data::BeatGridFinder::getInstance().start();
        return data::BeatGridFinder::getInstance().isRunning();
    }, "Start beat grid finder");

    m.def("stop_beat_grid_finder", []() {
        data::BeatGridFinder::getInstance().stop();
    }, "Stop beat grid finder");

    m.def("is_beat_grid_finder_running", []() {
        return data::BeatGridFinder::getInstance().isRunning();
    }, "Check if beat grid finder is running");

    // WaveformFinder functions
    m.def("start_waveform_finder", []() {
        data::WaveformFinder::getInstance().start();
        return data::WaveformFinder::getInstance().isRunning();
    }, "Start waveform finder");

    m.def("stop_waveform_finder", []() {
        data::WaveformFinder::getInstance().stop();
    }, "Stop waveform finder");

    m.def("is_waveform_finder_running", []() {
        return data::WaveformFinder::getInstance().isRunning();
    }, "Check if waveform finder is running");

    // SignatureFinder functions
    m.def("start_signature_finder", []() {
        data::SignatureFinder::getInstance().start();
        return data::SignatureFinder::getInstance().isRunning();
    }, "Start signature finder");

    m.def("stop_signature_finder", []() {
        data::SignatureFinder::getInstance().stop();
    }, "Stop signature finder");

    m.def("is_signature_finder_running", []() {
        return data::SignatureFinder::getInstance().isRunning();
    }, "Check if signature finder is running");

    m.def("get_signature", [](int player) {
        return data::SignatureFinder::getInstance().getLatestSignatureFor(player);
    }, "Get latest time signature for a player");

    // TimeFinder functions
    m.def("start_time_finder", []() {
        data::TimeFinder::getInstance().start();
        return data::TimeFinder::getInstance().isRunning();
    }, "Start time finder");

    m.def("stop_time_finder", []() {
        data::TimeFinder::getInstance().stop();
    }, "Stop time finder");

    m.def("is_time_finder_running", []() {
        return data::TimeFinder::getInstance().isRunning();
    }, "Check if time finder is running");

    m.def("get_track_position", [](int player) -> std::optional<PyTrackPosition> {
        auto position = data::TimeFinder::getInstance().getLatestPositionFor(player);
        if (!position) {
            return std::nullopt;
        }
        return trackPositionToPy(*position);
    }, "Get latest track position for a player");

    m.def("get_track_time", [](int player) {
        return data::TimeFinder::getInstance().getTimeFor(player);
    }, "Get interpolated track time for a player");

    // Beat listener registration
    m.def("add_beat_listener", [](nb::callable callback) {
        auto safeCallback = std::make_shared<SafePyCallback>(std::move(callback));
        auto listener = std::make_shared<BeatListenerCallbacks>(
            [safeCallback](const Beat& beat) {
                (*safeCallback)(beatToPyBeat(beat));
            });
        BeatFinder::getInstance().addBeatListener(listener);
        {
            std::lock_guard<std::mutex> lock(g_listeners_mutex);
            g_beat_listeners.push_back(listener);
        }
    }, "Register a callback for beat events");

    // Device listener registration
    m.def("add_device_found_listener", [](nb::callable callback) {
        auto safeCallback = std::make_shared<SafePyCallback>(std::move(callback));
        DeviceFinder::getInstance().addDeviceFoundListener([safeCallback](const DeviceAnnouncement& device) {
            (*safeCallback)(deviceToPyDevice(device));
        });
    }, "Register a callback for device found events");

    m.def("add_device_lost_listener", [](nb::callable callback) {
        auto safeCallback = std::make_shared<SafePyCallback>(std::move(callback));
        DeviceFinder::getInstance().addDeviceLostListener([safeCallback](const DeviceAnnouncement& device) {
            (*safeCallback)(deviceToPyDevice(device));
        });
    }, "Register a callback for device lost events");

    // Metadata listener registration
    m.def("add_track_metadata_listener", [](nb::callable callback) {
        auto safeCallback = std::make_shared<SafePyCallback>(std::move(callback));
        auto listener = std::make_shared<data::TrackMetadataCallbacks>(
            [safeCallback](const data::TrackMetadataUpdate& update) {
                (*safeCallback)(trackMetadataUpdateToPy(update));
            });
        data::MetadataFinder::getInstance().addTrackMetadataListener(listener);
        {
            std::lock_guard<std::mutex> lock(g_listeners_mutex);
            g_metadata_listeners.push_back(listener);
        }
    }, "Register a callback for track metadata updates");

    // Signature listener registration
    m.def("add_signature_listener", [](nb::callable callback) {
        auto safeCallback = std::make_shared<SafePyCallback>(std::move(callback));
        auto listener = std::make_shared<data::SignatureCallbacks>(
            [safeCallback](const data::SignatureUpdate& update) {
                (*safeCallback)(signatureToPy(update));
            });
        data::SignatureFinder::getInstance().addSignatureListener(listener);
        {
            std::lock_guard<std::mutex> lock(g_listeners_mutex);
            g_signature_listeners.push_back(listener);
        }
    }, "Register a callback for signature updates");

    // Track position listener registration
    m.def("add_track_position_listener", [](int player, nb::callable callback) {
        auto safeCallback = std::make_shared<SafePyCallback>(std::move(callback));
        auto listener = std::make_shared<data::TrackPositionCallbacks>(
            [safeCallback, player](const std::shared_ptr<data::TrackPositionUpdate>& update) {
                (*safeCallback)(trackPositionUpdateToPy(player, update));
            });
        data::TimeFinder::getInstance().addTrackPositionListener(player, listener);
        {
            std::lock_guard<std::mutex> lock(g_track_position_mutex);
            g_track_position_listeners.push_back(listener);
        }
    }, "Register a callback for track position updates");

    // Utility functions
    m.def("pitch_to_percentage", &Util::pitchToPercentage,
          "Convert raw pitch value to percentage");

    m.def("pitch_to_multiplier", &Util::pitchToMultiplier,
          "Convert raw pitch value to multiplier");

    // Safety limits (read-only)
    nb::class_<SafetyLimits>(m, "SafetyLimits")
        .def_ro_static("MIN_BPM", &SafetyLimits::MIN_BPM)
        .def_ro_static("MAX_BPM", &SafetyLimits::MAX_BPM)
        .def_ro_static("MIN_PITCH_PERCENT", &SafetyLimits::MIN_PITCH_PERCENT)
        .def_ro_static("MAX_PITCH_PERCENT", &SafetyLimits::MAX_PITCH_PERCENT);

    // Ports (read-only)
    m.attr("PORT_ANNOUNCEMENT") = Ports::ANNOUNCEMENT;
    m.attr("PORT_BEAT") = Ports::BEAT;
    m.attr("PORT_UPDATE") = Ports::UPDATE;

    // ==== Phase 5.2: Data retrieval functions ====

    // ArtFinder functions
    m.def("start_art_finder", []() {
        data::ArtFinder::getInstance().start();
        return data::ArtFinder::getInstance().isRunning();
    }, "Start album art finder");

    m.def("stop_art_finder", []() {
        data::ArtFinder::getInstance().stop();
    }, "Stop album art finder");

    m.def("is_art_finder_running", []() {
        return data::ArtFinder::getInstance().isRunning();
    }, "Check if art finder is running");

    m.def("get_waveform_preview", [](int player) -> std::optional<PyWaveformPreview> {
        auto preview = data::WaveformFinder::getInstance().getLatestPreviewFor(player);
        if (!preview) {
            return std::nullopt;
        }
        return waveformPreviewToPy(*preview);
    }, "Get waveform preview for a player");

    m.def("get_waveform_detail", [](int player) -> std::optional<PyWaveformDetail> {
        auto detail = data::WaveformFinder::getInstance().getLatestDetailFor(player);
        if (!detail) {
            return std::nullopt;
        }
        return waveformDetailToPy(*detail);
    }, "Get waveform detail for a player");

    m.def("get_album_art", [](int player) -> std::optional<PyAlbumArt> {
        auto art = data::ArtFinder::getInstance().getLatestArtFor(player);
        if (!art) {
            return std::nullopt;
        }
        return albumArtToPy(*art);
    }, "Get album art for a player");

    m.def("get_cdj_status", [](int player) -> std::optional<PyCdjStatus> {
        auto status = VirtualCdj::getInstance().getLatestStatusFor(player);
        if (!status) {
            return std::nullopt;
        }
        auto cdjStatus = std::dynamic_pointer_cast<CdjStatus>(status);
        if (!cdjStatus) {
            return std::nullopt;
        }
        return cdjStatusToPy(*cdjStatus);
    }, "Get CDJ status for a player");

    m.def("get_beat_grid", [](int player) -> std::optional<PyBeatGrid> {
        auto grid = data::BeatGridFinder::getInstance().getLatestBeatGridFor(player);
        if (!grid) {
            return std::nullopt;
        }
        return beatGridToPy(*grid);
    }, "Get beat grid for a player");

    m.def("get_cue_list", [](int player) -> std::optional<PyCueList> {
        auto metadata = data::MetadataFinder::getInstance().getLatestMetadataFor(player);
        if (!metadata) {
            return std::nullopt;
        }
        auto cueList = metadata->getCueList();
        if (!cueList) {
            return std::nullopt;
        }
        return cueListToPy(*cueList);
    }, "Get cue list for a player");

    // ==== Phase 5.3: VirtualCdj control methods ====

    m.def("get_local_device_number", []() {
        return VirtualCdj::getInstance().getDeviceNumber();
    }, "Get the local device number");

    m.def("set_device_number", [](int number) {
        VirtualCdj::getInstance().setDeviceNumber(static_cast<uint8_t>(number));
    }, "Set the local device number");

    m.def("get_device_name", []() {
        return VirtualCdj::getInstance().getDeviceName();
    }, "Get the local device name");

    m.def("set_device_name", [](const std::string& name) {
        VirtualCdj::getInstance().setDeviceName(name);
    }, "Set the local device name");

    m.def("get_use_standard_player_number", []() {
        return VirtualCdj::getInstance().getUseStandardPlayerNumber();
    }, "Check if using standard player number");

    m.def("set_use_standard_player_number", [](bool use) {
        VirtualCdj::getInstance().setUseStandardPlayerNumber(use);
    }, "Set whether to use standard player number");

    m.def("get_tempo_master", []() -> std::optional<int> {
        auto master = VirtualCdj::getInstance().getTempoMaster();
        if (!master) {
            return std::nullopt;
        }
        return master->getDeviceNumber();
    }, "Get the device number of the tempo master");

    m.def("get_master_tempo", []() {
        return VirtualCdj::getInstance().getMasterTempo();
    }, "Get the master tempo in BPM");

    m.def("is_sending_status", []() {
        return VirtualCdj::getInstance().isSendingStatus();
    }, "Check if VirtualCdj is sending status packets");

    m.def("set_sending_status", [](bool send) {
        VirtualCdj::getInstance().setSendingStatus(send);
    }, "Enable or disable sending status packets");

    m.def("set_playing", [](bool playing) {
        VirtualCdj::getInstance().setPlaying(playing);
    }, "Set whether the virtual player is playing");

    m.def("is_playing", []() {
        return VirtualCdj::getInstance().isPlaying();
    }, "Check if the virtual player is playing");

    m.def("set_tempo", [](double bpm) {
        VirtualCdj::getInstance().setTempo(bpm);
    }, "Set the tempo of the virtual player");

    m.def("get_tempo", []() {
        return VirtualCdj::getInstance().getTempo();
    }, "Get the tempo of the virtual player");

    m.def("become_tempo_master", []() {
        VirtualCdj::getInstance().becomeTempoMaster();
    }, "Request to become the tempo master");

    m.def("is_tempo_master", []() {
        return VirtualCdj::getInstance().isTempoMaster();
    }, "Check if we are the tempo master");

    m.def("set_synced", [](bool synced) {
        VirtualCdj::getInstance().setSynced(synced);
    }, "Set whether the virtual player is synced");

    m.def("is_synced", []() {
        return VirtualCdj::getInstance().isSynced();
    }, "Check if the virtual player is synced");

    m.def("set_on_air", [](bool on_air) {
        VirtualCdj::getInstance().setOnAir(on_air);
    }, "Set whether the virtual player is on air");

    m.def("is_on_air", []() {
        return VirtualCdj::getInstance().isOnAir();
    }, "Check if the virtual player is on air");

    m.def("send_sync_mode_command", [](int device_number, bool synced) {
        VirtualCdj::getInstance().sendSyncModeCommand(device_number, synced);
    }, "Send sync mode command to a device");

    m.def("appoint_tempo_master", [](int device_number) {
        VirtualCdj::getInstance().appointTempoMaster(device_number);
    }, "Appoint a device as tempo master");

    m.def("send_load_track_command", [](int target_player, int rekordbox_id,
                                         int source_player, int source_slot, int source_type) {
        VirtualCdj::getInstance().sendLoadTrackCommand(
            target_player, rekordbox_id, source_player,
            static_cast<TrackSourceSlot>(source_slot),
            static_cast<TrackType>(source_type));
    }, "Send load track command to a player");

    // ==== Phase 5.4: OpusProvider bindings ====

    m.def("start_opus_provider", []() {
        data::OpusProvider::getInstance().start();
        return data::OpusProvider::getInstance().isRunning();
    }, "Start Opus provider");

    m.def("stop_opus_provider", []() {
        data::OpusProvider::getInstance().stop();
    }, "Stop Opus provider");

    m.def("is_opus_provider_running", []() {
        return data::OpusProvider::getInstance().isRunning();
    }, "Check if Opus provider is running");

    m.def("attach_metadata_archive", [](const std::string& path, int slot) {
        return data::OpusProvider::getInstance().attachMetadataArchive(path, slot);
    }, "Attach a metadata archive for Opus Quad");

    m.def("detach_metadata_archive", [](int slot) {
        data::OpusProvider::getInstance().detachMetadataArchive(slot);
    }, "Detach metadata archive from a slot");

    m.def("set_database_key", [](const std::string& key) {
        data::OpusProvider::getInstance().setDatabaseKey(key);
    }, "Set the database key for encrypted databases");

    m.def("is_using_device_library_plus", []() {
        return data::OpusProvider::getInstance().usingDeviceLibraryPlus();
    }, "Check if using Device Library Plus databases");

    m.def("is_opus_quad", [](const std::string& device_name) {
        return data::OpusProvider::isOpusQuad(device_name);
    }, "Check if device name indicates an Opus Quad");

    m.def("needs_opus_provider", [](const std::string& device_name) {
        return data::OpusProvider::needsOpusProvider(device_name);
    }, "Check if a device needs Opus provider for metadata");

    // ==== Phase 5.1: Listener removal functions ====

    m.def("clear_beat_listeners", []() {
        std::lock_guard<std::mutex> lock(g_listeners_mutex);
        for (const auto& listener : g_beat_listeners) {
            BeatFinder::getInstance().removeBeatListener(listener);
        }
        g_beat_listeners.clear();
    }, "Clear all beat listeners");

    m.def("clear_track_position_listeners", []() {
        std::lock_guard<std::mutex> lock(g_listeners_mutex);
        for (const auto& listener : g_track_position_listeners) {
            data::TimeFinder::getInstance().removeTrackPositionListener(listener);
        }
        g_track_position_listeners.clear();
    }, "Clear all track position listeners");

    m.def("clear_metadata_listeners", []() {
        std::lock_guard<std::mutex> lock(g_listeners_mutex);
        for (const auto& listener : g_metadata_listeners) {
            data::MetadataFinder::getInstance().removeTrackMetadataListener(listener);
        }
        g_metadata_listeners.clear();
    }, "Clear all metadata listeners");

    m.def("clear_waveform_listeners", []() {
        std::lock_guard<std::mutex> lock(g_listeners_mutex);
        for (const auto& listener : g_waveform_listeners) {
            data::WaveformFinder::getInstance().removeWaveformListener(listener);
        }
        g_waveform_listeners.clear();
    }, "Clear all waveform listeners");

    m.def("clear_album_art_listeners", []() {
        std::lock_guard<std::mutex> lock(g_listeners_mutex);
        for (const auto& listener : g_albumart_listeners) {
            data::ArtFinder::getInstance().removeAlbumArtListener(listener);
        }
        g_albumart_listeners.clear();
    }, "Clear all album art listeners");

    m.def("clear_signature_listeners", []() {
        std::lock_guard<std::mutex> lock(g_listeners_mutex);
        for (const auto& listener : g_signature_listeners) {
            data::SignatureFinder::getInstance().removeSignatureListener(listener);
        }
        g_signature_listeners.clear();
    }, "Clear all signature listeners");

    m.def("clear_all_listeners", []() {
        std::lock_guard<std::mutex> lock(g_listeners_mutex);

        for (const auto& listener : g_beat_listeners) {
            BeatFinder::getInstance().removeBeatListener(listener);
        }
        g_beat_listeners.clear();

        for (const auto& listener : g_track_position_listeners) {
            data::TimeFinder::getInstance().removeTrackPositionListener(listener);
        }
        g_track_position_listeners.clear();

        for (const auto& listener : g_metadata_listeners) {
            data::MetadataFinder::getInstance().removeTrackMetadataListener(listener);
        }
        g_metadata_listeners.clear();

        for (const auto& listener : g_waveform_listeners) {
            data::WaveformFinder::getInstance().removeWaveformListener(listener);
        }
        g_waveform_listeners.clear();

        for (const auto& listener : g_albumart_listeners) {
            data::ArtFinder::getInstance().removeAlbumArtListener(listener);
        }
        g_albumart_listeners.clear();

        for (const auto& listener : g_signature_listeners) {
            data::SignatureFinder::getInstance().removeSignatureListener(listener);
        }
        g_signature_listeners.clear();

        BeatFinder::getInstance().clearListeners();
        DeviceFinder::getInstance().clearListeners();
    }, "Clear all listeners");

    // Cleanup function to be called before Python exits
    m.def("_cleanup", []() {
        // Stop all finders to prevent callbacks during shutdown
        {
            std::lock_guard<std::mutex> lock(g_track_position_mutex);
            for (const auto& listener : g_track_position_listeners) {
                data::TimeFinder::getInstance().removeTrackPositionListener(listener);
            }
            g_track_position_listeners.clear();
        }
        for (const auto& listener : data::SignatureFinder::getInstance().getSignatureListeners()) {
            data::SignatureFinder::getInstance().removeSignatureListener(listener);
        }
        for (const auto& listener : data::WaveformFinder::getInstance().getWaveformListeners()) {
            data::WaveformFinder::getInstance().removeWaveformListener(listener);
        }
        for (const auto& listener : data::BeatGridFinder::getInstance().getBeatGridListeners()) {
            data::BeatGridFinder::getInstance().removeBeatGridListener(listener);
        }
        for (const auto& listener : data::MetadataFinder::getInstance().getTrackMetadataListeners()) {
            data::MetadataFinder::getInstance().removeTrackMetadataListener(listener);
        }
        data::TimeFinder::getInstance().stop();
        data::SignatureFinder::getInstance().stop();
        data::WaveformFinder::getInstance().stop();
        data::BeatGridFinder::getInstance().stop();
        data::AnalysisTagFinder::getInstance().stop();
        data::MetadataFinder::getInstance().stop();
        VirtualCdj::getInstance().stop();
        BeatFinder::getInstance().stop();
        DeviceFinder::getInstance().stop();
        // Clear all listeners to release Python callback references
        BeatFinder::getInstance().clearListeners();
        DeviceFinder::getInstance().clearListeners();
    }, "Internal cleanup function - called automatically on exit");

    // Register atexit handler to clean up before Python shuts down
    nb::module_::import_("atexit").attr("register")(m.attr("_cleanup"));
}
