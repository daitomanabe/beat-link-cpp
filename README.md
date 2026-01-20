# Beat Link C++

[![C++20](https://img.shields.io/badge/C%2B%2B-20-blue.svg)](https://isocpp.org/std/the-standard)
[![License: EPL-2.0](https://img.shields.io/badge/License-EPL--2.0-green.svg)](LICENSE.md)
[![Python 3.8+](https://img.shields.io/badge/Python-3.8%2B-blue.svg)](https://www.python.org/)

A C++20 implementation of the Pioneer DJ Link protocol for real-time communication with CDJ, XDJ, and DJM equipment.

---

## About

**Beat Link C++** is a faithful C++20 port of the original [Java Beat Link](https://github.com/Deep-Symmetry/beat-link) library created by [James Elliott](https://github.com/brunchboy) at [Deep Symmetry](https://deepsymmetry.org).

This port was developed by **[Daito Manabe](https://daito.ws)** / **[Rhizomatiks](https://rhizomatiks.com)** to enable native C++ applications and Python scripts to interact with Pioneer DJ equipment using the DJ Link protocol.

### Features

| Feature | Description |
|---------|-------------|
| **Device Discovery** | Automatic detection of CDJ, XDJ, DJM devices on the network |
| **Beat Synchronization** | Real-time beat timing, BPM, and bar position |
| **Track Metadata** | Title, artist, album, genre, key, duration, rating |
| **Cue Points** | Hot cues, memory points, loops with colors and comments |
| **Waveforms** | Preview and detailed waveform data |
| **Beat Grids** | Beat positions and tempo changes |
| **Album Art** | JPEG/PNG artwork retrieval |
| **Player Control** | Tempo master, sync, pitch control |
| **Python Bindings** | Full-featured Python API (105+ functions) |
| **Cross-Platform** | macOS, Linux, Windows |

---

## Table of Contents

1. [Installation](#installation)
2. [Quick Start](#quick-start)
3. [Architecture](#architecture)
4. [C++ API Reference](#c-api-reference)
5. [Python API Reference](#python-api-reference)
6. [Protocol Reference](#protocol-reference)
7. [Examples](#examples)
8. [Testing](#testing)
9. [Troubleshooting](#troubleshooting)
10. [License](#license)

---

## Installation

### Requirements

- **OS**: macOS 10.15+, Linux (Ubuntu 20.04+), Windows 10+
- **Compiler**: C++20 compatible
  - GCC 11+
  - Clang 14+
  - MSVC 2022+
- **CMake**: 3.15+
- **Python**: 3.8+ (for Python bindings)
- **Network**: Same LAN as DJ Link devices

### Build from Source

```bash
git clone https://github.com/daitomanabe/beat-link-cpp.git
cd beat-link-cpp
mkdir build && cd build

# Standard build
cmake ..
make -j$(nproc)

# With Python bindings
cmake .. -DBEATLINK_BUILD_PYTHON=ON
make beatlink_py

# With unit tests
cmake .. -DBEATLINK_BUILD_TESTS=ON
make beatlink_tests
```

### Build Options

| Option | Default | Description |
|--------|---------|-------------|
| `BEATLINK_BUILD_PYTHON` | OFF | Build Python bindings |
| `BEATLINK_BUILD_TESTS` | OFF | Build unit tests |
| `BEATLINK_USE_SQLCIPHER` | OFF | Use SQLCipher for encrypted databases |

### Build Outputs

| File | Description |
|------|-------------|
| `libbeatlink.a` | Static library |
| `beatlink_example` | Simple beat listener example |
| `beatlink_gui` | GUI monitor (Dear ImGui) |
| `beatlink_cli` | CLI tool with `--schema` support |
| `beatlink_py.*.so` | Python module |
| `beatlink_tests` | Unit tests |

---

## Quick Start

### C++ - Minimal Example

```cpp
#include <beatlink/BeatLink.hpp>
#include <iostream>
#include <thread>

int main() {
    using namespace beatlink;

    // Get singleton instances
    auto& deviceFinder = DeviceFinder::getInstance();
    auto& beatFinder = BeatFinder::getInstance();

    // Register callbacks
    deviceFinder.addDeviceFoundListener([](const DeviceAnnouncement& d) {
        std::cout << "Found: " << d.getDeviceName()
                  << " (#" << d.getDeviceNumber() << ")\n";
    });

    beatFinder.addBeatListener([](const Beat& b) {
        std::cout << "Beat " << b.getBeatWithinBar() << "/4 @ "
                  << b.getEffectiveTempo() << " BPM\n";
    });

    // Start services
    deviceFinder.start();
    beatFinder.start();

    // Run for 60 seconds
    std::this_thread::sleep_for(std::chrono::seconds(60));

    // Cleanup
    beatFinder.stop();
    deviceFinder.stop();
    return 0;
}
```

### Python - Minimal Example

```python
import beatlink_py as bl
import time

def on_beat(beat):
    print(f"Beat {beat.beat_in_bar}/4 @ {beat.effective_bpm:.2f} BPM")

def on_device(device):
    print(f"Found: {device.device_name} (#{device.device_number})")

bl.add_beat_listener(on_beat)
bl.add_device_found_listener(on_device)

bl.start_device_finder()
bl.start_beat_finder()

try:
    while True:
        time.sleep(1)
except KeyboardInterrupt:
    pass

bl.stop_beat_finder()
bl.stop_device_finder()
bl.clear_all_listeners()
```

---

## Architecture

### Component Overview

```
┌─────────────────────────────────────────────────────────────────┐
│                      Beat Link C++ Architecture                  │
├─────────────────────────────────────────────────────────────────┤
│                                                                  │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐          │
│  │ DeviceFinder │  │  BeatFinder  │  │  VirtualCdj  │          │
│  │  (UDP 50000) │  │  (UDP 50001) │  │  (UDP 50002) │          │
│  └──────┬───────┘  └──────┬───────┘  └──────┬───────┘          │
│         │                 │                 │                    │
│         ▼                 ▼                 ▼                    │
│  ┌─────────────────────────────────────────────────────┐        │
│  │                   Listener System                    │        │
│  │  DeviceAnnouncementListener, BeatListener,          │        │
│  │  DeviceUpdateListener, MasterListener, etc.         │        │
│  └─────────────────────────────────────────────────────┘        │
│                                                                  │
│  ┌─────────────────────────────────────────────────────┐        │
│  │                    Data Finders                      │        │
│  │  MetadataFinder, WaveformFinder, BeatGridFinder,    │        │
│  │  ArtFinder, TimeFinder, SignatureFinder             │        │
│  └──────────────────────┬──────────────────────────────┘        │
│                         │                                        │
│                         ▼                                        │
│  ┌─────────────────────────────────────────────────────┐        │
│  │              DBServer (TCP 12523)                    │        │
│  │  Query track metadata from players' databases       │        │
│  └──────────────────────┬──────────────────────────────┘        │
│                         │                                        │
│                         ▼                                        │
│  ┌─────────────────────────────────────────────────────┐        │
│  │                   Data Types                         │        │
│  │  TrackMetadata, BeatGrid, CueList, WaveformPreview, │        │
│  │  WaveformDetail, AlbumArt, DataReference            │        │
│  └─────────────────────────────────────────────────────┘        │
│                                                                  │
└─────────────────────────────────────────────────────────────────┘
```

### Service Startup Order

```cpp
// Recommended startup order for full functionality:

// 1. Device discovery (required)
DeviceFinder::getInstance().start();

// 2. Beat monitoring (optional)
BeatFinder::getInstance().start();

// 3. Virtual CDJ - join the network as a player (required for metadata)
VirtualCdj::getInstance().start();

// 4. Metadata services (require VirtualCdj)
data::MetadataFinder::getInstance().start();
data::BeatGridFinder::getInstance().start();
data::WaveformFinder::getInstance().start();
data::ArtFinder::getInstance().start();
data::TimeFinder::getInstance().start();
```

---

## C++ API Reference

### Core Classes

#### DeviceFinder

Discovers DJ Link devices on the network (UDP port 50000).

```cpp
class DeviceFinder {
    static DeviceFinder& getInstance();

    bool start();
    void stop();
    bool isRunning() const;

    std::vector<DeviceAnnouncement> getCurrentDevices() const;

    void addDeviceFoundListener(DeviceAnnouncementListenerPtr);
    void addDeviceLostListener(DeviceAnnouncementListenerPtr);
    void removeDeviceFoundListener(DeviceAnnouncementListenerPtr);
    void removeDeviceLostListener(DeviceAnnouncementListenerPtr);
};
```

#### BeatFinder

Receives beat packets from players (UDP port 50001).

```cpp
class BeatFinder {
    static BeatFinder& getInstance();

    bool start();
    void stop();
    bool isRunning() const;

    void addBeatListener(BeatListenerPtr);
    void removeBeatListener(BeatListenerPtr);
};
```

#### VirtualCdj

Join the DJ Link network as a virtual player (UDP port 50002).
Required for metadata retrieval.

```cpp
class VirtualCdj {
    static VirtualCdj& getInstance();

    bool start();
    void stop();
    bool isRunning() const;

    int getDeviceNumber() const;
    void setDeviceNumber(int number);

    void setTempo(double bpm);
    void becomeTempMaster();
    void sendSyncCommand(bool sync);

    void addUpdateListener(DeviceUpdateListenerPtr);
    void addMasterListener(MasterListenerPtr);
};
```

### Data Types

#### Beat

```cpp
class Beat {
    int getDeviceNumber() const;
    std::string getDeviceName() const;

    int getBpm() const;              // BPM * 100 (e.g., 12850 = 128.50 BPM)
    int getPitch() const;            // Raw pitch value
    double getEffectiveTempo() const; // BPM adjusted for pitch

    int getBeatWithinBar() const;    // 1-4
    bool isBeatWithinBarMeaningful() const;

    int64_t getNextBeat() const;     // ms until next beat
    int64_t getNextBar() const;      // ms until next bar
    int64_t getSecondBeat() const;
    int64_t getFourthBeat() const;
    int64_t getEighthBeat() const;
};
```

#### CdjStatus

```cpp
class CdjStatus {
    int getDeviceNumber() const;
    std::string getDeviceName() const;

    // Play state
    bool isPlaying() const;
    bool isAtCue() const;
    bool isTrackLoaded() const;
    PlayState getPlayState() const;

    // Tempo
    int getBpm() const;
    int getPitch() const;
    double getEffectiveTempo() const;
    int getBeatWithinBar() const;

    // Sync
    bool isTempoMaster() const;
    bool isSynced() const;
    bool isOnAir() const;

    // Track source
    int getTrackSourcePlayer() const;
    TrackSourceSlot getTrackSourceSlot() const;
    TrackType getTrackType() const;
    int getRekordboxId() const;
};
```

#### TrackMetadata

```cpp
class TrackMetadata {
    std::string getTitle() const;
    std::string getArtist() const;
    std::string getAlbum() const;
    std::string getGenre() const;
    std::string getLabel() const;
    std::string getComment() const;
    std::string getKey() const;
    std::string getOriginalArtist() const;
    std::string getRemixer() const;
    std::string getDateAdded() const;

    int getTempo() const;           // BPM * 100
    int getDuration() const;        // seconds
    int getRating() const;          // 0-5
    int getYear() const;
    int getTrackNumber() const;
    int getBitrate() const;
    int getPlayCount() const;

    int getColorId() const;
    std::string getColorName() const;
};
```

#### CueList

```cpp
class CueList {
    struct Entry {
        int hotCueNumber;           // 0 = memory point, 1-8 = hot cue
        bool isLoop;
        int64_t cueTimeMs;
        int64_t loopEndMs;          // only if isLoop
        int colorId;
        std::string comment;
    };

    int getHotCueCount() const;
    int getMemoryPointCount() const;
    int getLoopCount() const;
    const std::vector<Entry>& getEntries() const;
};
```

#### BeatGrid

```cpp
class BeatGrid {
    int getBeatCount() const;
    int getBpm(int beatNumber) const;         // BPM * 100 at beat
    int getBeatWithinBar(int beatNumber) const;
    int64_t getTimeWithinTrack(int beatNumber) const;  // ms

    int findBeatAtTime(int64_t timeMs) const;
};
```

#### WaveformPreview / WaveformDetail

```cpp
class WaveformPreview {
    int getSegmentCount() const;
    int getMaxHeight() const;
    bool isColor() const;
    std::span<const uint8_t> getData() const;
};

class WaveformDetail {
    int getFrameCount() const;
    bool isColor() const;
    std::span<const uint8_t> getData() const;
};
```

### Data Finders

```cpp
namespace data {
    // Get track metadata for a player
    MetadataFinder::getInstance().getLatestMetadataFor(int player);

    // Get beat grid
    BeatGridFinder::getInstance().getLatestBeatGridFor(int player);

    // Get waveforms
    WaveformFinder::getInstance().getLatestPreviewFor(int player);
    WaveformFinder::getInstance().getLatestDetailFor(int player);

    // Get album art
    ArtFinder::getInstance().getLatestArtFor(int player);
}
```

### Utility Functions

```cpp
namespace beatlink {
    class Util {
        // Pitch conversion
        static double pitchToPercentage(int64_t pitch);
        static double pitchToMultiplier(int64_t pitch);
        static int percentageToPitch(double percentage);

        // Time conversion
        static int64_t halfFrameToTime(int64_t halfFrame);
        static int64_t timeToHalfFrame(int64_t timeMs);

        // Byte operations
        static int64_t bytesToNumber(const uint8_t* buf, size_t start, size_t len);
        static std::string formatMac(const std::array<uint8_t, 6>& mac);
    };
}
```

---

## Python API Reference

### Module: `beatlink_py`

```python
import beatlink_py as bl

# Version
print(bl.__version__)  # "0.2.0"
```

### Service Control

```python
# Device discovery
bl.start_device_finder()        # Start device discovery
bl.stop_device_finder()         # Stop device discovery
bl.is_device_finder_running()   # Check if running

# Beat monitoring
bl.start_beat_finder()
bl.stop_beat_finder()
bl.is_beat_finder_running()

# Virtual CDJ (required for metadata)
bl.start_virtual_cdj()
bl.stop_virtual_cdj()
bl.is_virtual_cdj_running()

# Metadata services
bl.start_metadata_finder()
bl.stop_metadata_finder()
bl.start_beat_grid_finder()
bl.stop_beat_grid_finder()
bl.start_waveform_finder()
bl.stop_waveform_finder()
bl.start_art_finder()
bl.stop_art_finder()

# Opus provider (offline mode)
bl.start_opus_provider()
bl.stop_opus_provider()
```

### Data Retrieval

```python
# Get connected devices
devices = bl.get_devices()
for d in devices:
    print(f"{d.device_name} #{d.device_number} @ {d.address}")

# Get track metadata (player 1-4)
metadata = bl.get_track_metadata(1)
if metadata:
    print(f"Title: {metadata.title}")
    print(f"Artist: {metadata.artist}")
    print(f"BPM: {metadata.tempo / 100:.2f}")
    print(f"Key: {metadata.key}")

# Get CDJ status
status = bl.get_cdj_status(1)
if status:
    print(f"Playing: {status.is_playing}")
    print(f"Master: {status.is_tempo_master}")
    print(f"BPM: {status.effective_tempo:.2f}")

# Get cue list
cues = bl.get_cue_list(1)
if cues:
    for entry in cues.entries:
        print(f"Cue @ {entry.cue_time_ms}ms - {entry.color_name}")

# Get beat grid
grid = bl.get_beat_grid(1)
if grid:
    print(f"Beat count: {grid.beat_count}")
    for beat in grid.beats[:10]:
        print(f"Beat {beat.beat_number}: {beat.bpm/100:.2f} BPM @ {beat.time_ms}ms")

# Get waveform
preview = bl.get_waveform_preview(1)
if preview:
    print(f"Segments: {preview.segment_count}")
    print(f"Color: {preview.is_color}")

# Get album art
art = bl.get_album_art(1)
if art:
    print(f"Size: {art.width}x{art.height}")
    # Save to file
    with open('artwork.jpg', 'wb') as f:
        f.write(bytes(art.raw_bytes))
```

### Event Listeners

```python
# Beat listener
def on_beat(beat):
    print(f"Beat {beat.beat_in_bar}/4 @ {beat.effective_bpm:.2f} BPM")

bl.add_beat_listener(on_beat)
bl.remove_beat_listener(on_beat)
bl.clear_beat_listeners()

# Device listeners
def on_device_found(device):
    print(f"Found: {device.device_name}")

def on_device_lost(device):
    print(f"Lost: {device.device_name}")

bl.add_device_found_listener(on_device_found)
bl.add_device_lost_listener(on_device_lost)
bl.clear_device_found_listeners()
bl.clear_device_lost_listeners()

# Metadata listener
def on_metadata(update):
    if update.metadata:
        print(f"Player {update.player}: {update.metadata.title}")

bl.add_track_metadata_listener(on_metadata)
bl.clear_track_metadata_listeners()

# Clear all listeners
bl.clear_all_listeners()
```

### VirtualCdj Control

```python
# Get/set device number
num = bl.get_device_number()
bl.set_device_number(5)

# Tempo control
bl.set_tempo(128.0)
bl.become_tempo_master()
bl.send_sync_command(True)
```

### Python Data Types

```python
# PyBeat
beat.device_number      # int
beat.device_name        # str
beat.bpm               # int (BPM * 100)
beat.effective_bpm     # float
beat.pitch             # int
beat.beat_in_bar       # int (1-4)
beat.next_beat_ms      # int

# PyDeviceAnnouncement
device.device_number   # int
device.device_name     # str
device.address         # str

# PyTrackMetadata
metadata.title         # str
metadata.artist        # str
metadata.album         # str
metadata.genre         # str
metadata.key           # str
metadata.tempo         # int (BPM * 100)
metadata.duration      # int (seconds)
metadata.rating        # int (0-5)
metadata.year          # int

# PyCdjStatus
status.is_playing      # bool
status.is_paused       # bool
status.is_cued         # bool
status.is_tempo_master # bool
status.is_synced       # bool
status.is_on_air       # bool
status.effective_tempo # float
status.pitch           # int

# PyCueList
cues.hot_cue_count     # int
cues.memory_point_count # int
cues.loop_count        # int
cues.entries           # list[PyCueEntry]

# PyCueEntry
entry.hot_cue_number   # int
entry.is_loop          # bool
entry.cue_time_ms      # int
entry.loop_time_ms     # int
entry.color_name       # str
entry.comment          # str

# PyBeatGrid
grid.beat_count        # int
grid.beats             # list[PyBeatGridEntry]

# PyBeatGridEntry
beat.beat_number       # int
beat.beat_within_bar   # int
beat.bpm               # int (BPM * 100)
beat.time_ms           # int

# PyWaveformPreview
preview.segment_count  # int
preview.max_height     # int
preview.is_color       # bool
preview.data           # list[int]

# PyAlbumArt
art.width              # int
art.height             # int
art.format             # str ("jpeg" or "png")
art.raw_bytes          # list[int]
```

---

## Protocol Reference

### Network Ports

| Port | Protocol | Purpose |
|------|----------|---------|
| 50000 | UDP | Device announcement and discovery |
| 50001 | UDP | Beat broadcast |
| 50002 | UDP | Device status updates |
| 12523 | TCP | DBServer metadata queries |

### Packet Types

| Offset | Size | Description |
|--------|------|-------------|
| 0x00 | 10 | Magic header "Qspt1WmJOL" |
| 0x0a | 1 | Packet type |
| 0x0b | 20 | Device name |
| 0x21 | 1 | Device number |

### Pitch Values

```cpp
// Pitch encoding:
// 0x000000 = stopped (0%)
// 0x100000 = normal speed (100%)
// 0x200000 = double speed (200%)

double percentage = (pitch - 0x100000) * 100.0 / 0x100000;
double multiplier = pitch / static_cast<double>(0x100000);
```

### References

- [DJ Link Analysis](https://djl-analysis.deepsymmetry.org/) - Comprehensive protocol documentation
- [Original Beat Link (Java)](https://github.com/Deep-Symmetry/beat-link)
- [Crate Digger](https://github.com/Deep-Symmetry/crate-digger) - Database parsing

---

## Examples

### C++ - Full Metadata Retrieval

```cpp
#include <beatlink/BeatLink.hpp>
#include <iostream>
#include <thread>

using namespace beatlink;

int main() {
    // Start all services
    DeviceFinder::getInstance().start();
    VirtualCdj::getInstance().start();
    data::MetadataFinder::getInstance().start();
    data::BeatGridFinder::getInstance().start();

    std::this_thread::sleep_for(std::chrono::seconds(5));

    // Get metadata for player 1
    auto metadata = data::MetadataFinder::getInstance().getLatestMetadataFor(1);
    if (metadata) {
        std::cout << "Title: " << metadata->getTitle() << "\n";
        std::cout << "Artist: " << metadata->getArtist() << "\n";
        std::cout << "BPM: " << metadata->getTempo() / 100.0 << "\n";

        auto cueList = metadata->getCueList();
        if (cueList) {
            for (const auto& cue : cueList->getEntries()) {
                std::cout << "Cue @ " << cue.cueTimeMs << "ms\n";
            }
        }
    }

    // Cleanup
    data::BeatGridFinder::getInstance().stop();
    data::MetadataFinder::getInstance().stop();
    VirtualCdj::getInstance().stop();
    DeviceFinder::getInstance().stop();
    return 0;
}
```

### Python Scripts

See the `examples/` directory:

| Script | Description |
|--------|-------------|
| `beat_monitor.py` | Real-time beat visualization |
| `track_info.py` | Display track metadata, cue points, beat grid |
| `waveform_export.py` | Export waveform and album art to files |

---

## Testing

### C++ Unit Tests

```bash
cmake .. -DBEATLINK_BUILD_TESTS=ON
make beatlink_tests
./beatlink_tests             # Run all tests
./beatlink_tests [Beat]      # Run Beat tests only
ctest --output-on-failure    # Run via CTest
```

### Python Tests

```bash
cd tests
python golden_test.py           # API and CLI tests
python communication_test.py    # Network tests
python real_device_test.py      # Device integration tests
```

### Generate Documentation

```bash
doxygen Doxyfile
open docs/api/html/index.html
```

---

## Troubleshooting

### Port Already in Use

```
Failed to start DeviceFinder (port 50000 may be in use)
```

**Solution**: Close rekordbox or other DJ Link applications.

```bash
# Check what's using the port
lsof -i :50000
```

### Devices Not Found

1. Ensure PC and DJ equipment are on the same network subnet
2. Check firewall settings (allow UDP 50000-50002, TCP 12523)
3. Verify DJ Link is enabled on the equipment

### macOS Network Permission

On macOS, allow network access when prompted.

### Build Errors

```bash
# If Asio not found, install manually:
brew install asio          # macOS
apt install libasio-dev    # Ubuntu/Debian
```

---

## Supported Equipment

### Tested

- CDJ-2000NXS2, CDJ-3000
- XDJ-1000MK2, XDJ-XZ, XDJ-RX3
- DJM-900NXS2, DJM-V10
- Opus Quad (partial support)

### Should Work

- All DJ Link compatible Pioneer DJ equipment
- rekordbox (PC/Mac)

---

## License

This project is licensed under the **Eclipse Public License v2.0** (EPL-2.0), the same license as the original Java Beat Link library.

**Original Java Implementation:**
Copyright (c) 2016-2024 [Deep Symmetry, LLC](https://deepsymmetry.org)

**C++20 Port:**
Copyright (c) 2024-2026 [Daito Manabe](https://daito.ws) / [Rhizomatiks](https://rhizomatiks.com)

See [LICENSE.md](LICENSE.md) for the full license text.

---

## Acknowledgments

- **James Elliott** and **Deep Symmetry** for the original [Beat Link](https://github.com/Deep-Symmetry/beat-link) library
- The [DJ Link Analysis](https://djl-analysis.deepsymmetry.org/) documentation
- The reverse engineering community

---

## Version History

| Version | Date | Changes |
|---------|------|---------|
| 0.2.0 | 2026-01 | Python bindings (105 API), unit tests, documentation |
| 0.1.0 | 2024-12 | Initial release: DeviceFinder, BeatFinder, GUI monitor |
