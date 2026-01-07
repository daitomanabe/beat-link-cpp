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
std::mutex g_track_position_mutex;
std::vector<beatlink::data::TrackPositionListenerPtr> g_track_position_listeners;
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
        BeatFinder::getInstance().addBeatListener([safeCallback](const Beat& beat) {
            (*safeCallback)(beatToPyBeat(beat));
        });
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
        data::MetadataFinder::getInstance().addTrackMetadataListener(
            std::make_shared<data::TrackMetadataCallbacks>(
                [safeCallback](const data::TrackMetadataUpdate& update) {
                    (*safeCallback)(trackMetadataUpdateToPy(update));
                }));
    }, "Register a callback for track metadata updates");

    // Signature listener registration
    m.def("add_signature_listener", [](nb::callable callback) {
        auto safeCallback = std::make_shared<SafePyCallback>(std::move(callback));
        data::SignatureFinder::getInstance().addSignatureListener(
            std::make_shared<data::SignatureCallbacks>(
                [safeCallback](const data::SignatureUpdate& update) {
                    (*safeCallback)(signatureToPy(update));
                }));
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
