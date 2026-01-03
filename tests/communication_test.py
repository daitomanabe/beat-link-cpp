#!/usr/bin/env python3
"""
Beat Link C++ Communication Tests

Tests actual UDP communication between the emulator and Beat Link library.
Verifies that devices are discovered and beats are received correctly.

Usage:
    python communication_test.py              # Run all communication tests
    python communication_test.py --verbose    # Verbose output
    python communication_test.py --emulator-only  # Only test emulator (no port needed)

Note: If rekordbox or other DJ software is running, the ports will be in use
and communication tests will be skipped. Close the software or use --emulator-only.
"""

import sys
import time
import threading
import json
import socket
from pathlib import Path
from typing import Optional, List
from dataclasses import dataclass

# Add parent directories to path
script_dir = Path(__file__).parent
build_dir = script_dir.parent / "build"
sys.path.insert(0, str(build_dir))
sys.path.insert(0, str(script_dir))

# Import emulator
from emulator import DJLinkEmulator, EmulatedDevice, run_quick_test, PORT_ANNOUNCEMENT, PORT_BEAT

# Test tracking
passed = 0
failed = 0
skipped = 0
verbose = "--verbose" in sys.argv or "-v" in sys.argv
emulator_only = "--emulator-only" in sys.argv

def log(msg: str):
    if verbose:
        print(f"    {msg}")

def check_port_available(port: int) -> bool:
    """Check if a UDP port is available."""
    try:
        sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        sock.bind(('0.0.0.0', port))
        sock.close()
        return True
    except OSError:
        return False

def check_dj_ports() -> tuple[bool, str]:
    """Check if DJ Link ports are available."""
    ports = [PORT_ANNOUNCEMENT, PORT_BEAT]
    in_use = []
    for port in ports:
        if not check_port_available(port):
            in_use.append(port)

    if in_use:
        return False, f"Ports {in_use} in use (rekordbox running?)"
    return True, ""

def test(name: str, condition: bool, message: str = ""):
    global passed, failed
    if condition:
        print(f"  ✓ {name}")
        passed += 1
    else:
        print(f"  ✗ {name}: {message}")
        failed += 1

def skip(name: str, reason: str):
    global skipped
    print(f"  ⊘ {name}: SKIPPED ({reason})")
    skipped += 1


# ============================================================================
# Emulator Unit Tests
# ============================================================================

def test_emulator_packets():
    """Test that the emulator creates valid packets."""
    print("\n[Emulator Packet Tests]")

    device = EmulatedDevice(
        name="TestCDJ",
        device_number=2,
        bpm=140.0,
        pitch_percent=4.0
    )
    emulator = DJLinkEmulator(device)

    # Test announcement packet
    announcement = emulator._create_device_announcement_packet()
    test("Announcement packet is 54 bytes", len(announcement) == 54,
         f"Got {len(announcement)} bytes")

    # Check magic header
    MAGIC_HEADER = bytes([0x51, 0x73, 0x70, 0x74, 0x31, 0x57, 0x6d, 0x4a, 0x4f, 0x4c])
    test("Announcement has magic header", announcement[:10] == MAGIC_HEADER)

    # Check packet type
    test("Announcement packet type is 0x0a", announcement[0x0a] == 0x0a,
         f"Got 0x{announcement[0x0a]:02x}")

    # Check device name
    name_bytes = announcement[0x0c:0x0c+7]
    test("Announcement contains device name", name_bytes == b"TestCDJ")

    # Check device number
    test("Announcement has device number 2", announcement[0x24] == 2,
         f"Got {announcement[0x24]}")

    # Test beat packet
    beat = emulator._create_beat_packet()
    test("Beat packet is 96 bytes", len(beat) == 96,
         f"Got {len(beat)} bytes")

    # Check magic header
    test("Beat has magic header", beat[:10] == MAGIC_HEADER)

    # Check packet type
    test("Beat packet type is 0x28", beat[0x0a] == 0x28,
         f"Got 0x{beat[0x0a]:02x}")

    # Check BPM encoding (BPM * 100 at offset 0x5a-0x5b)
    bpm_encoded = (beat[0x5a] << 8) | beat[0x5b]
    expected_bpm = int(140.0 * 100)
    test("Beat BPM encoded correctly", bpm_encoded == expected_bpm,
         f"Got {bpm_encoded}, expected {expected_bpm}")

    # Check beat position (1-4)
    test("Beat position is 1-4", 1 <= beat[0x5c] <= 4,
         f"Got {beat[0x5c]}")


# ============================================================================
# Communication Tests (require Beat Link library)
# ============================================================================

def test_device_discovery():
    """Test that emulated devices are discovered by Beat Link."""
    print("\n[Device Discovery Tests]")

    try:
        import beatlink_py as bl
    except ImportError:
        skip("Device discovery", "Python bindings not available")
        return

    # Track discovered devices
    discovered_devices: List[dict] = []
    discovery_lock = threading.Lock()

    def on_device_found(device):
        with discovery_lock:
            discovered_devices.append({
                "number": device.device_number,
                "name": device.device_name
            })
            log(f"Discovered: {device.device_name} #{device.device_number}")

    # Register listener
    bl.add_device_found_listener(on_device_found)

    # Start device finder
    started = bl.start_device_finder()
    test("Device finder started", started)

    if not started:
        skip("Device discovery communication", "Could not start device finder (port in use?)")
        bl.stop_device_finder()
        return

    # Create and send from emulator
    device = EmulatedDevice(
        name="EmulatedCDJ",
        device_number=3,
        bpm=128.0
    )
    emulator = DJLinkEmulator(device, "127.0.0.1")

    # Send multiple announcements to ensure delivery
    for _ in range(5):
        emulator.send_announcement()
        time.sleep(0.1)

    # Wait for processing
    time.sleep(0.5)

    # Stop finder
    bl.stop_device_finder()
    emulator.stop()

    # Check results
    with discovery_lock:
        # Device name may have trailing null bytes, strip them
        found = any(d["name"].rstrip('\x00') == "EmulatedCDJ" and d["number"] == 3
                   for d in discovered_devices)

    test("Emulated device was discovered", found,
         f"Found devices: {discovered_devices}")


def test_beat_reception():
    """Test that emulated beats are received by Beat Link."""
    print("\n[Beat Reception Tests]")

    try:
        import beatlink_py as bl
    except ImportError:
        skip("Beat reception", "Python bindings not available")
        return

    # Track received beats
    received_beats: List[dict] = []
    beats_lock = threading.Lock()

    def on_beat(beat):
        with beats_lock:
            received_beats.append({
                "device": beat.device_number,
                "bpm": beat.effective_bpm,
                "beat_in_bar": beat.beat_in_bar,
                "pitch": beat.pitch_percent
            })
            log(f"Beat: {beat.effective_bpm:.1f} BPM, beat {beat.beat_in_bar}/4")

    # Register listener
    bl.add_beat_listener(on_beat)

    # Start beat finder
    started = bl.start_beat_finder()
    test("Beat finder started", started)

    if not started:
        skip("Beat reception communication", "Could not start beat finder (port in use?)")
        bl.stop_beat_finder()
        return

    # Create emulator
    device = EmulatedDevice(
        name="BeatTestCDJ",
        device_number=1,
        bpm=120.0,
        pitch_percent=2.0
    )
    emulator = DJLinkEmulator(device, "127.0.0.1")

    # Send beats
    for _ in range(8):
        emulator.send_beat()
        time.sleep(0.1)  # Fast for testing

    # Wait for processing
    time.sleep(0.3)

    # Stop
    bl.stop_beat_finder()
    emulator.stop()

    # Check results
    with beats_lock:
        test("Received at least 4 beats", len(received_beats) >= 4,
             f"Received {len(received_beats)} beats")

        if received_beats:
            # Check BPM is approximately correct
            avg_bpm = sum(b["bpm"] for b in received_beats) / len(received_beats)
            expected_bpm = 120.0 * 1.02  # 120 BPM + 2% pitch
            test("BPM is approximately correct", abs(avg_bpm - expected_bpm) < 5,
                 f"Got avg {avg_bpm:.1f}, expected ~{expected_bpm:.1f}")

            # Check beat positions cycle 1-4
            beat_positions = [b["beat_in_bar"] for b in received_beats]
            test("Beat positions are 1-4", all(1 <= p <= 4 for p in beat_positions),
                 f"Got positions: {beat_positions}")


def test_multi_device():
    """Test discovery and beats from multiple emulated devices."""
    print("\n[Multi-Device Tests]")

    try:
        import beatlink_py as bl
    except ImportError:
        skip("Multi-device", "Python bindings not available")
        return

    # Track data
    devices_found = set()
    beats_by_device = {}
    lock = threading.Lock()

    def on_device_found(device):
        with lock:
            devices_found.add(device.device_number)
            log(f"Found device #{device.device_number}")

    def on_beat(beat):
        with lock:
            dev_num = beat.device_number
            if dev_num not in beats_by_device:
                beats_by_device[dev_num] = []
            beats_by_device[dev_num].append(beat.effective_bpm)

    # Register listeners
    bl.add_device_found_listener(on_device_found)
    bl.add_beat_listener(on_beat)

    # Start finders
    df_started = bl.start_device_finder()
    bf_started = bl.start_beat_finder()

    if not (df_started and bf_started):
        skip("Multi-device", "Could not start finders")
        bl.stop_device_finder()
        bl.stop_beat_finder()
        return

    # Create two emulators with different settings
    device1 = EmulatedDevice(name="CDJ-1", device_number=1, bpm=128.0)
    device2 = EmulatedDevice(name="CDJ-2", device_number=2, bpm=130.0)

    emulator1 = DJLinkEmulator(device1, "127.0.0.1")
    emulator2 = DJLinkEmulator(device2, "127.0.0.1")

    # Send announcements
    for _ in range(3):
        emulator1.send_announcement()
        emulator2.send_announcement()
        time.sleep(0.1)

    # Send beats from both
    for _ in range(4):
        emulator1.send_beat()
        emulator2.send_beat()
        time.sleep(0.1)

    time.sleep(0.3)

    # Cleanup
    bl.stop_beat_finder()
    bl.stop_device_finder()
    emulator1.stop()
    emulator2.stop()

    # Verify
    with lock:
        test("Found device #1", 1 in devices_found, f"Found: {devices_found}")
        test("Found device #2", 2 in devices_found, f"Found: {devices_found}")

        test("Received beats from device #1", 1 in beats_by_device,
             f"Beat devices: {list(beats_by_device.keys())}")
        test("Received beats from device #2", 2 in beats_by_device,
             f"Beat devices: {list(beats_by_device.keys())}")

        if 1 in beats_by_device and 2 in beats_by_device:
            avg1 = sum(beats_by_device[1]) / len(beats_by_device[1])
            avg2 = sum(beats_by_device[2]) / len(beats_by_device[2])
            test("Device 1 BPM ~128", abs(avg1 - 128) < 5, f"Got {avg1:.1f}")
            test("Device 2 BPM ~130", abs(avg2 - 130) < 5, f"Got {avg2:.1f}")


def test_pitch_variations():
    """Test different pitch values are correctly received."""
    print("\n[Pitch Variation Tests]")

    try:
        import beatlink_py as bl
    except ImportError:
        skip("Pitch variations", "Python bindings not available")
        return

    pitch_tests = [
        (0.0, "neutral"),
        (8.0, "+8%"),
        (-8.0, "-8%"),
        (16.0, "+16%"),
    ]

    for pitch_percent, label in pitch_tests:
        received_pitches = []
        lock = threading.Lock()

        def on_beat(beat):
            with lock:
                received_pitches.append(beat.pitch_percent)

        bl.add_beat_listener(on_beat)

        started = bl.start_beat_finder()
        if not started:
            skip(f"Pitch {label}", "Could not start beat finder")
            continue

        device = EmulatedDevice(name="PitchTest", device_number=1, bpm=100.0,
                               pitch_percent=pitch_percent)
        emulator = DJLinkEmulator(device, "127.0.0.1")

        for _ in range(4):
            emulator.send_beat()
            time.sleep(0.05)

        time.sleep(0.2)
        bl.stop_beat_finder()
        emulator.stop()

        with lock:
            if received_pitches:
                avg_pitch = sum(received_pitches) / len(received_pitches)
                test(f"Pitch {label} received correctly",
                     abs(avg_pitch - pitch_percent) < 1.0,
                     f"Expected {pitch_percent}, got {avg_pitch:.2f}")
            else:
                test(f"Pitch {label} received", False, "No beats received")


def test_rapid_beats():
    """Test handling of rapid beat sequences (high BPM)."""
    print("\n[Rapid Beat Tests]")

    try:
        import beatlink_py as bl
    except ImportError:
        skip("Rapid beats", "Python bindings not available")
        return

    received_count = [0]
    lock = threading.Lock()

    def on_beat(beat):
        with lock:
            received_count[0] += 1

    bl.add_beat_listener(on_beat)

    started = bl.start_beat_finder()
    if not started:
        skip("Rapid beats", "Could not start beat finder")
        return

    # High BPM device
    device = EmulatedDevice(name="HighBPM", device_number=1, bpm=180.0)
    emulator = DJLinkEmulator(device, "127.0.0.1")

    # Send 20 beats rapidly
    for _ in range(20):
        emulator.send_beat()
        time.sleep(0.02)  # 50ms between beats

    time.sleep(0.3)
    bl.stop_beat_finder()
    emulator.stop()

    with lock:
        test("Received most rapid beats", received_count[0] >= 15,
             f"Received {received_count[0]}/20")


# ============================================================================
# Main
# ============================================================================

def main():
    global passed, failed, skipped

    print("=" * 70)
    print("Beat Link C++ Communication Tests")
    print("=" * 70)

    # Check port availability
    ports_available, port_msg = check_dj_ports()
    if not ports_available:
        print(f"\n⚠ {port_msg}")
        print("  Communication tests will be skipped.")
        print("  Close rekordbox or use --emulator-only to run emulator tests only.\n")

    # Emulator unit tests (no library needed, no ports needed)
    test_emulator_packets()

    if emulator_only:
        print("\n[Communication Tests]")
        skip("All communication tests", "--emulator-only specified")
    elif not ports_available:
        print("\n[Communication Tests]")
        skip("All communication tests", port_msg)
    else:
        # Communication tests (require library and ports)
        test_device_discovery()
        test_beat_reception()
        test_multi_device()
        test_pitch_variations()
        test_rapid_beats()

    # Summary
    total = passed + failed + skipped
    print("\n" + "=" * 70)
    print(f"Results: {passed} passed, {failed} failed, {skipped} skipped (total: {total})")
    print("=" * 70)

    if failed > 0:
        print("\n⚠ Some tests failed!")
        return 1
    else:
        print("\n✓ All tests passed!")
        return 0


if __name__ == "__main__":
    sys.exit(main())
