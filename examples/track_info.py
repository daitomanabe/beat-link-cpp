#!/usr/bin/env python3
"""
Track Info - Display track metadata from DJ players

This example demonstrates how to:
- Start VirtualCdj and MetadataFinder
- Register metadata listeners
- Retrieve and display track information including:
  - Track metadata (title, artist, BPM, key, etc.)
  - Cue points and hot cues
  - Beat grid information
  - Album art

Usage:
    python track_info.py

Requirements:
    - beatlink_py module (build with -DBEATLINK_BUILD_PYTHON=ON)
    - CDJ/XDJ devices on the network with tracks loaded
"""

import sys
import time
import signal

sys.path.insert(0, '../build')

try:
    import beatlink_py as bl
except ImportError:
    print("Error: beatlink_py module not found.")
    print("Build with: cmake .. -DBEATLINK_BUILD_PYTHON=ON && make beatlink_py")
    sys.exit(1)

running = True

def signal_handler(sig, frame):
    global running
    print("\nShutting down...")
    running = False

def format_time(ms):
    """Format milliseconds as mm:ss."""
    if ms < 0:
        return "--:--"
    seconds = ms // 1000
    minutes = seconds // 60
    seconds = seconds % 60
    return f"{minutes:02d}:{seconds:02d}"

def on_metadata_update(update):
    """Callback for track metadata updates."""
    print(f"\n{'=' * 60}")
    print(f"Track Update - Player {update.player}")
    print(f"{'=' * 60}")

    if update.metadata is None:
        print("  (No track loaded)")
        return

    meta = update.metadata
    print(f"  Title:    {meta.title}")
    print(f"  Artist:   {meta.artist}")
    print(f"  Album:    {meta.album}")
    print(f"  Genre:    {meta.genre}")
    print(f"  Key:      {meta.key}")
    print(f"  Duration: {format_time(meta.duration * 1000)}")
    print(f"  BPM:      {meta.tempo / 100:.2f}")
    print(f"  Rating:   {'★' * meta.rating}{'☆' * (5 - meta.rating)}")
    if meta.year > 0:
        print(f"  Year:     {meta.year}")

def display_player_info(player):
    """Display detailed information for a player."""
    print(f"\n{'─' * 60}")
    print(f"Player {player} Details")
    print(f"{'─' * 60}")

    # Track metadata
    metadata = bl.get_track_metadata(player)
    if metadata:
        print(f"\n[Track Metadata]")
        print(f"  Title:  {metadata.title}")
        print(f"  Artist: {metadata.artist}")
        print(f"  BPM:    {metadata.tempo / 100:.2f}")
    else:
        print("\n[Track Metadata] Not available")

    # CDJ Status
    status = bl.get_cdj_status(player)
    if status:
        print(f"\n[CDJ Status]")
        state = "Playing" if status.is_playing else "Paused" if status.is_paused else "Cued" if status.is_cued else "Stopped"
        print(f"  State:    {state}")
        print(f"  Tempo:    {status.effective_tempo:.2f} BPM")
        print(f"  Pitch:    {bl.pitch_to_percentage(status.pitch):+.2f}%")
        print(f"  On Air:   {'Yes' if status.is_on_air else 'No'}")
        print(f"  Synced:   {'Yes' if status.is_synced else 'No'}")
        print(f"  Master:   {'Yes' if status.is_tempo_master else 'No'}")
    else:
        print("\n[CDJ Status] Not available")

    # Cue List
    cue_list = bl.get_cue_list(player)
    if cue_list:
        print(f"\n[Cue Points] {cue_list.hot_cue_count} hot cues, {cue_list.memory_point_count} memory points, {cue_list.loop_count} loops")
        for cue in cue_list.entries[:8]:  # Show first 8 cues
            cue_type = "Loop" if cue.is_loop else f"HotCue {cue.hot_cue_number}" if cue.hot_cue_number > 0 else "Memory"
            color = f" [{cue.color_name}]" if cue.color_name and cue.color_name != "No Color" else ""
            comment = f' "{cue.comment}"' if cue.comment else ""
            print(f"  {cue_type:10s} @ {format_time(cue.cue_time_ms)}{color}{comment}")
    else:
        print("\n[Cue Points] Not available")

    # Beat Grid
    beat_grid = bl.get_beat_grid(player)
    if beat_grid:
        print(f"\n[Beat Grid] {beat_grid.beat_count} beats")
        # Show BPM changes if any
        if beat_grid.beat_count > 0:
            first_bpm = beat_grid.beats[0].bpm
            bpm_changes = []
            for beat in beat_grid.beats:
                if beat.bpm != first_bpm and beat.bpm not in [b for b, _ in bpm_changes]:
                    bpm_changes.append((beat.bpm, beat.time_ms))
            if bpm_changes:
                print(f"  BPM changes detected:")
                for bpm, time_ms in bpm_changes[:5]:
                    print(f"    {bpm / 100:.2f} BPM @ {format_time(time_ms)}")
    else:
        print("\n[Beat Grid] Not available")

    # Album Art
    art = bl.get_album_art(player)
    if art:
        print(f"\n[Album Art] {art.width}x{art.height} {art.format.upper()} ({len(art.raw_bytes)} bytes)")
    else:
        print("\n[Album Art] Not available")

    # Waveform
    preview = bl.get_waveform_preview(player)
    if preview:
        color_str = "Color" if preview.is_color else "Monochrome"
        print(f"\n[Waveform Preview] {preview.segment_count} segments, {color_str}")
    else:
        print("\n[Waveform Preview] Not available")

def main():
    global running

    print("=" * 60)
    print("Beat Link - Track Info")
    print(f"Version: {bl.__version__}")
    print("=" * 60)

    signal.signal(signal.SIGINT, signal_handler)

    # Register metadata listener
    bl.add_track_metadata_listener(on_metadata_update)

    # Start services
    print("\nStarting services...")

    if not bl.start_device_finder():
        print("Error: Failed to start device finder")
        return 1

    if not bl.start_virtual_cdj():
        print("Error: Failed to start virtual CDJ")
        return 1

    bl.start_metadata_finder()
    bl.start_beat_grid_finder()
    bl.start_waveform_finder()
    bl.start_art_finder()

    print("Waiting for devices and tracks...")
    print("(Press Ctrl+C to stop, Enter to refresh player info)")
    print("-" * 60)

    # Wait for devices
    time.sleep(2)

    # Show initial device list
    devices = bl.get_devices()
    if devices:
        print(f"\nFound {len(devices)} device(s):")
        for d in devices:
            print(f"  - {d.device_name} (#{d.device_number}) @ {d.address}")

    # Main loop - refresh on Enter key
    import select
    while running:
        # Check for input (non-blocking)
        try:
            import sys
            if sys.stdin in select.select([sys.stdin], [], [], 0.5)[0]:
                sys.stdin.readline()
                # Refresh all players
                devices = bl.get_devices()
                for device in devices:
                    if device.device_number <= 6:  # CDJ players only
                        display_player_info(device.device_number)
        except:
            time.sleep(0.5)

    # Cleanup
    print("\n\nStopping services...")
    bl.stop_art_finder()
    bl.stop_waveform_finder()
    bl.stop_beat_grid_finder()
    bl.stop_metadata_finder()
    bl.stop_virtual_cdj()
    bl.stop_device_finder()
    bl.clear_all_listeners()

    print("Done.")
    return 0

if __name__ == "__main__":
    sys.exit(main())
