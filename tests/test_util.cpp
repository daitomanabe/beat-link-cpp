/**
 * @file test_util.cpp
 * @brief Unit tests for Util functions
 */

#include <catch2/catch_all.hpp>
#include <beatlink/Util.hpp>
#include <array>
#include <cstring>

using namespace beatlink;

TEST_CASE("Util::bytesToNumber", "[Util]") {
    SECTION("2 bytes big-endian") {
        uint8_t data[] = {0x12, 0x34};
        CHECK(Util::bytesToNumber(data, 0, 2) == 0x1234);
    }

    SECTION("4 bytes big-endian") {
        uint8_t data[] = {0x12, 0x34, 0x56, 0x78};
        CHECK(Util::bytesToNumber(data, 0, 4) == 0x12345678);
    }

    SECTION("With offset") {
        uint8_t data[] = {0x00, 0x00, 0xAB, 0xCD, 0xEF, 0x12};
        CHECK(Util::bytesToNumber(data, 2, 4) == 0xABCDEF12);
    }

    SECTION("Single byte") {
        uint8_t data[] = {0xFF};
        CHECK(Util::bytesToNumber(data, 0, 1) == 0xFF);
    }

    SECTION("3 bytes") {
        uint8_t data[] = {0xAB, 0xCD, 0xEF};
        CHECK(Util::bytesToNumber(data, 0, 3) == 0xABCDEF);
    }
}

TEST_CASE("Util::pitchToPercentage", "[Util]") {
    SECTION("Normal pitch (0%)") {
        // Normal pitch is 0x100000 (1048576)
        CHECK(Util::pitchToPercentage(0x100000) == Catch::Approx(0.0).margin(0.01));
    }

    SECTION("+8% pitch") {
        // +8% = 0x100000 * 1.08 ≈ 1132462
        int pitch8percent = static_cast<int>(0x100000 * 1.08);
        CHECK(Util::pitchToPercentage(pitch8percent) == Catch::Approx(8.0).margin(0.1));
    }

    SECTION("-8% pitch") {
        // -8% = 0x100000 * 0.92 ≈ 964689
        int pitchMinus8 = static_cast<int>(0x100000 * 0.92);
        CHECK(Util::pitchToPercentage(pitchMinus8) == Catch::Approx(-8.0).margin(0.1));
    }

    SECTION("+50% pitch") {
        int pitch50 = static_cast<int>(0x100000 * 1.50);
        CHECK(Util::pitchToPercentage(pitch50) == Catch::Approx(50.0).margin(0.1));
    }

    SECTION("-50% pitch (half speed)") {
        int pitchHalf = 0x100000 / 2;
        CHECK(Util::pitchToPercentage(pitchHalf) == Catch::Approx(-50.0).margin(0.1));
    }
}

TEST_CASE("Util::pitchToMultiplier", "[Util]") {
    SECTION("Normal pitch (1.0x)") {
        CHECK(Util::pitchToMultiplier(0x100000) == Catch::Approx(1.0).epsilon(0.001));
    }

    SECTION("+100% pitch (2.0x)") {
        CHECK(Util::pitchToMultiplier(0x200000) == Catch::Approx(2.0).epsilon(0.001));
    }

    SECTION("Half speed (0.5x)") {
        CHECK(Util::pitchToMultiplier(0x080000) == Catch::Approx(0.5).epsilon(0.001));
    }

    SECTION("Stopped (0x)") {
        CHECK(Util::pitchToMultiplier(0) == Catch::Approx(0.0).epsilon(0.001));
    }
}

TEST_CASE("Util::percentageToPitch", "[Util]") {
    SECTION("0% -> normal pitch") {
        CHECK(Util::percentageToPitch(0.0) == 0x100000);
    }

    SECTION("+8% pitch") {
        int pitch = Util::percentageToPitch(8.0);
        // Round-trip test
        CHECK(Util::pitchToPercentage(pitch) == Catch::Approx(8.0).margin(0.01));
    }

    SECTION("-8% pitch") {
        int pitch = Util::percentageToPitch(-8.0);
        CHECK(Util::pitchToPercentage(pitch) == Catch::Approx(-8.0).margin(0.01));
    }
}

TEST_CASE("Util::halfFrameToTime", "[Util]") {
    SECTION("Zero") {
        CHECK(Util::halfFrameToTime(0) == 0);
    }

    SECTION("150 half-frames per second") {
        // DJ Link uses 150 half-frames per second
        // 150 half-frames = 1 second = 1000ms
        CHECK(Util::halfFrameToTime(150) == Catch::Approx(1000).margin(10));
    }

    SECTION("300 half-frames = 2 seconds") {
        CHECK(Util::halfFrameToTime(300) == Catch::Approx(2000).margin(20));
    }
}

TEST_CASE("Util::timeToHalfFrame", "[Util]") {
    SECTION("Zero") {
        CHECK(Util::timeToHalfFrame(0) == 0);
    }

    SECTION("1000ms = 150 half-frames") {
        CHECK(Util::timeToHalfFrame(1000) == Catch::Approx(150).margin(2));
    }

    SECTION("Round trip") {
        int64_t originalTime = 5000; // 5 seconds
        int64_t halfFrame = Util::timeToHalfFrame(originalTime);
        int64_t backToTime = Util::halfFrameToTime(halfFrame);
        CHECK(backToTime == Catch::Approx(originalTime).margin(50));
    }
}

TEST_CASE("Util::formatMac", "[Util]") {
    SECTION("Standard MAC address") {
        std::array<uint8_t, 6> mac = {0x00, 0x1A, 0x2B, 0x3C, 0x4D, 0x5E};
        std::string formatted = Util::formatMac(mac);
        CHECK(formatted == "00:1a:2b:3c:4d:5e");
    }

    SECTION("All zeros") {
        std::array<uint8_t, 6> mac = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
        std::string formatted = Util::formatMac(mac);
        CHECK(formatted == "00:00:00:00:00:00");
    }

    SECTION("All ones") {
        std::array<uint8_t, 6> mac = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
        std::string formatted = Util::formatMac(mac);
        CHECK(formatted == "ff:ff:ff:ff:ff:ff");
    }
}
