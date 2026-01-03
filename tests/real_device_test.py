#!/usr/bin/env python3
"""
Real CDJ/XDJ Device Test

Tests Beat Link C++ library with actual Pioneer DJ equipment.
Requires CDJ/XDJ connected to the same network.

Usage:
    python3 real_device_test.py              # 30 second test
    python3 real_device_test.py --duration 60   # 60 second test
    python3 real_device_test.py --verbose       # Verbose output
"""

import sys
import time
import argparse
from pathlib import Path
from datetime import datetime

# Add build directory to path
build_dir = Path(__file__).parent.parent / "build"
sys.path.insert(0, str(build_dir))

try:
    import beatlink_py as bl
except ImportError:
    print("ERROR: beatlink_py not found!")
    print(f"       Expected at: {build_dir}")
    print("       Run 'make beatlink_py' first.")
    sys.exit(1)


def main():
    parser = argparse.ArgumentParser(description="Test Beat Link with real CDJ/XDJ devices")
    parser.add_argument("--duration", type=int, default=30, help="Test duration in seconds (default: 30)")
    parser.add_argument("--verbose", "-v", action="store_true", help="Verbose output")
    args = parser.parse_args()

    print("=" * 70)
    print("Beat Link C++ - Real Device Test")
    print("=" * 70)
    print()
    print("Before running this test, ensure:")
    print("  1. Rekordbox is CLOSED (to free up ports 50000-50002)")
    print("  2. CDJ/XDJ is powered ON and connected to the same network")
    print("  3. A track is loaded and PLAYING on the CDJ (for beat detection)")
    print()
    print(f"Test duration: {args.duration} seconds")
    print()

    # Check if ports are available
    import socket
    for port in [50000, 50001]:
        try:
            sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
            sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
            sock.bind(('0.0.0.0', port))
            sock.close()
        except OSError:
            print(f"ERROR: Port {port} is in use!")
            print("       Close Rekordbox or other DJ software first.")
            return 1

    # Data collection
    devices_found = {}
    beats_by_device = {}
    start_time = None

    def on_device_found(device):
        key = (device.device_number, device.device_name)
        if key not in devices_found:
            devices_found[key] = {
                "device": device,
                "found_at": time.time() - start_time if start_time else 0
            }
            print(f"[{elapsed():.1f}s] DEVICE FOUND: {device.device_name} #{device.device_number} @ {device.address}")

    def on_device_lost(device):
        print(f"[{elapsed():.1f}s] DEVICE LOST: {device.device_name} #{device.device_number}")

    def on_beat(beat):
        dev_num = beat.device_number
        if dev_num not in beats_by_device:
            beats_by_device[dev_num] = []
        beats_by_device[dev_num].append({
            "bpm": beat.effective_bpm,
            "beat_in_bar": beat.beat_in_bar,
            "pitch": beat.pitch_percent,
            "time": time.time() - start_time if start_time else 0
        })
        if args.verbose:
            print(f"[{elapsed():.1f}s] BEAT: {beat.effective_bpm:7.2f} BPM | "
                  f"Beat {beat.beat_in_bar}/4 | Pitch {beat.pitch_percent:+.1f}% | "
                  f"Device #{beat.device_number}")
        else:
            # Compact output - show only on beat 1
            if beat.beat_in_bar == 1:
                bar_count = len(beats_by_device[dev_num]) // 4
                print(f"[{elapsed():.1f}s] Device #{dev_num}: {beat.effective_bpm:.1f} BPM | Bar {bar_count}")

    def elapsed():
        return time.time() - start_time if start_time else 0

    # Register listeners
    bl.add_device_found_listener(on_device_found)
    bl.add_device_lost_listener(on_device_lost)
    bl.add_beat_listener(on_beat)

    # Start finders
    print("-" * 70)
    print("Starting Beat Link...")
    print("-" * 70)

    if not bl.start_device_finder():
        print("ERROR: Failed to start device finder!")
        return 1
    print("Device finder started (UDP port 50000)")

    if not bl.start_beat_finder():
        print("ERROR: Failed to start beat finder!")
        bl.stop_device_finder()
        return 1
    print("Beat finder started (UDP port 50001)")

    start_time = time.time()
    print()
    print("-" * 70)
    print(f"Listening for {args.duration} seconds... (Press Ctrl+C to stop)")
    print("-" * 70)
    print()

    try:
        time.sleep(args.duration)
    except KeyboardInterrupt:
        print("\n[Stopped by user]")

    # Stop finders
    bl.stop_beat_finder()
    bl.stop_device_finder()

    # Generate report
    print()
    print("=" * 70)
    print("TEST RESULTS")
    print("=" * 70)
    print()

    print(f"Test Duration: {elapsed():.1f} seconds")
    print(f"Timestamp: {datetime.now().isoformat()}")
    print()

    # Devices
    print(f"Devices Discovered: {len(devices_found)}")
    if devices_found:
        for key, info in devices_found.items():
            d = info["device"]
            print(f"  [{info['found_at']:.1f}s] {d.device_name} #{d.device_number}")
            print(f"         IP: {d.address}")
            if d.device_number in beats_by_device:
                beats = beats_by_device[d.device_number]
                print(f"         Beats received: {len(beats)}")
    print()

    # Beats analysis
    total_beats = sum(len(b) for b in beats_by_device.values())
    print(f"Total Beats Received: {total_beats}")

    if beats_by_device:
        for dev_num, beats in beats_by_device.items():
            if beats:
                bpms = [b["bpm"] for b in beats]
                pitches = [b["pitch"] for b in beats]
                print(f"\n  Device #{dev_num}:")
                print(f"    Beats: {len(beats)}")
                print(f"    BPM: {sum(bpms)/len(bpms):.2f} (min: {min(bpms):.2f}, max: {max(bpms):.2f})")
                print(f"    Pitch: {sum(pitches)/len(pitches):+.2f}% average")
                print(f"    Bars: {len(beats) // 4}")
    print()

    # Verdict
    print("-" * 70)
    if devices_found and total_beats > 0:
        print("RESULT: SUCCESS")
        print("Real device communication verified!")
        return 0
    elif devices_found:
        print("RESULT: PARTIAL SUCCESS")
        print("Devices found but no beats received.")
        print("Is a track playing on the CDJ?")
        return 1
    else:
        print("RESULT: FAILED")
        print("No devices found.")
        print("\nTroubleshooting:")
        print("  - Check that CDJ is powered on")
        print("  - Check network connection (same subnet)")
        print("  - Check CDJ Link settings")
        print("  - Try: sudo tcpdump -i any udp port 50000")
        return 1


if __name__ == "__main__":
    sys.exit(main())
