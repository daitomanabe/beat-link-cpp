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

// Lifecycle and listeners
#include "LifecycleParticipant.hpp"
#include "LifecycleListener.hpp"
#include "BeatListener.hpp"
#include "DeviceAnnouncementListener.hpp"
#include "DeviceUpdateListener.hpp"
#include "MasterListener.hpp"
#include "MasterAdapter.hpp"
#include "PrecisePositionListener.hpp"
#include "SyncListener.hpp"
#include "FaderStartListener.hpp"
#include "OnAirListener.hpp"
#include "MasterHandoffListener.hpp"

// Data types
#include "DeviceReference.hpp"
#include "DeviceAnnouncement.hpp"
#include "DeviceUpdate.hpp"
#include "Beat.hpp"
#include "CdjStatus.hpp"
#include "MixerStatus.hpp"
#include "MediaDetails.hpp"
#include "MediaDetailsListener.hpp"
#include "data/DataReference.hpp"
#include "data/DeckReference.hpp"
#include "data/AlbumArt.hpp"
#include "data/AlbumArtListener.hpp"
#include "data/AlbumArtUpdate.hpp"
#include "data/AnalysisTag.hpp"
#include "data/AnalysisTagListener.hpp"
#include "data/AnalysisTagUpdate.hpp"
#include "data/BeatGrid.hpp"
#include "data/BeatGridListener.hpp"
#include "data/BeatGridUpdate.hpp"
#include "data/CueList.hpp"
#include "data/CrateDigger.hpp"
#include "data/Database.hpp"
#include "data/DatabaseListener.hpp"
#include "data/MetadataProvider.hpp"
#include "data/MountListener.hpp"
#include "data/MenuLoader.hpp"
#include "data/TrackMetadata.hpp"
#include "data/TrackMetadataListener.hpp"
#include "data/TrackMetadataUpdate.hpp"
#include "data/TrackPositionBeatListener.hpp"
#include "data/TrackPositionListener.hpp"
#include "data/TrackPositionUpdate.hpp"
#include "data/PlaybackState.hpp"
#include "data/SignatureListener.hpp"
#include "data/SignatureUpdate.hpp"
#include "data/SQLiteConnection.hpp"
#include "data/SQLiteConnectionListener.hpp"
#include "data/WaveformTypes.hpp"
#include "data/WaveformPreview.hpp"
#include "data/WaveformPreviewUpdate.hpp"
#include "data/WaveformDetail.hpp"
#include "data/WaveformDetailUpdate.hpp"
#include "data/WaveformListener.hpp"

// Dbserver protocol
#include "dbserver/Field.hpp"
#include "dbserver/NumberField.hpp"
#include "dbserver/StringField.hpp"
#include "dbserver/BinaryField.hpp"
#include "dbserver/Message.hpp"
#include "dbserver/Client.hpp"
#include "dbserver/ConnectionManager.hpp"

// Finders
#include "DeviceFinder.hpp"
#include "BeatFinder.hpp"
#include "data/AnalysisTagFinder.hpp"
#include "data/MetadataFinder.hpp"
#include "data/BeatGridFinder.hpp"
#include "data/WaveformFinder.hpp"
#include "data/ArtFinder.hpp"
#include "data/SignatureFinder.hpp"
#include "data/TimeFinder.hpp"
#include "VirtualCdj.hpp"
#include "VirtualRekordbox.hpp"

// Version is defined in PacketTypes.hpp to avoid circular dependencies
