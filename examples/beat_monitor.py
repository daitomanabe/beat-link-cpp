#!/usr/bin/env python3
"""
Beat Monitor - Real-time beat information display

This example demonstrates how to:
- Start device and beat finders
- Register beat listeners
- Display real-time beat information

Usage:
    python beat_monitor.py

Requirements:
    - beatlink_py module (build with -DBEATLINK_BUILD_PYTHON=ON)
    - CDJ/XDJ devices on the network
"""

import sys
import time
import signal

# Add build directory to path if needed
sys.path.insert(0, '../build')

try:
    import beatlink_py as bl
except ImportError:
    print("Error: beatlink_py module not found.")
    print("Build with: cmake .. -DBEATLINK_BUILD_PYTHON=ON && make beatlink_py")
    sys.exit(1)

# Global flag for clean shutdown
running = True

def signal_handler(sig, frame):
    global running
    print("\nShutting down...")
    running = False

def on_beat(beat):
    """Callback for beat events."""
    bar_position = "█" * beat.beat_in_bar + "░" * (4 - beat.beat_in_bar)
    print(f"\r[{bar_position}] Device {beat.device_number}: "
          f"{beat.effective_bpm:6.2f} BPM | "
          f"Beat {beat.beat_in_bar}/4 | "
          f"Next beat: {beat.next_beat_ms:4d}ms  ", end="", flush=True)

def on_device_found(device):
    """Callback for device found events."""
    print(f"\n✓ Device found: {device.device_name} (#{device.device_number}) @ {device.address}")

def on_device_lost(device):
    """Callback for device lost events."""
    print(f"\n✗ Device lost: {device.device_name} (#{device.device_number})")

def main():
    global running

    print("=" * 60)
    print("Beat Link - Beat Monitor")
    print(f"Version: {bl.__version__}")
    print("=" * 60)

    # Set up signal handler for clean shutdown
    signal.signal(signal.SIGINT, signal_handler)

    # Register listeners
    bl.add_beat_listener(on_beat)
    bl.add_device_found_listener(on_device_found)
    bl.add_device_lost_listener(on_device_lost)

    # Start finders
    print("\nStarting device finder...")
    if not bl.start_device_finder():
        print("Error: Failed to start device finder")
        return 1

    print("Starting beat finder...")
    if not bl.start_beat_finder():
        print("Error: Failed to start beat finder")
        bl.stop_device_finder()
        return 1

    print("\nListening for beats... (Press Ctrl+C to stop)")
    print("-" * 60)

    # Main loop
    while running:
        time.sleep(0.1)

    # Cleanup
    print("\n\nStopping...")
    bl.stop_beat_finder()
    bl.stop_device_finder()
    bl.clear_all_listeners()

    print("Done.")
    return 0

if __name__ == "__main__":
    sys.exit(main())
