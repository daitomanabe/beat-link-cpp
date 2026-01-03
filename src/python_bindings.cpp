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
        BeatFinder::getInstance().stop();
        DeviceFinder::getInstance().stop();
        // Clear all listeners to release Python callback references
        BeatFinder::getInstance().clearListeners();
        DeviceFinder::getInstance().clearListeners();
    }, "Internal cleanup function - called automatically on exit");

    // Register atexit handler to clean up before Python shuts down
    nb::module_::import_("atexit").attr("register")(m.attr("_cleanup"));
}
