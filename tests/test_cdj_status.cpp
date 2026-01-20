/**
 * @file test_cdj_status.cpp
 * @brief Unit tests for CdjStatus packet parsing
 */

#include <catch2/catch_all.hpp>
#include <beatlink/CdjStatus.hpp>
#include <array>

using namespace beatlink;

// Sample CDJ status packet (minimum 204 bytes)
static std::array<uint8_t, 212> createCdjStatusPacket(
    uint8_t deviceNumber,
    int bpm100,
    int pitch,
    uint8_t beatWithinBar,
    bool playing,
    bool master,
    bool synced,
    bool onAir,
    TrackSourceSlot slot = TrackSourceSlot::USB_SLOT,
    TrackType trackType = TrackType::REKORDBOX,
    int rekordboxId = 12345
) {
    std::array<uint8_t, 212> packet{};

    // Magic header "Qspt1WmJOL" (10 bytes)
    const char* magic = "Qspt1WmJOL";
    std::memcpy(packet.data(), magic, 10);

    // Packet type = 0x0a (status update) at offset 0x0a
    packet[0x0a] = 0x0a;

    // Device name at offset 0x0b (20 bytes)
    std::memcpy(&packet[0x0b], "CDJ-2000NXS2", 12);

    // Device number at offset 0x21
    packet[0x21] = deviceNumber;

    // Track source player at offset 0x28
    packet[0x28] = deviceNumber;

    // Track source slot at offset 0x29
    packet[0x29] = static_cast<uint8_t>(slot);

    // Track type at offset 0x2a
    packet[0x2a] = static_cast<uint8_t>(trackType);

    // Rekordbox ID at offset 0x2c (4 bytes, big-endian)
    packet[0x2c] = (rekordboxId >> 24) & 0xff;
    packet[0x2d] = (rekordboxId >> 16) & 0xff;
    packet[0x2e] = (rekordboxId >> 8) & 0xff;
    packet[0x2f] = rekordboxId & 0xff;

    // Track number at offset 0x32 (2 bytes)
    packet[0x32] = 0x00;
    packet[0x33] = 0x01;

    // Play state 1 at offset 0x7b
    if (playing) {
        packet[0x7b] = 0x03; // PLAYING
    } else {
        packet[0x7b] = 0x06; // CUED
    }

    // Sync number at offset 0x84 (4 bytes)
    packet[0x84] = 0x00;
    packet[0x85] = 0x00;
    packet[0x86] = 0x00;
    packet[0x87] = 0x01;

    // Status flags at offset 0x89
    uint8_t flags = 0;
    if (playing) flags |= CdjStatus::PLAYING_FLAG;
    if (master) flags |= CdjStatus::MASTER_FLAG;
    if (synced) flags |= CdjStatus::SYNCED_FLAG;
    if (onAir) flags |= CdjStatus::ON_AIR_FLAG;
    packet[0x89] = flags;

    // Play state 2 at offset 0x8b
    packet[0x8b] = playing ? 0x7a : 0x7e; // MOVING or STOPPED

    // Pitch at offset 0x8d (4 bytes, big-endian)
    packet[0x8d] = (pitch >> 24) & 0xff;
    packet[0x8e] = (pitch >> 16) & 0xff;
    packet[0x8f] = (pitch >> 8) & 0xff;
    packet[0x90] = pitch & 0xff;

    // BPM at offset 0x92 (2 bytes, big-endian)
    packet[0x92] = (bpm100 >> 8) & 0xff;
    packet[0x93] = bpm100 & 0xff;

    // Play state 3 at offset 0x9d
    packet[0x9d] = playing ? 0x0d : 0x01; // FORWARD_CDJ or PAUSED_OR_REVERSE

    // Master handoff at offset 0x9f
    packet[0x9f] = 0xff; // No handoff

    // Beat number at offset 0xa0 (4 bytes)
    packet[0xa0] = 0x00;
    packet[0xa1] = 0x00;
    packet[0xa2] = 0x00;
    packet[0xa3] = 0x20; // Beat 32

    // Beat within bar at offset 0xa6
    packet[0xa6] = beatWithinBar;

    // Packet counter at offset 0xc8 (4 bytes)
    packet[0xc8] = 0x00;
    packet[0xc9] = 0x00;
    packet[0xc9] = 0x00;
    packet[0xcb] = 0x01;

    return packet;
}

TEST_CASE("CdjStatus::create with valid packet", "[CdjStatus]") {
    auto packet = createCdjStatusPacket(1, 12850, 0x100000, 1, true, false, true, true);
    auto senderAddress = asio::ip::address_v4::from_string("192.168.1.10");

    auto status = CdjStatus::create(std::span<const uint8_t>(packet), senderAddress);

    REQUIRE(status.has_value());
    CHECK(status->getDeviceNumber() == 1);
    CHECK(status->getBpm() == 12850);
    CHECK(status->getPitch() == 0x100000);
    CHECK(status->getBeatWithinBar() == 1);
}

TEST_CASE("CdjStatus::create with invalid packet size", "[CdjStatus]") {
    std::array<uint8_t, 100> shortPacket{}; // Too short (min 204)
    auto senderAddress = asio::ip::address_v4::from_string("192.168.1.10");

    auto status = CdjStatus::create(std::span<const uint8_t>(shortPacket), senderAddress);

    REQUIRE_FALSE(status.has_value());
}

TEST_CASE("CdjStatus play state flags", "[CdjStatus]") {
    auto senderAddress = asio::ip::address_v4::from_string("192.168.1.10");

    SECTION("Playing") {
        auto packet = createCdjStatusPacket(1, 12000, 0x100000, 1, true, false, false, false);
        auto status = CdjStatus::create(std::span<const uint8_t>(packet), senderAddress);
        REQUIRE(status.has_value());
        CHECK(status->isPlaying() == true);
        CHECK(status->getPlayState() == PlayState::PLAYING);
    }

    SECTION("Cued (not playing)") {
        auto packet = createCdjStatusPacket(1, 12000, 0x100000, 1, false, false, false, false);
        auto status = CdjStatus::create(std::span<const uint8_t>(packet), senderAddress);
        REQUIRE(status.has_value());
        CHECK(status->isPlaying() == false);
        CHECK(status->getPlayState() == PlayState::CUED);
        CHECK(status->isAtCue() == true);
    }
}

TEST_CASE("CdjStatus master and sync flags", "[CdjStatus]") {
    auto senderAddress = asio::ip::address_v4::from_string("192.168.1.10");

    SECTION("Tempo master") {
        auto packet = createCdjStatusPacket(1, 12000, 0x100000, 1, true, true, false, false);
        auto status = CdjStatus::create(std::span<const uint8_t>(packet), senderAddress);
        REQUIRE(status.has_value());
        CHECK(status->isTempoMaster() == true);
        CHECK(status->isSynced() == false);
    }

    SECTION("Synced slave") {
        auto packet = createCdjStatusPacket(2, 12000, 0x100000, 1, true, false, true, false);
        auto status = CdjStatus::create(std::span<const uint8_t>(packet), senderAddress);
        REQUIRE(status.has_value());
        CHECK(status->isTempoMaster() == false);
        CHECK(status->isSynced() == true);
    }

    SECTION("Master and synced") {
        auto packet = createCdjStatusPacket(1, 12000, 0x100000, 1, true, true, true, false);
        auto status = CdjStatus::create(std::span<const uint8_t>(packet), senderAddress);
        REQUIRE(status.has_value());
        CHECK(status->isTempoMaster() == true);
        CHECK(status->isSynced() == true);
    }
}

TEST_CASE("CdjStatus on-air flag", "[CdjStatus]") {
    auto senderAddress = asio::ip::address_v4::from_string("192.168.1.10");

    SECTION("On air") {
        auto packet = createCdjStatusPacket(1, 12000, 0x100000, 1, true, false, false, true);
        auto status = CdjStatus::create(std::span<const uint8_t>(packet), senderAddress);
        REQUIRE(status.has_value());
        CHECK(status->isOnAir() == true);
    }

    SECTION("Not on air") {
        auto packet = createCdjStatusPacket(1, 12000, 0x100000, 1, true, false, false, false);
        auto status = CdjStatus::create(std::span<const uint8_t>(packet), senderAddress);
        REQUIRE(status.has_value());
        CHECK(status->isOnAir() == false);
    }
}

TEST_CASE("CdjStatus track source information", "[CdjStatus]") {
    auto senderAddress = asio::ip::address_v4::from_string("192.168.1.10");

    SECTION("USB slot") {
        auto packet = createCdjStatusPacket(1, 12000, 0x100000, 1, true, false, false, false,
                                            TrackSourceSlot::USB_SLOT, TrackType::REKORDBOX, 12345);
        auto status = CdjStatus::create(std::span<const uint8_t>(packet), senderAddress);
        REQUIRE(status.has_value());
        CHECK(status->getTrackSourceSlot() == TrackSourceSlot::USB_SLOT);
        CHECK(status->getTrackType() == TrackType::REKORDBOX);
        CHECK(status->getRekordboxId() == 12345);
        CHECK(status->isTrackLoaded() == true);
    }

    SECTION("SD slot") {
        auto packet = createCdjStatusPacket(1, 12000, 0x100000, 1, true, false, false, false,
                                            TrackSourceSlot::SD_SLOT, TrackType::REKORDBOX, 54321);
        auto status = CdjStatus::create(std::span<const uint8_t>(packet), senderAddress);
        REQUIRE(status.has_value());
        CHECK(status->getTrackSourceSlot() == TrackSourceSlot::SD_SLOT);
        CHECK(status->getRekordboxId() == 54321);
    }

    SECTION("No track") {
        auto packet = createCdjStatusPacket(1, 0, 0x100000, 0, false, false, false, false,
                                            TrackSourceSlot::NO_TRACK, TrackType::NO_TRACK, 0);
        auto status = CdjStatus::create(std::span<const uint8_t>(packet), senderAddress);
        REQUIRE(status.has_value());
        CHECK(status->getTrackSourceSlot() == TrackSourceSlot::NO_TRACK);
        CHECK(status->getTrackType() == TrackType::NO_TRACK);
        CHECK(status->isTrackLoaded() == false);
    }
}

TEST_CASE("CdjStatus effective tempo calculation", "[CdjStatus]") {
    auto senderAddress = asio::ip::address_v4::from_string("192.168.1.10");

    SECTION("Normal pitch") {
        auto packet = createCdjStatusPacket(1, 12800, 0x100000, 1, true, false, false, false);
        auto status = CdjStatus::create(std::span<const uint8_t>(packet), senderAddress);
        REQUIRE(status.has_value());
        CHECK(status->getEffectiveTempo() == Catch::Approx(128.0).epsilon(0.01));
    }

    SECTION("Slower pitch") {
        // -4% pitch = 0x100000 * 0.96 = 0x0F5C28
        auto packet = createCdjStatusPacket(1, 12800, 0x0F5C28, 1, true, false, false, false);
        auto status = CdjStatus::create(std::span<const uint8_t>(packet), senderAddress);
        REQUIRE(status.has_value());
        CHECK(status->getEffectiveTempo() == Catch::Approx(122.88).epsilon(0.5));
    }
}

TEST_CASE("CdjStatus beat within bar", "[CdjStatus]") {
    auto senderAddress = asio::ip::address_v4::from_string("192.168.1.10");

    for (uint8_t beat = 1; beat <= 4; ++beat) {
        auto packet = createCdjStatusPacket(1, 12000, 0x100000, beat, true, false, false, false);
        auto status = CdjStatus::create(std::span<const uint8_t>(packet), senderAddress);
        REQUIRE(status.has_value());
        CHECK(status->getBeatWithinBar() == beat);
        CHECK(status->isBeatWithinBarMeaningful() == true);
    }
}
