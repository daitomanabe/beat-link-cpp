#!/usr/bin/env python3
"""
DJ Link Device Emulator

Emulates Pioneer DJ equipment for testing Beat Link C++ library.
Sends device announcements and beat packets via UDP.

Usage:
    python emulator.py                    # Run interactive emulator
    python emulator.py --device CDJ-3000  # Emulate specific device
    python emulator.py --bpm 128          # Set BPM
    python emulator.py --beats 16         # Send 16 beats then stop

Protocol Reference:
    Port 50000: Device announcements (54 bytes)
    Port 50001: Beat packets (96 bytes)
    Magic header: 51 73 70 74 31 57 6d 4a 4f 4c
"""

import socket
import struct
import time
import argparse
import threading
import sys
from dataclasses import dataclass
from typing import Optional, Callable
import random

# DJ Link Protocol Constants
MAGIC_HEADER = bytes([0x51, 0x73, 0x70, 0x74, 0x31, 0x57, 0x6d, 0x4a, 0x4f, 0x4c])
PORT_ANNOUNCEMENT = 50000
PORT_BEAT = 50001
PORT_STATUS = 50002

PACKET_TYPE_DEVICE_HELLO = 0x0a
PACKET_TYPE_DEVICE_KEEPALIVE = 0x06
PACKET_TYPE_BEAT = 0x28

# Use keep-alive for announcements (more compatible with Rekordbox)
USE_KEEPALIVE_FOR_ANNOUNCEMENT = True

NEUTRAL_PITCH = 1048576

@dataclass
class EmulatedDevice:
    """Configuration for an emulated DJ device."""
    name: str = "CDJ-3000"
    device_number: int = 1
    ip_address: str = "127.0.0.1"
    mac_address: bytes = b'\x00\x11\x22\x33\x44\x55'
    bpm: float = 128.0
    pitch_percent: float = 0.0
    is_playing: bool = True
    is_master: bool = False
    is_synced: bool = False


class DJLinkEmulator:
    """
    Emulates a Pioneer DJ Link device.

    Sends:
    - Device announcement packets to port 50000
    - Beat packets to port 50001
    """

    def __init__(self, device: EmulatedDevice, target_ip: str = "127.0.0.1"):
        self.device = device
        self.target_ip = target_ip
        self.running = False
        self.beat_count = 0
        self.beat_in_bar = 1

        # Callbacks for testing
        self.on_announcement_sent: Optional[Callable] = None
        self.on_beat_sent: Optional[Callable] = None

        # Sockets
        self.announcement_socket: Optional[socket.socket] = None
        self.beat_socket: Optional[socket.socket] = None

    def _create_device_announcement_packet(self) -> bytes:
        """
        Create a 54-byte device announcement packet.

        Packet structure (based on actual CDJ packets):
        0x00-0x09: Magic header (10 bytes)
        0x0a:      Packet type (0x0a = device hello)
        0x0b:      Unknown (0x00)
        0x0c-0x1f: Device name (20 bytes, null-padded ASCII)
        0x20:      Unknown (0x01)
        0x21:      Unknown (0x02)
        0x22-0x23: Unknown
        0x24:      Device number (1-4)
        0x25:      Unknown (0x02)
        0x26-0x29: IP address (4 bytes)
        0x2a-0x31: Unknown (8 bytes)
        0x32-0x35: MAC address first 4 bytes
        Total: 54 bytes (0x36)
        """
        packet = bytearray(54)

        # Magic header
        packet[0:10] = MAGIC_HEADER

        # Packet type (use keep-alive for better Rekordbox compatibility)
        if USE_KEEPALIVE_FOR_ANNOUNCEMENT:
            packet[0x0a] = PACKET_TYPE_DEVICE_KEEPALIVE
        else:
            packet[0x0a] = PACKET_TYPE_DEVICE_HELLO

        # Device name (20 bytes, null-padded)
        name_bytes = self.device.name.encode('ascii')[:20]
        packet[0x0c:0x0c + len(name_bytes)] = name_bytes

        # Various flags
        packet[0x20] = 0x01
        packet[0x21] = 0x02

        # Device number
        packet[0x24] = self.device.device_number

        # Protocol version indicator
        packet[0x25] = 0x02

        # IP address (4 bytes at 0x26-0x29)
        ip_parts = [int(p) for p in self.device.ip_address.split('.')]
        packet[0x26:0x2a] = bytes(ip_parts)

        # MAC address (starts at 0x32, only first 4 bytes fit in 54-byte packet)
        packet[0x32:0x36] = self.device.mac_address[:4]

        return bytes(packet)

    def _create_beat_packet(self) -> bytes:
        """
        Create a 96-byte beat packet.

        Packet structure:
        0x00-0x09: Magic header (10 bytes)
        0x0a:      Packet type (0x28 = beat)
        0x0b:      Unknown
        0x0c-0x1f: Device name (20 bytes)
        0x20:      Unknown (0x01)
        0x21:      Device number
        ...
        0x55-0x57: Pitch (3 bytes, big-endian)
        0x58-0x59: Unknown
        0x5a-0x5b: BPM * 100 (2 bytes, big-endian)
        0x5c:      Beat within bar (1-4)
        ...
        """
        packet = bytearray(96)

        # Magic header
        packet[0:10] = MAGIC_HEADER

        # Packet type
        packet[0x0a] = PACKET_TYPE_BEAT

        # Device name (20 bytes)
        name_bytes = self.device.name.encode('ascii')[:20]
        packet[0x0c:0x0c + len(name_bytes)] = name_bytes

        # Device info
        packet[0x20] = 0x01
        packet[0x21] = self.device.device_number

        # Calculate pitch value from percentage
        pitch = int(NEUTRAL_PITCH * (1 + self.device.pitch_percent / 100))

        # Pitch (3 bytes at offset 0x55, big-endian)
        packet[0x55] = (pitch >> 16) & 0xff
        packet[0x56] = (pitch >> 8) & 0xff
        packet[0x57] = pitch & 0xff

        # BPM * 100 (2 bytes at offset 0x5a)
        bpm_value = int(self.device.bpm * 100)
        packet[0x5a] = (bpm_value >> 8) & 0xff
        packet[0x5b] = bpm_value & 0xff

        # Beat within bar (1-4)
        packet[0x5c] = self.beat_in_bar

        # Next beat timing (placeholder)
        # These are milliseconds until next beat/bar

        return bytes(packet)

    def send_announcement(self) -> bool:
        """Send a single device announcement packet."""
        try:
            if not self.announcement_socket:
                self.announcement_socket = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
                self.announcement_socket.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)

            packet = self._create_device_announcement_packet()
            self.announcement_socket.sendto(packet, (self.target_ip, PORT_ANNOUNCEMENT))

            if self.on_announcement_sent:
                self.on_announcement_sent(self.device)

            return True
        except Exception as e:
            print(f"Error sending announcement: {e}")
            return False

    def send_beat(self) -> bool:
        """Send a single beat packet."""
        try:
            if not self.beat_socket:
                self.beat_socket = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

            packet = self._create_beat_packet()
            self.beat_socket.sendto(packet, (self.target_ip, PORT_BEAT))

            if self.on_beat_sent:
                self.on_beat_sent(self.device, self.beat_in_bar)

            # Update beat counter
            self.beat_count += 1
            self.beat_in_bar = (self.beat_in_bar % 4) + 1

            return True
        except Exception as e:
            print(f"Error sending beat: {e}")
            return False

    def start(self, announcement_interval: float = 1.5, beat_count: Optional[int] = None):
        """
        Start the emulator.

        Args:
            announcement_interval: Seconds between announcements (default 1.5)
            beat_count: Number of beats to send (None = infinite)
        """
        self.running = True
        self.beat_count = 0

        # Calculate beat interval from BPM
        effective_bpm = self.device.bpm * (1 + self.device.pitch_percent / 100)
        beat_interval = 60.0 / effective_bpm

        # Start announcement thread
        def announcement_loop():
            while self.running:
                self.send_announcement()
                time.sleep(announcement_interval)

        announcement_thread = threading.Thread(target=announcement_loop, daemon=True)
        announcement_thread.start()

        # Send beats
        try:
            sent = 0
            while self.running:
                if beat_count is not None and sent >= beat_count:
                    break
                self.send_beat()
                sent += 1
                time.sleep(beat_interval)
        except KeyboardInterrupt:
            pass

        self.running = False

    def stop(self):
        """Stop the emulator."""
        self.running = False
        if self.announcement_socket:
            self.announcement_socket.close()
            self.announcement_socket = None
        if self.beat_socket:
            self.beat_socket.close()
            self.beat_socket = None


class MultiDeviceEmulator:
    """Emulate multiple DJ devices simultaneously."""

    def __init__(self):
        self.emulators: list[DJLinkEmulator] = []

    def add_device(self, device: EmulatedDevice, target_ip: str = "127.0.0.1"):
        emulator = DJLinkEmulator(device, target_ip)
        self.emulators.append(emulator)
        return emulator

    def start_all(self):
        """Start all emulators in separate threads."""
        threads = []
        for emulator in self.emulators:
            t = threading.Thread(target=emulator.start, daemon=True)
            t.start()
            threads.append(t)
        return threads

    def stop_all(self):
        """Stop all emulators."""
        for emulator in self.emulators:
            emulator.stop()


def run_quick_test(target_ip: str = "127.0.0.1", beats: int = 8):
    """
    Run a quick test by sending a few beats.

    Returns True if all packets were sent successfully.
    """
    device = EmulatedDevice(
        name="TestCDJ",
        device_number=1,
        bpm=120.0,
        pitch_percent=0.0
    )

    emulator = DJLinkEmulator(device, target_ip)

    announcements_sent = 0
    beats_sent = 0

    def on_announcement(d):
        nonlocal announcements_sent
        announcements_sent += 1

    def on_beat(d, beat_in_bar):
        nonlocal beats_sent
        beats_sent += 1

    emulator.on_announcement_sent = on_announcement
    emulator.on_beat_sent = on_beat

    # Send announcement
    emulator.send_announcement()
    time.sleep(0.1)

    # Send beats
    for _ in range(beats):
        emulator.send_beat()
        time.sleep(0.1)  # ~600 BPM for quick testing

    # Send final announcement
    emulator.send_announcement()

    emulator.stop()

    return announcements_sent >= 1 and beats_sent == beats


def main():
    parser = argparse.ArgumentParser(description="DJ Link Device Emulator")
    parser.add_argument("--device", default="CDJ-3000", help="Device name")
    parser.add_argument("--number", type=int, default=1, help="Device number (1-4)")
    parser.add_argument("--bpm", type=float, default=128.0, help="BPM")
    parser.add_argument("--pitch", type=float, default=0.0, help="Pitch percent")
    parser.add_argument("--beats", type=int, default=None, help="Number of beats to send (default: infinite)")
    parser.add_argument("--target", default="127.0.0.1", help="Target IP address")
    parser.add_argument("--quick-test", action="store_true", help="Run quick test and exit")
    args = parser.parse_args()

    if args.quick_test:
        print("Running quick test...")
        success = run_quick_test(args.target, 8)
        if success:
            print("✓ Quick test passed")
            sys.exit(0)
        else:
            print("✗ Quick test failed")
            sys.exit(1)

    device = EmulatedDevice(
        name=args.device,
        device_number=args.number,
        bpm=args.bpm,
        pitch_percent=args.pitch
    )

    print(f"Starting DJ Link Emulator")
    print(f"  Device: {device.name} (#{device.device_number})")
    print(f"  BPM: {device.bpm} (pitch: {device.pitch_percent:+.1f}%)")
    print(f"  Target: {args.target}")
    print(f"  Sending to ports {PORT_ANNOUNCEMENT}, {PORT_BEAT}")
    print()
    print("Press Ctrl+C to stop")
    print()

    emulator = DJLinkEmulator(device, args.target)

    def on_beat(d, beat_in_bar):
        effective_bpm = d.bpm * (1 + d.pitch_percent / 100)
        print(f"[BEAT] {d.name} | {effective_bpm:.1f} BPM | Beat {beat_in_bar}/4")

    emulator.on_beat_sent = on_beat

    try:
        emulator.start(beat_count=args.beats)
    except KeyboardInterrupt:
        print("\nStopping...")
    finally:
        emulator.stop()

    print("Done")


if __name__ == "__main__":
    main()
