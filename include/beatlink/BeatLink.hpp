#pragma once

/**
 * Beat Link C++ - Pioneer DJ Link Protocol Library
 *
 * A C++17 port of the Java Beat Link library for communicating with
 * Pioneer DJ equipment on a Pro DJ Link network.
 *
 * Ported from https://github.com/Deep-Symmetry/beat-link
 */

// Core utilities
#include "Util.hpp"
#include "PacketTypes.hpp"

// Patterns
#include "HandlePool.hpp"
#include "SafetyCurtain.hpp"
#include "ApiSchema.hpp"

// Data types
#include "DeviceReference.hpp"
#include "DeviceAnnouncement.hpp"
#include "DeviceUpdate.hpp"
#include "Beat.hpp"
#include "CdjStatus.hpp"
#include "MixerStatus.hpp"

// Finders
#include "DeviceFinder.hpp"
#include "BeatFinder.hpp"

// Version is defined in PacketTypes.hpp to avoid circular dependencies
