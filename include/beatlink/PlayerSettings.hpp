#pragma once

#include <cstdint>
#include "Span.hpp"

#include "Util.hpp"

namespace beatlink {

struct PlayerSettings {
    enum class Toggle : uint8_t { Off = 0x80, On = 0x81 };

    enum class LcdBrightness : uint8_t {
        Dimmest = 0x81,
        Dim = 0x82,
        Medium = 0x83,
        Bright = 0x84,
        Brightest = 0x85
    };

    enum class AutoCueLevel : uint8_t {
        Minus36 = 0x80,
        Minus42 = 0x81,
        Minus48 = 0x82,
        Minus54 = 0x83,
        Minus60 = 0x84,
        Minus66 = 0x85,
        Minus72 = 0x86,
        Minus78 = 0x87,
        Memory = 0x88
    };

    enum class Language : uint8_t {
        English = 0x81,
        French = 0x82,
        German = 0x83,
        Italian = 0x84,
        Dutch = 0x85,
        Spanish = 0x86,
        Russian = 0x87,
        Korean = 0x88,
        ChineseSimplified = 0x89,
        ChineseTraditional = 0x8a,
        Japanese = 0x8b,
        Portuguese = 0x8c,
        Swedish = 0x8d,
        Czech = 0x8e,
        Magyar = 0x8f,
        Danish = 0x90,
        Greek = 0x91,
        Turkish = 0x92
    };

    enum class Illumination : uint8_t { Off = 0x80, Dark = 0x81, Bright = 0x82 };

    enum class PlayMode : uint8_t { Continue = 0x80, Single = 0x81 };

    enum class QuantizeMode : uint8_t { Beat = 0x80, Half = 0x81, Quarter = 0x82, Eighth = 0x83 };

    enum class AutoLoadMode : uint8_t { Off = 0x80, On = 0x81, Rekordbox = 0x82 };

    enum class TimeDisplayMode : uint8_t { Elapsed = 0x80, Remaining = 0x81 };

    enum class JogMode : uint8_t { Cdj = 0x80, Vinyl = 0x81 };

    enum class TempoRange : uint8_t { PlusMinus6 = 0x80, PlusMinus10 = 0x81, PlusMinus16 = 0x82, Wide = 0x83 };

    enum class PhaseMeterType : uint8_t { Type1 = 0x80, Type2 = 0x81 };

    enum class VinylSpeedAdjust : uint8_t { TouchRelease = 0x80, Touch = 0x81, Release = 0x82 };

    enum class JogWheelDisplay : uint8_t { Auto = 0x80, Info = 0x81, Simple = 0x82, Artwork = 0x83 };

    enum class PadButtonBrightness : uint8_t {
        Dimmest = 0x81,
        Dim = 0x82,
        Bright = 0x83,
        Brightest = 0x84
    };

    Toggle onAirDisplay = Toggle::On;
    LcdBrightness lcdBrightness = LcdBrightness::Medium;
    Toggle quantize = Toggle::On;
    AutoCueLevel autoCueLevel = AutoCueLevel::Memory;
    Language language = Language::English;
    Illumination jogRingIllumination = Illumination::Bright;
    Toggle jogRingIndicator = Toggle::On;
    Toggle slipFlashing = Toggle::On;
    Illumination discSlotIllumination = Illumination::Dark;
    Toggle ejectLoadLock = Toggle::On;
    Toggle sync = Toggle::On;
    PlayMode autoPlayMode = PlayMode::Single;
    QuantizeMode quantizeBeatValue = QuantizeMode::Beat;
    AutoLoadMode autoLoadMode = AutoLoadMode::Rekordbox;
    Toggle hotCueColor = Toggle::On;
    Toggle needleLock = Toggle::On;
    TimeDisplayMode timeDisplayMode = TimeDisplayMode::Remaining;
    JogMode jogMode = JogMode::Vinyl;
    Toggle autoCue = Toggle::On;
    Toggle masterTempo = Toggle::On;
    TempoRange tempoRange = TempoRange::PlusMinus6;
    PhaseMeterType phaseMeterType = PhaseMeterType::Type1;
    VinylSpeedAdjust vinylSpeedAdjust = VinylSpeedAdjust::TouchRelease;
    JogWheelDisplay jogWheelDisplay = JogWheelDisplay::Auto;
    PadButtonBrightness padButtonBrightness = PadButtonBrightness::Bright;
    LcdBrightness jogWheelLcdBrightness = LcdBrightness::Medium;

    void applyToPayload(std::span<uint8_t> payload) const {
        Util::setPayloadByte(payload, 0x2c, static_cast<uint8_t>(onAirDisplay));
        Util::setPayloadByte(payload, 0x2d, static_cast<uint8_t>(lcdBrightness));
        Util::setPayloadByte(payload, 0x2e, static_cast<uint8_t>(quantize));
        Util::setPayloadByte(payload, 0x2f, static_cast<uint8_t>(autoCueLevel));
        Util::setPayloadByte(payload, 0x30, static_cast<uint8_t>(language));
        Util::setPayloadByte(payload, 0x32, static_cast<uint8_t>(jogRingIllumination));
        Util::setPayloadByte(payload, 0x33, static_cast<uint8_t>(jogRingIndicator));
        Util::setPayloadByte(payload, 0x34, static_cast<uint8_t>(slipFlashing));
        Util::setPayloadByte(payload, 0x38, static_cast<uint8_t>(discSlotIllumination));
        Util::setPayloadByte(payload, 0x39, static_cast<uint8_t>(ejectLoadLock));
        Util::setPayloadByte(payload, 0x3a, static_cast<uint8_t>(sync));
        Util::setPayloadByte(payload, 0x3b, static_cast<uint8_t>(autoPlayMode));
        Util::setPayloadByte(payload, 0x3c, static_cast<uint8_t>(quantizeBeatValue));
        Util::setPayloadByte(payload, 0x3d, static_cast<uint8_t>(autoLoadMode));
        Util::setPayloadByte(payload, 0x3e, static_cast<uint8_t>(hotCueColor));
        Util::setPayloadByte(payload, 0x41, static_cast<uint8_t>(needleLock));
        Util::setPayloadByte(payload, 0x44, static_cast<uint8_t>(timeDisplayMode));
        Util::setPayloadByte(payload, 0x45, static_cast<uint8_t>(jogMode));
        Util::setPayloadByte(payload, 0x46, static_cast<uint8_t>(autoCue));
        Util::setPayloadByte(payload, 0x47, static_cast<uint8_t>(masterTempo));
        Util::setPayloadByte(payload, 0x48, static_cast<uint8_t>(tempoRange));
        Util::setPayloadByte(payload, 0x49, static_cast<uint8_t>(phaseMeterType));
        Util::setPayloadByte(payload, 0x4c, static_cast<uint8_t>(vinylSpeedAdjust));
        Util::setPayloadByte(payload, 0x4d, static_cast<uint8_t>(jogWheelDisplay));
        Util::setPayloadByte(payload, 0x4e, static_cast<uint8_t>(padButtonBrightness));
        Util::setPayloadByte(payload, 0x4f, static_cast<uint8_t>(jogWheelLcdBrightness));
    }
};

} // namespace beatlink
