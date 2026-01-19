# Beat Link C++ Manual (English)

Beat Link C++ is a C++20 implementation of the Pioneer DJ Link protocol library.
For the full Japanese manual, see `README.md`.

## Contents

1. Requirements
2. Build
3. Binaries
4. Examples
5. Quickstart (CLI/GUI)
6. Library Usage
7. Flow Diagrams
8. Troubleshooting
9. License

## Requirements

- OS: macOS, Linux, Windows
- Compiler: C++20 (Clang, GCC, MSVC)
- CMake: 3.15+
- Network: same subnet as DJ Link devices

### Dependencies

These are fetched automatically by CMake if not found locally:

- Asio (standalone)
- GLFW (GUI)
- Dear ImGui (GUI)

## Build

```bash
mkdir build
cd build
cmake ..
cmake --build . -j
```

## Binaries

After building, you will get:

- `libbeatlink.a`
- `beatlink_example` (CLI sample)
- `beatlink_cli` (schema/listen utility)
- `beatlink_gui` (ImGui monitor)

## Examples

```bash
./beatlink_example
./beatlink_cli --schema
./beatlink_cli --listen --human
./beatlink_gui
```

## Quickstart (CLI/GUI)

### CLI

```bash
# Output API schema as JSON
./beatlink_cli --schema

# Listen for devices and beats (human-readable)
./beatlink_cli --listen --human
```

Tips:
- Use Ctrl+C to stop.
- If startup fails, check that ports 50000-50002 are free.

### GUI

```bash
./beatlink_gui
```

Tips:
- Allow network access when prompted by the OS.
- The GUI starts metadata/time services after the first device is discovered.

## Library Usage

Minimal beat listener:

```cpp
#include <chrono>
#include <iostream>
#include <thread>

#include <beatlink/BeatLink.hpp>

int main() {
    auto& deviceFinder = beatlink::DeviceFinder::getInstance();
    auto& beatFinder = beatlink::BeatFinder::getInstance();

    deviceFinder.addDeviceFoundListener([](const beatlink::DeviceAnnouncement& device) {
        std::cout << "Found: " << device.getDeviceName() << std::endl;
    });

    beatFinder.addBeatListener([](const beatlink::Beat& beat) {
        std::cout << "BPM: " << beat.getEffectiveTempo() << std::endl;
    });

    deviceFinder.start();
    beatFinder.start();

    while (true) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}
```

## Flow Diagrams

Startup order and runtime flows are documented here:

- `../../docs/flow-diagrams.md`

## Troubleshooting

### Port already in use

If ports 50000-50002 are in use, stop rekordbox or other DJ Link apps.

### No devices found

- Ensure the computer and DJ devices are on the same subnet
- Check firewall rules for UDP 50000-50002

## License

EPL-2.0. See `LICENSE.md`.
This library is a C++ port of Deep Symmetry Beat Link.
