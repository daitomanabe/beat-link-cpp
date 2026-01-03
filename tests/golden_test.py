#!/usr/bin/env python3
"""
Beat Link C++ Golden Tests (Expanded)

Per INTRODUCTION_JAVA_TO_CPP.md Section 7 (Deliverables):
Golden Tests verify the C++ Core via Python bindings and CLI.

Test Categories:
1. CLI Schema Tests - API introspection via --schema
2. CLI JSONL Tests - Structured logging format
3. Python Binding Tests - Core functionality
4. Safety Curtain Tests - Output limiting
5. Edge Case Tests - Error handling

Usage:
    python golden_test.py              # Run all tests
    python golden_test.py --cli-only   # Only test CLI (no Python bindings needed)
    python golden_test.py --verbose    # Verbose output
"""

import subprocess
import json
import sys
import os
import math
from pathlib import Path
from typing import Optional, Any

# Test results
passed = 0
failed = 0
skipped = 0
verbose = "--verbose" in sys.argv or "-v" in sys.argv

def log(msg: str):
    """Print verbose output."""
    if verbose:
        print(f"    {msg}")

def test(name: str, condition: bool, message: str = ""):
    """Record a test result."""
    global passed, failed
    if condition:
        print(f"  ✓ {name}")
        passed += 1
    else:
        print(f"  ✗ {name}: {message}")
        failed += 1

def skip(name: str, reason: str):
    """Skip a test."""
    global skipped
    print(f"  ⊘ {name}: SKIPPED ({reason})")
    skipped += 1

def get_cli_path() -> Optional[Path]:
    """Get path to CLI binary."""
    script_dir = Path(__file__).parent
    build_dir = script_dir.parent / "build"
    cli_path = build_dir / "beatlink_cli"
    return cli_path if cli_path.exists() else None

def run_cli(*args) -> subprocess.CompletedProcess:
    """Run CLI with arguments."""
    cli_path = get_cli_path()
    if not cli_path:
        raise FileNotFoundError("CLI binary not found")
    return subprocess.run(
        [str(cli_path)] + list(args),
        capture_output=True,
        text=True,
        timeout=10
    )

# ============================================================================
# CLI Schema Tests
# ============================================================================

def test_cli_schema():
    """Test that CLI --schema returns valid JSON with required fields."""
    print("\n[CLI Schema Tests]")

    cli_path = get_cli_path()
    if not cli_path:
        skip("CLI binary exists", f"Not found")
        return

    test("CLI binary exists", True)

    try:
        result = run_cli("--schema")
        test("CLI --schema exits successfully", result.returncode == 0,
             f"Exit code: {result.returncode}")

        schema = json.loads(result.stdout)
        test("Schema is valid JSON", True)

        # Required top-level fields
        test("Schema has 'name' field", "name" in schema)
        test("Schema has 'version' field", "version" in schema)
        test("Schema has 'description' field", "description" in schema)
        test("Schema has 'commands' array", isinstance(schema.get("commands"), list))
        test("Schema has 'inputs' array", isinstance(schema.get("inputs"), list))
        test("Schema has 'outputs' array", isinstance(schema.get("outputs"), list))

        # Version check
        test("Version is 0.2.0", schema.get("version") == "0.2.0",
             f"Got: {schema.get('version')}")

        # Name check
        test("Name is 'beatlink'", schema.get("name") == "beatlink",
             f"Got: {schema.get('name')}")

        # Commands structure
        commands = schema.get("commands", [])
        test("Has at least 5 commands", len(commands) >= 5, f"Got {len(commands)}")

        if commands:
            cmd = commands[0]
            test("Command has 'name'", "name" in cmd)
            test("Command has 'description'", "description" in cmd)
            test("Command has 'params' array", isinstance(cmd.get("params"), list))
            test("Command has 'returns'", "returns" in cmd)

        # Required commands
        command_names = [c["name"] for c in commands]
        required_commands = [
            "start_device_finder",
            "stop_device_finder",
            "start_beat_finder",
            "stop_beat_finder",
            "get_devices",
            "add_beat_listener",
            "set_time"  # Time Injection pattern
        ]
        for cmd_name in required_commands:
            test(f"Has '{cmd_name}' command", cmd_name in command_names,
                 f"Missing from: {command_names}")

        # Check set_time has ticks parameter (Time Injection)
        set_time_cmd = next((c for c in commands if c["name"] == "set_time"), None)
        if set_time_cmd:
            params = set_time_cmd.get("params", [])
            param_names = [p["name"] for p in params]
            test("set_time has 'ticks' parameter", "ticks" in param_names,
                 f"Params: {param_names}")

        # Inputs
        inputs = schema.get("inputs", [])
        test("Has at least 1 input", len(inputs) >= 1, f"Got {len(inputs)}")
        if inputs:
            test("Input has 'name'", "name" in inputs[0])
            test("Input has 'format'", "format" in inputs[0])

        # Outputs
        outputs = schema.get("outputs", [])
        test("Has at least 3 outputs", len(outputs) >= 3, f"Got {len(outputs)}")
        output_names = [o["name"] for o in outputs]
        test("Has 'beat_event' output", "beat_event" in output_names)
        test("Has 'device_announcement' output", "device_announcement" in output_names)

    except json.JSONDecodeError as e:
        test("Schema is valid JSON", False, f"Parse error: {e}")
    except subprocess.TimeoutExpired:
        test("CLI --schema completes in time", False, "Timeout")
    except Exception as e:
        test("CLI --schema runs", False, str(e))

def test_cli_help():
    """Test CLI help output."""
    print("\n[CLI Help Tests]")

    if not get_cli_path():
        skip("CLI --help", "CLI binary not found")
        return

    try:
        result = run_cli("--help")
        test("CLI --help exits with 0", result.returncode == 0,
             f"Exit code: {result.returncode}")

        output = result.stdout
        test("Help mentions --schema", "--schema" in output)
        test("Help mentions --listen", "--listen" in output)
        test("Help mentions --json", "--json" in output or "JSON" in output)
        test("Help mentions --human", "--human" in output)
        test("Help shows version", "0.2.0" in output or "Beat Link" in output)

        # Test invalid option
        result = run_cli("--invalid-option-xyz")
        test("Invalid option shows error", result.returncode != 0 or "Unknown" in result.stderr)

    except Exception as e:
        test("CLI help tests", False, str(e))

# ============================================================================
# CLI JSONL Output Tests
# ============================================================================

def test_cli_jsonl():
    """Test CLI JSONL output format."""
    print("\n[CLI JSONL Format Tests]")

    if not get_cli_path():
        skip("CLI JSONL tests", "CLI binary not found")
        return

    # We can't actually test --listen without devices, but we can verify the
    # schema describes the expected JSONL format

    try:
        result = run_cli("--schema")
        schema = json.loads(result.stdout)

        # Check outputs are described as json format
        outputs = schema.get("outputs", [])
        json_outputs = [o for o in outputs if o.get("format") == "json"]
        test("Outputs use JSON format", len(json_outputs) >= 2,
             f"Got {len(json_outputs)} json outputs")

        # Verify beat_event output
        beat_output = next((o for o in outputs if o["name"] == "beat_event"), None)
        test("beat_event output exists", beat_output is not None)
        if beat_output:
            test("beat_event format is json", beat_output.get("format") == "json")

    except Exception as e:
        test("JSONL format tests", False, str(e))

# ============================================================================
# Python Binding Tests
# ============================================================================

def test_python_bindings():
    """Test Python bindings basic functionality."""
    print("\n[Python Binding Tests]")

    try:
        script_dir = Path(__file__).parent
        build_dir = script_dir.parent / "build"
        sys.path.insert(0, str(build_dir))

        try:
            import beatlink_py as bl
            test("Import beatlink_py", True)

            # Version
            test("Has __version__", hasattr(bl, "__version__"))
            test("Version is 0.2.0", getattr(bl, "__version__", "") == "0.2.0",
                 f"Got: {getattr(bl, '__version__', 'N/A')}")

            # describe_api
            api_json = bl.describe_api()
            test("describe_api() returns string", isinstance(api_json, str))
            api = json.loads(api_json)
            test("describe_api() is valid JSON", True)
            test("API name is 'beatlink'", api.get("name") == "beatlink")

            # Port constants
            test("PORT_ANNOUNCEMENT = 50000", bl.PORT_ANNOUNCEMENT == 50000)
            test("PORT_BEAT = 50001", bl.PORT_BEAT == 50001)
            test("PORT_UPDATE = 50002", bl.PORT_UPDATE == 50002)

            # SafetyLimits class
            test("Has SafetyLimits class", hasattr(bl, "SafetyLimits"))
            test("SafetyLimits.MIN_BPM = 20.0", bl.SafetyLimits.MIN_BPM == 20.0)
            test("SafetyLimits.MAX_BPM = 300.0", bl.SafetyLimits.MAX_BPM == 300.0)
            test("SafetyLimits.MIN_PITCH_PERCENT = -100.0",
                 bl.SafetyLimits.MIN_PITCH_PERCENT == -100.0)
            test("SafetyLimits.MAX_PITCH_PERCENT = 100.0",
                 bl.SafetyLimits.MAX_PITCH_PERCENT == 100.0)

            # Utility functions
            test("Has pitch_to_percentage", hasattr(bl, "pitch_to_percentage"))
            test("Has pitch_to_multiplier", hasattr(bl, "pitch_to_multiplier"))

            # Finder functions
            test("Has start_device_finder", hasattr(bl, "start_device_finder"))
            test("Has stop_device_finder", hasattr(bl, "stop_device_finder"))
            test("Has start_beat_finder", hasattr(bl, "start_beat_finder"))
            test("Has stop_beat_finder", hasattr(bl, "stop_beat_finder"))
            test("Has is_device_finder_running", hasattr(bl, "is_device_finder_running"))
            test("Has is_beat_finder_running", hasattr(bl, "is_beat_finder_running"))
            test("Has get_devices", hasattr(bl, "get_devices"))

            # Listener functions
            test("Has add_beat_listener", hasattr(bl, "add_beat_listener"))
            test("Has add_device_found_listener", hasattr(bl, "add_device_found_listener"))
            test("Has add_device_lost_listener", hasattr(bl, "add_device_lost_listener"))

            # Data classes
            test("Has Beat class", hasattr(bl, "Beat"))
            test("Has Device class", hasattr(bl, "Device"))

        except ImportError as e:
            skip("Python bindings", f"Module not found: {e}")
            print("    Build with: cmake .. -DBEATLINK_BUILD_PYTHON=ON && make beatlink_py")

    except Exception as e:
        test("Python bindings", False, str(e))

# ============================================================================
# Safety Curtain Tests (via Python bindings)
# ============================================================================

def test_safety_curtain():
    """Test Safety Curtain functionality."""
    print("\n[Safety Curtain Tests]")

    try:
        script_dir = Path(__file__).parent
        build_dir = script_dir.parent / "build"
        sys.path.insert(0, str(build_dir))

        try:
            import beatlink_py as bl

            # Test pitch_to_percentage with various values
            NEUTRAL_PITCH = 1048576

            # Neutral pitch = 0%
            result = bl.pitch_to_percentage(NEUTRAL_PITCH)
            test("Neutral pitch → 0%", abs(result) < 0.001,
                 f"Got: {result}")

            # +8% pitch
            pitch_plus_8 = int(NEUTRAL_PITCH * 1.08)
            result = bl.pitch_to_percentage(pitch_plus_8)
            test("+8% pitch calculation", abs(result - 8.0) < 0.1,
                 f"Got: {result}")

            # -8% pitch
            pitch_minus_8 = int(NEUTRAL_PITCH * 0.92)
            result = bl.pitch_to_percentage(pitch_minus_8)
            test("-8% pitch calculation", abs(result - (-8.0)) < 0.1,
                 f"Got: {result}")

            # pitch_to_multiplier
            result = bl.pitch_to_multiplier(NEUTRAL_PITCH)
            test("Neutral pitch multiplier = 1.0", abs(result - 1.0) < 0.001,
                 f"Got: {result}")

            result = bl.pitch_to_multiplier(NEUTRAL_PITCH * 2)
            test("2x pitch multiplier = 2.0", abs(result - 2.0) < 0.001,
                 f"Got: {result}")

            result = bl.pitch_to_multiplier(NEUTRAL_PITCH // 2)
            test("0.5x pitch multiplier = 0.5", abs(result - 0.5) < 0.001,
                 f"Got: {result}")

            # Verify SafetyLimits values are sensible
            test("MIN_BPM < MAX_BPM", bl.SafetyLimits.MIN_BPM < bl.SafetyLimits.MAX_BPM)
            test("MIN_BPM > 0", bl.SafetyLimits.MIN_BPM > 0)
            test("MAX_BPM < 1000", bl.SafetyLimits.MAX_BPM < 1000)
            test("Pitch range is symmetric",
                 abs(bl.SafetyLimits.MIN_PITCH_PERCENT) == bl.SafetyLimits.MAX_PITCH_PERCENT)

        except ImportError:
            skip("Safety Curtain tests", "Python bindings not available")

    except Exception as e:
        test("Safety Curtain tests", False, str(e))

# ============================================================================
# Edge Case / Negative Tests
# ============================================================================

def test_edge_cases():
    """Test edge cases and error handling."""
    print("\n[Edge Case Tests]")

    try:
        script_dir = Path(__file__).parent
        build_dir = script_dir.parent / "build"
        sys.path.insert(0, str(build_dir))

        try:
            import beatlink_py as bl

            # Extreme pitch values
            NEUTRAL_PITCH = 1048576

            # Zero pitch
            result = bl.pitch_to_percentage(0)
            test("Zero pitch doesn't crash", True)
            log(f"Zero pitch result: {result}")

            # Very large pitch
            result = bl.pitch_to_percentage(NEUTRAL_PITCH * 100)
            test("Large pitch doesn't crash", True)
            log(f"Large pitch result: {result}")

            # Negative pitch (shouldn't happen but test anyway)
            result = bl.pitch_to_percentage(-1000)
            test("Negative pitch doesn't crash", True)
            log(f"Negative pitch result: {result}")

            # Test multiplier with zero
            result = bl.pitch_to_multiplier(0)
            test("Zero pitch multiplier doesn't crash", True)
            test("Zero pitch multiplier = 0", result == 0.0, f"Got: {result}")

            # Verify describe_api is consistent
            api1 = bl.describe_api()
            api2 = bl.describe_api()
            test("describe_api() is consistent", api1 == api2)

            # Test finder state before starting
            test("Device finder not running initially",
                 not bl.is_device_finder_running())
            test("Beat finder not running initially",
                 not bl.is_beat_finder_running())

            # Test get_devices returns empty list initially
            devices = bl.get_devices()
            test("get_devices() returns list", isinstance(devices, list))
            test("No devices initially", len(devices) == 0, f"Got {len(devices)} devices")

        except ImportError:
            skip("Edge case tests", "Python bindings not available")

    except Exception as e:
        test("Edge case tests", False, str(e))

# ============================================================================
# Integration Tests
# ============================================================================

def test_integration():
    """Test integration between components."""
    print("\n[Integration Tests]")

    try:
        script_dir = Path(__file__).parent
        build_dir = script_dir.parent / "build"
        sys.path.insert(0, str(build_dir))

        try:
            import beatlink_py as bl

            # Verify CLI and Python bindings report same version
            cli_result = run_cli("--schema")
            cli_schema = json.loads(cli_result.stdout)
            py_schema = json.loads(bl.describe_api())

            test("CLI and Python report same version",
                 cli_schema.get("version") == py_schema.get("version"),
                 f"CLI: {cli_schema.get('version')}, Python: {py_schema.get('version')}")

            test("CLI and Python report same name",
                 cli_schema.get("name") == py_schema.get("name"))

            test("CLI and Python have same number of commands",
                 len(cli_schema.get("commands", [])) == len(py_schema.get("commands", [])),
                 f"CLI: {len(cli_schema.get('commands', []))}, "
                 f"Python: {len(py_schema.get('commands', []))}")

            # Verify port constants match schema description
            schema_desc = py_schema.get("description", "")
            test("Schema mentions UDP", "UDP" in schema_desc or "udp" in schema_desc.lower())

        except ImportError:
            skip("Integration tests", "Python bindings not available")
        except FileNotFoundError:
            skip("Integration tests", "CLI binary not found")

    except Exception as e:
        test("Integration tests", False, str(e))

# ============================================================================
# Performance / Smoke Tests
# ============================================================================

def test_performance():
    """Basic performance and smoke tests."""
    print("\n[Performance Tests]")

    import time

    try:
        script_dir = Path(__file__).parent
        build_dir = script_dir.parent / "build"
        sys.path.insert(0, str(build_dir))

        try:
            import beatlink_py as bl

            # describe_api should be fast
            start = time.time()
            for _ in range(100):
                bl.describe_api()
            elapsed = time.time() - start
            test("100x describe_api() < 100ms", elapsed < 0.1,
                 f"Took {elapsed*1000:.1f}ms")

            # pitch_to_percentage should be very fast
            start = time.time()
            for _ in range(10000):
                bl.pitch_to_percentage(1048576)
            elapsed = time.time() - start
            test("10000x pitch_to_percentage() < 50ms", elapsed < 0.05,
                 f"Took {elapsed*1000:.1f}ms")

            # get_devices should be fast even with empty list
            start = time.time()
            for _ in range(1000):
                bl.get_devices()
            elapsed = time.time() - start
            test("1000x get_devices() < 100ms", elapsed < 0.1,
                 f"Took {elapsed*1000:.1f}ms")

        except ImportError:
            skip("Performance tests", "Python bindings not available")

    except Exception as e:
        test("Performance tests", False, str(e))

# ============================================================================
# Main
# ============================================================================

def main():
    global passed, failed, skipped

    print("=" * 70)
    print("Beat Link C++ Golden Tests (Expanded)")
    print("=" * 70)

    cli_only = "--cli-only" in sys.argv

    # CLI tests (always run)
    test_cli_schema()
    test_cli_help()
    test_cli_jsonl()

    # Python binding tests
    if not cli_only:
        test_python_bindings()
        test_safety_curtain()
        test_edge_cases()
        test_integration()
        test_performance()
    else:
        print("\n[Python Tests]")
        skip("All Python tests", "--cli-only specified")

    # Summary
    total = passed + failed + skipped
    print("\n" + "=" * 70)
    print(f"Results: {passed} passed, {failed} failed, {skipped} skipped (total: {total})")
    print("=" * 70)

    if failed > 0:
        print("\n⚠ Some tests failed!")
        return 1
    elif skipped > 0 and passed == 0:
        print("\n⚠ All tests skipped - build the project first")
        return 1
    else:
        print("\n✓ All tests passed!")
        return 0

if __name__ == "__main__":
    sys.exit(main())
