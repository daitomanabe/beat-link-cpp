#pragma once

/**
 * Beat Link C++ - Pioneer DJ Link Protocol Library
 *
 * A C++20 port of the Java Beat Link library for communicating with
 * Pioneer DJ equipment on a Pro DJ Link network.
 *
 * Ported from https://github.com/Deep-Symmetry/beat-link
 *
 * Updated for C++20 per INTRODUCTION_JAVA_TO_CPP.md:
 * - std::span for safe buffer access
 * - Concepts for template constraints
 * - Handle Pattern for object management
 * - Safety Curtain for output limiting
 * - API introspection for AI agent integration
 */

// Core utilities (C++20 features: std::span, std::format, concepts)
#include "Util.hpp"
#include "PacketTypes.hpp"

// C++20 patterns
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
