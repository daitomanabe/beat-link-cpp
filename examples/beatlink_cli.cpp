/**
 * Beat Link C++ CLI - Command-Line Interface
 *
 * Per INTRODUCTION_JAVA_TO_CPP.md Section 7 (Deliverables):
 * - CLI Tool with JSONL I/O
 * - --schema support for AI agent introspection
 *
 * Usage:
 *   beatlink_cli --schema           # Output API schema as JSON
 *   beatlink_cli --listen           # Listen for devices and beats (JSONL output)
 *   beatlink_cli --help             # Show help
 */

#include <iostream>
#include <string>
#include <csignal>
#include <atomic>
#include <chrono>
#include <thread>
#include <cstring>

#include <beatlink/BeatLink.hpp>

// Global running flag for signal handling
std::atomic<bool> running{true};

void signalHandler(int) {
    running = false;
}

void printHelp() {
    std::cout << R"(
Beat Link C++ CLI v)" << beatlink::Version::STRING << R"(

Usage:
  beatlink_cli [OPTIONS]

Options:
  --schema        Output API schema as JSON (for AI agent introspection)
  --listen        Listen for DJ Link devices and beats (JSONL output)
  --json          Output in JSON format (default for --listen)
  --human         Output in human-readable format
  --help, -h      Show this help message

Examples:
  beatlink_cli --schema
  beatlink_cli --listen
  beatlink_cli --listen --human

)" << std::endl;
}

void printSchema() {
    // Output the API schema for AI agent consumption
    std::cout << beatlink::describe_api_json() << std::endl;
}

int64_t getCurrentTimeMs() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();
}

void listenJsonl() {
    auto& deviceFinder = beatlink::DeviceFinder::getInstance();
    auto& beatFinder = beatlink::BeatFinder::getInstance();

    // Set up device found listener (JSONL output)
    deviceFinder.addDeviceFoundListener([](const beatlink::DeviceAnnouncement& device) {
        std::cout << std::format(
            R"({{"event":"device_found","timestamp_ms":{},"device":{{"number":{},"name":"{}","address":"{}"}}}})",
            std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()
            ).count(),
            device.getDeviceNumber(),
            device.getDeviceName(),
            device.getAddress().to_string()
        ) << std::endl;
    });

    // Set up device lost listener (JSONL output)
    deviceFinder.addDeviceLostListener([](const beatlink::DeviceAnnouncement& device) {
        std::cout << std::format(
            R"({{"event":"device_lost","timestamp_ms":{},"device":{{"number":{},"name":"{}"}}}})",
            std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()
            ).count(),
            device.getDeviceNumber(),
            device.getDeviceName()
        ) << std::endl;
    });

    // Set up beat listener (JSONL output)
    beatFinder.addBeatListener([](const beatlink::Beat& beat) {
        // Apply safety curtain to values
        double safeBpm = beatlink::SafetyCurtain::sanitizeBpm(beat.getEffectiveTempo());
        double safePitch = beatlink::SafetyCurtain::sanitizePitchPercent(
            beatlink::Util::pitchToPercentage(beat.getPitch())
        );
        int safeBeat = beatlink::SafetyCurtain::sanitizeBeat(beat.getBeatWithinBar());

        std::cout << std::format(
            R"({{"event":"beat","timestamp_ms":{},"device":{},"bpm":{:.2f},"pitch_percent":{:.2f},"beat_in_bar":{}}})",
            std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()
            ).count(),
            beat.getDeviceNumber(),
            safeBpm,
            safePitch,
            safeBeat
        ) << std::endl;
    });

    // Start finders
    if (!deviceFinder.start()) {
        std::cerr << std::format(
            R"({{"event":"error","timestamp_ms":{},"message":"Failed to start DeviceFinder","source":"beatlink_cli.cpp:{}}})",
            getCurrentTimeMs(),
            __LINE__
        ) << std::endl;
        return;
    }

    std::cout << std::format(
        R"({{"event":"started","timestamp_ms":{},"component":"DeviceFinder","port":{}}})",
        getCurrentTimeMs(),
        beatlink::Ports::ANNOUNCEMENT
    ) << std::endl;

    if (!beatFinder.start()) {
        std::cerr << std::format(
            R"({{"event":"error","timestamp_ms":{},"message":"Failed to start BeatFinder","source":"beatlink_cli.cpp:{}}})",
            getCurrentTimeMs(),
            __LINE__
        ) << std::endl;
        deviceFinder.stop();
        return;
    }

    std::cout << std::format(
        R"({{"event":"started","timestamp_ms":{},"component":"BeatFinder","port":{}}})",
        getCurrentTimeMs(),
        beatlink::Ports::BEAT
    ) << std::endl;

    // Main loop
    while (running) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    // Cleanup
    beatFinder.stop();
    deviceFinder.stop();

    std::cout << std::format(
        R"({{"event":"stopped","timestamp_ms":{}}})",
        getCurrentTimeMs()
    ) << std::endl;
}

void listenHuman() {
    auto& deviceFinder = beatlink::DeviceFinder::getInstance();
    auto& beatFinder = beatlink::BeatFinder::getInstance();

    std::cout << "Beat Link C++ CLI v" << beatlink::Version::STRING << std::endl;
    std::cout << "Listening for DJ Link devices... Press Ctrl+C to stop." << std::endl;
    std::cout << std::endl;

    // Set up device found listener
    deviceFinder.addDeviceFoundListener([](const beatlink::DeviceAnnouncement& device) {
        std::cout << "[DEVICE+] " << device.getDeviceName()
                  << " (#" << device.getDeviceNumber() << ")"
                  << " @ " << device.getAddress().to_string()
                  << std::endl;
    });

    // Set up device lost listener
    deviceFinder.addDeviceLostListener([](const beatlink::DeviceAnnouncement& device) {
        std::cout << "[DEVICE-] " << device.getDeviceName()
                  << " (#" << device.getDeviceNumber() << ")"
                  << std::endl;
    });

    // Set up beat listener
    beatFinder.addBeatListener([](const beatlink::Beat& beat) {
        double bpm = beatlink::SafetyCurtain::sanitizeBpm(beat.getEffectiveTempo());
        double pitch = beatlink::SafetyCurtain::sanitizePitchPercent(
            beatlink::Util::pitchToPercentage(beat.getPitch())
        );
        int beatInBar = beatlink::SafetyCurtain::sanitizeBeat(beat.getBeatWithinBar());

        std::cout << std::format(
            "[BEAT] Device {:2d} | {:6.1f} BPM | Beat {:d}/4 | Pitch {:+6.2f}%",
            beat.getDeviceNumber(),
            bpm,
            beatInBar,
            pitch
        ) << std::endl;
    });

    // Start finders
    if (!deviceFinder.start()) {
        std::cerr << "ERROR: Failed to start DeviceFinder (port "
                  << beatlink::Ports::ANNOUNCEMENT << " in use?)" << std::endl;
        return;
    }
    std::cout << "DeviceFinder started on port " << beatlink::Ports::ANNOUNCEMENT << std::endl;

    if (!beatFinder.start()) {
        std::cerr << "ERROR: Failed to start BeatFinder (port "
                  << beatlink::Ports::BEAT << " in use?)" << std::endl;
        deviceFinder.stop();
        return;
    }
    std::cout << "BeatFinder started on port " << beatlink::Ports::BEAT << std::endl;
    std::cout << std::endl;

    // Main loop
    while (running) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    // Cleanup
    std::cout << std::endl;
    std::cout << "Stopping..." << std::endl;
    beatFinder.stop();
    deviceFinder.stop();
    std::cout << "Goodbye!" << std::endl;
}

int main(int argc, char* argv[]) {
    std::signal(SIGINT, signalHandler);

    bool showSchema = false;
    bool listen = false;
    bool humanReadable = false;

    // Parse arguments
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--schema") == 0) {
            showSchema = true;
        } else if (std::strcmp(argv[i], "--listen") == 0) {
            listen = true;
        } else if (std::strcmp(argv[i], "--json") == 0) {
            humanReadable = false;
        } else if (std::strcmp(argv[i], "--human") == 0) {
            humanReadable = true;
        } else if (std::strcmp(argv[i], "--help") == 0 || std::strcmp(argv[i], "-h") == 0) {
            printHelp();
            return 0;
        } else {
            std::cerr << "Unknown option: " << argv[i] << std::endl;
            printHelp();
            return 1;
        }
    }

    // Handle commands
    if (showSchema) {
        printSchema();
        return 0;
    }

    if (listen) {
        if (humanReadable) {
            listenHuman();
        } else {
            listenJsonl();
        }
        return 0;
    }

    // Default: show help
    printHelp();
    return 0;
}
