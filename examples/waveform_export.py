#!/usr/bin/env python3
"""
Waveform Export - Export waveform and album art data

This example demonstrates how to:
- Retrieve waveform preview and detail data
- Export album art as image files
- Save waveform data as JSON or render to PNG

Usage:
    python waveform_export.py [player_number] [output_dir]

    player_number: 1-4 (default: 1)
    output_dir: Directory to save files (default: ./export)

Requirements:
    - beatlink_py module
    - Optional: Pillow (PIL) for image export
"""

import sys
import os
import json
import time
import argparse

sys.path.insert(0, '../build')

try:
    import beatlink_py as bl
except ImportError:
    print("Error: beatlink_py module not found.")
    print("Build with: cmake .. -DBEATLINK_BUILD_PYTHON=ON && make beatlink_py")
    sys.exit(1)

# Try to import PIL for image handling
try:
    from PIL import Image
    HAS_PIL = True
except ImportError:
    HAS_PIL = False
    print("Note: Pillow not installed. Image export will be limited.")

def export_album_art(player, output_dir):
    """Export album art as image file."""
    art = bl.get_album_art(player)
    if not art:
        print(f"  No album art available for player {player}")
        return None

    filename = os.path.join(output_dir, f"player{player}_artwork.{art.format}")

    # Write raw bytes directly
    with open(filename, 'wb') as f:
        f.write(bytes(art.raw_bytes))

    print(f"  Saved album art: {filename} ({art.width}x{art.height} {art.format.upper()})")
    return filename

def export_waveform_json(player, output_dir):
    """Export waveform data as JSON."""
    preview = bl.get_waveform_preview(player)
    detail = bl.get_waveform_detail(player)

    data = {
        "player": player,
        "preview": None,
        "detail": None
    }

    if preview:
        data["preview"] = {
            "segment_count": preview.segment_count,
            "max_height": preview.max_height,
            "is_color": preview.is_color,
            "data": list(preview.data)
        }
        print(f"  Waveform preview: {preview.segment_count} segments")

    if detail:
        data["detail"] = {
            "frame_count": detail.frame_count,
            "is_color": detail.is_color,
            "data": list(detail.data)
        }
        print(f"  Waveform detail: {detail.frame_count} frames")

    if preview or detail:
        filename = os.path.join(output_dir, f"player{player}_waveform.json")
        with open(filename, 'w') as f:
            json.dump(data, f, indent=2)
        print(f"  Saved waveform data: {filename}")
        return filename
    else:
        print(f"  No waveform data available for player {player}")
        return None

def export_waveform_image(player, output_dir):
    """Export waveform preview as PNG image."""
    if not HAS_PIL:
        print("  Skipping waveform image (Pillow not installed)")
        return None

    preview = bl.get_waveform_preview(player)
    if not preview or preview.segment_count == 0:
        print(f"  No waveform preview available for player {player}")
        return None

    # Create waveform image
    width = preview.segment_count
    height = 128  # Standard height
    img = Image.new('RGB', (width, height), color=(20, 20, 30))
    pixels = img.load()

    # Draw waveform
    data = preview.data
    bytes_per_segment = len(data) // preview.segment_count if preview.segment_count > 0 else 1

    for x in range(width):
        if preview.is_color and bytes_per_segment >= 6:
            # Color waveform: RGB values for front and back
            idx = x * bytes_per_segment
            if idx + 5 < len(data):
                # Front half (bottom)
                r_front = data[idx]
                g_front = data[idx + 1]
                b_front = data[idx + 2]
                # Back half (top)
                r_back = data[idx + 3]
                g_back = data[idx + 4]
                b_back = data[idx + 5]

                # Draw from center
                center = height // 2
                front_height = min(data[idx] if bytes_per_segment > 6 else 31, height // 2)
                back_height = min(data[idx + 3] if bytes_per_segment > 6 else 31, height // 2)

                # Draw front (below center)
                for y in range(center, min(center + front_height * 2, height)):
                    pixels[x, y] = (r_front, g_front, b_front)

                # Draw back (above center)
                for y in range(max(center - back_height * 2, 0), center):
                    pixels[x, y] = (r_back, g_back, b_back)
        else:
            # Monochrome waveform
            idx = x * bytes_per_segment
            if idx < len(data):
                h = data[idx]
                # Scale height
                bar_height = int(h * height / (preview.max_height if preview.max_height > 0 else 31))
                bar_height = min(bar_height, height)

                # Draw blue bar from center
                center = height // 2
                color = (0, 100, 255)  # Blue
                for y in range(center - bar_height // 2, center + bar_height // 2):
                    if 0 <= y < height:
                        pixels[x, y] = color

    filename = os.path.join(output_dir, f"player{player}_waveform.png")
    img.save(filename)
    print(f"  Saved waveform image: {filename} ({width}x{height})")
    return filename

def export_beat_grid(player, output_dir):
    """Export beat grid as JSON."""
    grid = bl.get_beat_grid(player)
    if not grid:
        print(f"  No beat grid available for player {player}")
        return None

    data = {
        "player": player,
        "beat_count": grid.beat_count,
        "beats": [
            {
                "beat_number": b.beat_number,
                "beat_within_bar": b.beat_within_bar,
                "bpm": b.bpm / 100.0,
                "time_ms": b.time_ms
            }
            for b in grid.beats
        ]
    }

    filename = os.path.join(output_dir, f"player{player}_beatgrid.json")
    with open(filename, 'w') as f:
        json.dump(data, f, indent=2)
    print(f"  Saved beat grid: {filename} ({grid.beat_count} beats)")
    return filename

def export_cue_list(player, output_dir):
    """Export cue list as JSON."""
    cues = bl.get_cue_list(player)
    if not cues:
        print(f"  No cue list available for player {player}")
        return None

    data = {
        "player": player,
        "hot_cue_count": cues.hot_cue_count,
        "memory_point_count": cues.memory_point_count,
        "loop_count": cues.loop_count,
        "entries": [
            {
                "hot_cue_number": c.hot_cue_number,
                "is_loop": c.is_loop,
                "cue_time_ms": c.cue_time_ms,
                "loop_time_ms": c.loop_time_ms if c.is_loop else None,
                "comment": c.comment,
                "color": c.color_name
            }
            for c in cues.entries
        ]
    }

    filename = os.path.join(output_dir, f"player{player}_cues.json")
    with open(filename, 'w') as f:
        json.dump(data, f, indent=2)
    print(f"  Saved cue list: {filename} ({len(cues.entries)} entries)")
    return filename

def main():
    parser = argparse.ArgumentParser(description='Export waveform and track data from DJ players')
    parser.add_argument('player', type=int, nargs='?', default=1,
                        help='Player number (1-4, default: 1)')
    parser.add_argument('output_dir', nargs='?', default='./export',
                        help='Output directory (default: ./export)')
    args = parser.parse_args()

    print("=" * 60)
    print("Beat Link - Waveform Export")
    print(f"Version: {bl.__version__}")
    print("=" * 60)

    # Create output directory
    os.makedirs(args.output_dir, exist_ok=True)
    print(f"\nOutput directory: {os.path.abspath(args.output_dir)}")

    # Start services
    print("\nStarting services...")
    if not bl.start_device_finder():
        print("Error: Failed to start device finder")
        return 1

    if not bl.start_virtual_cdj():
        print("Error: Failed to start virtual CDJ")
        bl.stop_device_finder()
        return 1

    bl.start_metadata_finder()
    bl.start_beat_grid_finder()
    bl.start_waveform_finder()
    bl.start_art_finder()

    print(f"Waiting for data from player {args.player}...")
    time.sleep(3)

    # Check for devices
    devices = bl.get_devices()
    if not devices:
        print("\nNo devices found on the network.")
        print("Make sure CDJ/XDJ devices are connected and on the same network.")
    else:
        print(f"\nFound {len(devices)} device(s):")
        for d in devices:
            print(f"  - {d.device_name} (#{d.device_number})")

    # Export data
    print(f"\nExporting data for player {args.player}...")

    export_album_art(args.player, args.output_dir)
    export_waveform_json(args.player, args.output_dir)
    export_waveform_image(args.player, args.output_dir)
    export_beat_grid(args.player, args.output_dir)
    export_cue_list(args.player, args.output_dir)

    # Cleanup
    print("\nStopping services...")
    bl.stop_art_finder()
    bl.stop_waveform_finder()
    bl.stop_beat_grid_finder()
    bl.stop_metadata_finder()
    bl.stop_virtual_cdj()
    bl.stop_device_finder()

    print("\nDone!")
    return 0

if __name__ == "__main__":
    sys.exit(main())
