/**
 * Beat Link C++ Example - Simple Beat Listener
 *
 * This example demonstrates how to:
 * 1. Discover DJ Link devices on the network
 * 2. Listen for beat packets from players
 * 3. Display BPM and beat information
 */

#include <iostream>
#include <iomanip>
#include <chrono>
#include <thread>
#include <csignal>
#include <atomic>

#include <beatlink/BeatLink.hpp>

std::atomic<bool> running{true};

void signalHandler(int) {
    std::cout << "\nShutting down..." << std::endl;
    running = false;
}

int main() {
    std::cout << "Beat Link C++ v" << beatlink::Version::STRING << std::endl;
    std::cout << "==============================" << std::endl;
    std::cout << "Listening for DJ Link devices..." << std::endl;
    std::cout << std::endl;

    // Set up signal handler for clean shutdown
    std::signal(SIGINT, signalHandler);

    // Get singleton instances
    auto& deviceFinder = beatlink::DeviceFinder::getInstance();
    auto& beatFinder = beatlink::BeatFinder::getInstance();

    // Set up device found listener
    deviceFinder.addDeviceFoundListener([](const beatlink::DeviceAnnouncement& device) {
        std::cout << "[DEVICE FOUND] " << device.toString() << std::endl;
    });

    // Set up device lost listener
    deviceFinder.addDeviceLostListener([](const beatlink::DeviceAnnouncement& device) {
        std::cout << "[DEVICE LOST] " << device.toString() << std::endl;
    });

    // Set up beat listener
    beatFinder.addBeatListener([](const beatlink::Beat& beat) {
        std::cout << std::fixed << std::setprecision(1);
        std::cout << "[BEAT] Device " << beat.getDeviceNumber()
                  << " | BPM: " << std::setw(6) << beat.getEffectiveTempo()
                  << " | Beat: " << beat.getBeatWithinBar() << "/4"
                  << " | Pitch: " << std::showpos << std::setprecision(2)
                  << beatlink::Util::pitchToPercentage(beat.getPitch()) << "%"
                  << std::noshowpos << std::endl;
    });

    // Start device finder
    if (!deviceFinder.start()) {
        std::cerr << "Failed to start DeviceFinder (port 50000 may be in use)" << std::endl;
        return 1;
    }
    std::cout << "DeviceFinder started on port " << beatlink::Ports::ANNOUNCEMENT << std::endl;

    // Start beat finder
    if (!beatFinder.start()) {
        std::cerr << "Failed to start BeatFinder (port 50001 may be in use)" << std::endl;
        deviceFinder.stop();
        return 1;
    }
    std::cout << "BeatFinder started on port " << beatlink::Ports::BEAT << std::endl;

    std::cout << std::endl;
    std::cout << "Waiting for devices and beats... Press Ctrl+C to exit." << std::endl;
    std::cout << std::endl;

    // Main loop
    while (running) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    // Clean shutdown
    beatFinder.stop();
    deviceFinder.stop();

    std::cout << "Goodbye!" << std::endl;
    return 0;
}
