/**
 * @file test_beat.cpp
 * @brief Unit tests for Beat packet parsing
 */

#include <catch2/catch_all.hpp>
#include <beatlink/Beat.hpp>
#include <array>

using namespace beatlink;

// Sample beat packet (96 bytes) based on DJ Link protocol
// This represents a typical beat announcement packet
static std::array<uint8_t, 96> createBeatPacket(
    uint8_t deviceNumber,
    int bpm100,           // BPM * 100 (e.g., 12850 = 128.50 BPM)
    int pitch,            // Pitch value (0x100000 = normal)
    uint8_t beatWithinBar // 1-4
) {
    std::array<uint8_t, 96> packet{};

    // Magic header "Qspt1WmJOL" (10 bytes)
    const char* magic = "Qspt1WmJOL";
    std::memcpy(packet.data(), magic, 10);

    // Packet type = 0x28 (beat) at offset 0x0a
    packet[0x0a] = 0x28;

    // Device name at offset 0x0b (20 bytes) - pad with zeros
    std::memcpy(&packet[0x0b], "CDJ-2000NXS2", 12);

    // Device number at offset 0x21
    packet[0x21] = deviceNumber;

    // Next beat timing at offset 0x24 (4 bytes, big-endian)
    uint32_t nextBeat = 500; // 500ms to next beat
    packet[0x24] = (nextBeat >> 24) & 0xff;
    packet[0x25] = (nextBeat >> 16) & 0xff;
    packet[0x26] = (nextBeat >> 8) & 0xff;
    packet[0x27] = nextBeat & 0xff;

    // Second beat at offset 0x28
    uint32_t secondBeat = 1000;
    packet[0x28] = (secondBeat >> 24) & 0xff;
    packet[0x29] = (secondBeat >> 16) & 0xff;
    packet[0x2a] = (secondBeat >> 8) & 0xff;
    packet[0x2b] = secondBeat & 0xff;

    // Next bar at offset 0x2c
    uint32_t nextBar = 2000;
    packet[0x2c] = (nextBar >> 24) & 0xff;
    packet[0x2d] = (nextBar >> 16) & 0xff;
    packet[0x2e] = (nextBar >> 8) & 0xff;
    packet[0x2f] = nextBar & 0xff;

    // Fourth beat at offset 0x30
    uint32_t fourthBeat = 1500;
    packet[0x30] = (fourthBeat >> 24) & 0xff;
    packet[0x31] = (fourthBeat >> 16) & 0xff;
    packet[0x32] = (fourthBeat >> 8) & 0xff;
    packet[0x33] = fourthBeat & 0xff;

    // Second bar at offset 0x34
    uint32_t secondBar = 4000;
    packet[0x34] = (secondBar >> 24) & 0xff;
    packet[0x35] = (secondBar >> 16) & 0xff;
    packet[0x36] = (secondBar >> 8) & 0xff;
    packet[0x37] = secondBar & 0xff;

    // Eighth beat at offset 0x38
    uint32_t eighthBeat = 3500;
    packet[0x38] = (eighthBeat >> 24) & 0xff;
    packet[0x39] = (eighthBeat >> 16) & 0xff;
    packet[0x3a] = (eighthBeat >> 8) & 0xff;
    packet[0x3b] = eighthBeat & 0xff;

    // Pitch at offset 0x55 (3 bytes, big-endian)
    packet[0x55] = (pitch >> 16) & 0xff;
    packet[0x56] = (pitch >> 8) & 0xff;
    packet[0x57] = pitch & 0xff;

    // BPM at offset 0x5a (2 bytes, big-endian)
    packet[0x5a] = (bpm100 >> 8) & 0xff;
    packet[0x5b] = bpm100 & 0xff;

    // Beat within bar at offset 0x5c
    packet[0x5c] = beatWithinBar;

    return packet;
}

TEST_CASE("Beat::create with valid packet", "[Beat]") {
    auto packet = createBeatPacket(1, 12850, 0x100000, 1);
    auto senderAddress = asio::ip::address_v4::from_string("192.168.1.10");

    auto beat = Beat::create(std::span<const uint8_t>(packet), senderAddress);

    REQUIRE(beat.has_value());
    CHECK(beat->getDeviceNumber() == 1);
    CHECK(beat->getBpm() == 12850);
    CHECK(beat->getPitch() == 0x100000);
    CHECK(beat->getBeatWithinBar() == 1);
}

TEST_CASE("Beat::create with invalid packet size", "[Beat]") {
    std::array<uint8_t, 50> shortPacket{}; // Too short
    auto senderAddress = asio::ip::address_v4::from_string("192.168.1.10");

    auto beat = Beat::create(std::span<const uint8_t>(shortPacket), senderAddress);

    REQUIRE_FALSE(beat.has_value());
}

TEST_CASE("Beat timing values", "[Beat]") {
    auto packet = createBeatPacket(2, 12000, 0x100000, 3);
    auto senderAddress = asio::ip::address_v4::from_string("192.168.1.10");

    auto beat = Beat::create(std::span<const uint8_t>(packet), senderAddress);
    REQUIRE(beat.has_value());

    CHECK(beat->getNextBeat() == 500);
    CHECK(beat->getSecondBeat() == 1000);
    CHECK(beat->getNextBar() == 2000);
    CHECK(beat->getFourthBeat() == 1500);
    CHECK(beat->getSecondBar() == 4000);
    CHECK(beat->getEighthBeat() == 3500);
}

TEST_CASE("Beat effective tempo calculation", "[Beat]") {
    SECTION("Normal pitch (100%)") {
        auto packet = createBeatPacket(1, 12000, 0x100000, 1);
        auto senderAddress = asio::ip::address_v4::from_string("192.168.1.10");
        auto beat = Beat::create(std::span<const uint8_t>(packet), senderAddress);

        REQUIRE(beat.has_value());
        CHECK(beat->getEffectiveTempo() == Catch::Approx(120.0).epsilon(0.01));
    }

    SECTION("Pitch +8%") {
        // Pitch for +8% = 0x100000 * 1.08 ≈ 0x115C28
        auto packet = createBeatPacket(1, 12000, 0x115C28, 1);
        auto senderAddress = asio::ip::address_v4::from_string("192.168.1.10");
        auto beat = Beat::create(std::span<const uint8_t>(packet), senderAddress);

        REQUIRE(beat.has_value());
        CHECK(beat->getEffectiveTempo() == Catch::Approx(129.6).epsilon(0.5));
    }
}

TEST_CASE("Beat within bar meaningful check", "[Beat]") {
    auto senderAddress = asio::ip::address_v4::from_string("192.168.1.10");

    SECTION("Player device (< 33)") {
        auto packet = createBeatPacket(1, 12000, 0x100000, 2);
        auto beat = Beat::create(std::span<const uint8_t>(packet), senderAddress);
        REQUIRE(beat.has_value());
        CHECK(beat->isBeatWithinBarMeaningful() == true);
    }

    SECTION("Mixer device (>= 33)") {
        auto packet = createBeatPacket(33, 12000, 0x100000, 2);
        auto beat = Beat::create(std::span<const uint8_t>(packet), senderAddress);
        REQUIRE(beat.has_value());
        CHECK(beat->isBeatWithinBarMeaningful() == false);
    }
}

TEST_CASE("Beat device number range", "[Beat]") {
    auto senderAddress = asio::ip::address_v4::from_string("192.168.1.10");

    for (uint8_t deviceNum : {1, 2, 3, 4, 5, 6, 33, 34}) {
        auto packet = createBeatPacket(deviceNum, 12000, 0x100000, 1);
        auto beat = Beat::create(std::span<const uint8_t>(packet), senderAddress);

        REQUIRE(beat.has_value());
        CHECK(beat->getDeviceNumber() == deviceNum);
    }
}
