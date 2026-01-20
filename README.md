# Beat-Link C++

[![C++20](https://img.shields.io/badge/C%2B%2B-20-blue.svg)](https://isocpp.org/std/the-standard)
[![License](https://img.shields.io/badge/License-EPL--2.0-green.svg)](LICENSE)

A C++20 implementation of the Pioneer DJ Link protocol.
Communicate with CDJ, XDJ, and DJM devices over the network to retrieve BPM, beats, playback status, track metadata, waveforms, and more.

C++20 による Pioneer DJ Link プロトコルライブラリの実装です。
CDJ、XDJ、DJM などの Pioneer DJ 機器とネットワーク経由で通信し、BPM、ビート、再生状態、トラックメタデータ、波形データなどの情報を取得できます。

## Features / 機能

- **Device Discovery** - Automatic detection of DJ Link devices on the network
- **Beat Synchronization** - Real-time beat and tempo information
- **Track Metadata** - Title, artist, BPM, key, cue points, and more
- **Waveform Data** - Preview and detailed waveforms
- **Album Art** - JPEG/PNG artwork retrieval
- **Python Bindings** - Full-featured Python API (105+ functions)
- **Cross-Platform** - macOS, Linux, Windows support

## Table of Contents / 目次

1. [Requirements / 必要環境](#requirements--必要環境)
2. [Build / ビルド方法](#build--ビルド方法)
3. [Quick Start / クイックスタート](#quick-start--クイックスタート)
4. [Python Usage / Python での使用](#python-usage--python-での使用)
5. [Applications / アプリケーション](#applications--アプリケーション)
6. [API Reference / API リファレンス](#api-reference--api-リファレンス)
7. [Testing / テスト](#testing--テスト)
8. [Troubleshooting / トラブルシューティング](#troubleshooting--トラブルシューティング)

---

## Requirements / 必要環境

- **OS**: macOS, Linux, Windows
- **Compiler**: C++20 compatible (GCC 11+, Clang 14+, MSVC 2022+)
- **CMake**: 3.15+
- **Python**: 3.8+ (for Python bindings)
- **Network**: Same network as DJ Link devices

### Auto-fetched Dependencies / 自動取得される依存ライブラリ

CMake automatically downloads the following libraries:

| Library | Purpose |
|---------|---------|
| Asio | Network I/O (standalone, non-Boost) |
| nanobind | Python bindings |
| miniz | ZIP extraction |
| sqlite3 | Database access |
| kaitai_struct | Binary parsing |
| stb_image | Image decoding (JPEG/PNG) |
| GLFW + ImGui | GUI (optional) |
| Catch2 | Unit testing (optional) |

---

## Build / ビルド方法

### Basic Build / 基本的なビルド

```bash
cd beat-link-cpp
mkdir build && cd build

# Basic build (library + examples)
cmake ..
make -j$(nproc)

# With Python bindings
cmake .. -DBEATLINK_BUILD_PYTHON=ON
make -j$(nproc) beatlink_py

# With unit tests
cmake .. -DBEATLINK_BUILD_TESTS=ON
make -j$(nproc) beatlink_tests
ctest
```

### Build Outputs / ビルド成果物

| File | Description |
|------|-------------|
| `libbeatlink.a` | Static library |
| `beatlink_example` | CLI example |
| `beatlink_gui` | GUI monitor (Dear ImGui) |
| `beatlink_cli` | CLI tool with --schema support |
| `beatlink_py.*.so` | Python module (if enabled) |
| `beatlink_tests` | Unit tests (if enabled) |

---

## Quick Start / クイックスタート

### C++ Example

```cpp
#include <beatlink/BeatLink.hpp>
#include <iostream>

int main() {
    auto& deviceFinder = beatlink::DeviceFinder::getInstance();
    auto& beatFinder = beatlink::BeatFinder::getInstance();

    // Device discovery callback
    deviceFinder.addDeviceFoundListener([](const beatlink::DeviceAnnouncement& device) {
        std::cout << "Found: " << device.getDeviceName()
                  << " (#" << device.getDeviceNumber() << ")\n";
    });

    // Beat callback
    beatFinder.addBeatListener([](const beatlink::Beat& beat) {
        std::cout << "BPM: " << beat.getEffectiveTempo()
                  << " Beat: " << beat.getBeatWithinBar() << "/4\n";
    });

    deviceFinder.start();
    beatFinder.start();

    // Run until Ctrl+C
    std::this_thread::sleep_for(std::chrono::hours(24));

    beatFinder.stop();
    deviceFinder.stop();
    return 0;
}
```

---

## Python Usage / Python での使用

### Installation / インストール

```bash
# Build the Python module
cd build
cmake .. -DBEATLINK_BUILD_PYTHON=ON
make beatlink_py

# The module will be at: build/beatlink_py.cpython-*.so
# Add build directory to PYTHONPATH or copy the .so file
```

### Basic Example / 基本的な使用例

```python
import sys
sys.path.insert(0, './build')  # Add build directory

import beatlink_py as bl

# Define callbacks
def on_beat(beat):
    print(f"Beat: {beat.effective_bpm:.2f} BPM, {beat.beat_in_bar}/4")

def on_device_found(device):
    print(f"Found: {device.device_name} (#{device.device_number})")

# Register listeners
bl.add_beat_listener(on_beat)
bl.add_device_found_listener(on_device_found)

# Start services
bl.start_device_finder()
bl.start_beat_finder()

# Keep running
import time
try:
    while True:
        time.sleep(1)
except KeyboardInterrupt:
    pass

# Cleanup
bl.stop_beat_finder()
bl.stop_device_finder()
bl.clear_all_listeners()
```

### Track Metadata Example / トラックメタデータの取得

```python
import beatlink_py as bl

bl.start_device_finder()
bl.start_virtual_cdj()
bl.start_metadata_finder()

import time
time.sleep(3)  # Wait for connection

# Get metadata for player 1
metadata = bl.get_track_metadata(1)
if metadata:
    print(f"Title:  {metadata.title}")
    print(f"Artist: {metadata.artist}")
    print(f"BPM:    {metadata.tempo / 100:.2f}")
    print(f"Key:    {metadata.key}")

# Get cue points
cues = bl.get_cue_list(1)
if cues:
    for cue in cues.entries:
        cue_type = "Loop" if cue.is_loop else f"HotCue {cue.hot_cue_number}"
        print(f"  {cue_type} @ {cue.cue_time_ms}ms")

# Cleanup
bl.stop_metadata_finder()
bl.stop_virtual_cdj()
bl.stop_device_finder()
```

### Python Examples / サンプルスクリプト

See the `examples/` directory:

- **beat_monitor.py** - Real-time beat visualization
- **track_info.py** - Display track metadata, cue points, beat grid
- **waveform_export.py** - Export waveform and album art data

---

## Applications / アプリケーション

### GUI Monitor / GUI版モニター (beatlink_gui)

グラフィカルな DJ Link モニターアプリケーションです。

```bash
./beatlink_gui
```

#### 画面構成

```
┌─────────────────────────────────────────────────────┐
│ Beat Link C++ v0.1.0                   [CONNECTED] │
├─────────────────────────────────────────────────────┤
│ DJ Link Devices                                     │
├─────────────────────────────────────────────────────┤
│ ▼ CDJ-2000NXS2 (#1)                                │
│   Beat: ● ○ ○ ○                                    │
│   128.5 BPM                                         │
│   Track: 128.0 BPM    Pitch: +0.39%                │
│   Status: PLAYING MASTER SYNC                       │
├─────────────────────────────────────────────────────┤
│ Event Log                                           │
│ 12:34:56 [+] Device found: CDJ-2000NXS2 (#1)       │
└─────────────────────────────────────────────────────┘
```

#### 表示項目

| 項目 | 説明 |
|------|------|
| **Beat** | 4つの円で現在のビート位置を表示。1拍目（ダウンビート）は赤、それ以外は緑で光る |
| **BPM** | 実効BPM（ピッチ調整込み）を大きく緑色で表示 |
| **Track BPM** | トラック本来のBPM |
| **Pitch** | ピッチ調整率（%）。プラスは緑、マイナスは赤で表示 |
| **PLAYING** | 再生中 |
| **MASTER** | テンポマスター（黄色で表示） |
| **SYNC** | シンク有効 |
| **ON AIR** | ミキサーでフェーダーが上がっている状態 |

#### 操作方法

- ウィンドウを閉じるか、`Cmd+Q` (macOS) / `Alt+F4` (Windows) で終了
- 各プレイヤーのヘッダーをクリックで折りたたみ/展開

---

### コマンドライン版 (beatlink_example)

ターミナルで動作するシンプルなモニターです。

```bash
./beatlink_example
```

#### 出力例

```
Beat Link C++ v0.1.0
==============================
Listening for DJ Link devices...

DeviceFinder started on port 50000
BeatFinder started on port 50001

Waiting for devices and beats... Press Ctrl+C to exit.

[DEVICE FOUND] DeviceAnnouncement[device:1, name:CDJ-2000NXS2, address:192.168.1.10, peers:2]
[BEAT] Device 1 | BPM:  128.0 | Beat: 1/4 | Pitch: +0.00%
[BEAT] Device 1 | BPM:  128.0 | Beat: 2/4 | Pitch: +0.00%
[BEAT] Device 1 | BPM:  128.0 | Beat: 3/4 | Pitch: +0.00%
[BEAT] Device 1 | BPM:  128.0 | Beat: 4/4 | Pitch: +0.00%
```

`Ctrl+C` で終了します。

---

## ライブラリの使い方

### 基本的な使用例

```cpp
#include <beatlink/BeatLink.hpp>
#include <iostream>

int main() {
    // シングルトンインスタンスを取得
    auto& deviceFinder = beatlink::DeviceFinder::getInstance();
    auto& beatFinder = beatlink::BeatFinder::getInstance();

    // デバイス発見時のコールバック
    deviceFinder.addDeviceFoundListener([](const beatlink::DeviceAnnouncement& device) {
        std::cout << "発見: " << device.getDeviceName()
                  << " (#" << device.getDeviceNumber() << ")" << std::endl;
    });

    // デバイス消失時のコールバック
    deviceFinder.addDeviceLostListener([](const beatlink::DeviceAnnouncement& device) {
        std::cout << "消失: " << device.getDeviceName() << std::endl;
    });

    // ビート受信時のコールバック
    beatFinder.addBeatListener([](const beatlink::Beat& beat) {
        std::cout << "BPM: " << beat.getEffectiveTempo()
                  << " Beat: " << beat.getBeatWithinBar() << "/4" << std::endl;
    });

    // 開始
    deviceFinder.start();  // ポート 50000 でリッスン
    beatFinder.start();    // ポート 50001 でリッスン

    // メインループ（例）
    while (true) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    // 停止
    beatFinder.stop();
    deviceFinder.stop();

    return 0;
}
```

### CMake でのリンク方法

```cmake
# beatlink をサブディレクトリとして追加
add_subdirectory(path/to/beatlink/cpp)

# 自分のアプリケーション
add_executable(myapp main.cpp)
target_link_libraries(myapp beatlink)
```

---

## API リファレンス

### 主要クラス

#### DeviceFinder

DJ Link デバイスの検出を行います（ポート 50000）。

```cpp
class DeviceFinder {
    static DeviceFinder& getInstance();          // シングルトン取得
    bool start();                                 // 開始
    void stop();                                  // 停止
    bool isRunning() const;                       // 実行中かどうか

    std::vector<DeviceAnnouncement> getCurrentDevices();  // 現在のデバイス一覧

    void addDeviceFoundListener(callback);        // デバイス発見コールバック
    void addDeviceLostListener(callback);         // デバイス消失コールバック
};
```

#### BeatFinder

ビートパケットの検出を行います（ポート 50001）。

```cpp
class BeatFinder {
    static BeatFinder& getInstance();             // シングルトン取得
    bool start();                                 // 開始
    void stop();                                  // 停止
    bool isRunning() const;                       // 実行中かどうか

    void addBeatListener(callback);               // ビート受信コールバック
};
```

#### DeviceAnnouncement

デバイスアナウンスパケットを表します。

```cpp
class DeviceAnnouncement {
    std::string getDeviceName() const;            // デバイス名 (例: "CDJ-2000NXS2")
    int getDeviceNumber() const;                  // デバイス番号 (1-4)
    asio::ip::address_v4 getAddress() const;      // IPアドレス
    std::array<uint8_t, 6> getHardwareAddress() const;  // MACアドレス
    bool isOpusQuad() const;                      // Opus Quad かどうか
};
```

#### Beat

ビートパケットを表します。

```cpp
class Beat {
    int getDeviceNumber() const;                  // デバイス番号
    std::string getDeviceName() const;            // デバイス名
    int getBpm() const;                           // BPM × 100 (例: 12850 = 128.50 BPM)
    double getEffectiveTempo() const;             // 実効BPM（ピッチ込み）
    int getPitch() const;                         // ピッチ生値
    int getBeatWithinBar() const;                 // 小節内位置 (1-4)

    int64_t getNextBeat() const;                  // 次のビートまでの時間(ms)
    int64_t getNextBar() const;                   // 次の小節までの時間(ms)
};
```

#### CdjStatus

CDJ ステータスパケットを表します（より詳細な情報）。

```cpp
class CdjStatus {
    bool isPlaying() const;                       // 再生中
    bool isTempoMaster() const;                   // テンポマスター
    bool isSynced() const;                        // シンク中
    bool isOnAir() const;                         // オンエア
    bool isTrackLoaded() const;                   // トラックロード済み

    int getRekordboxId() const;                   // rekordbox トラックID
    TrackSourceSlot getTrackSourceSlot() const;   // トラックソース (USB/SD/CD等)
    PlayState getPlayState() const;               // 再生状態
};
```

### ユーティリティ関数

```cpp
namespace beatlink {
    class Util {
        // ピッチ変換
        static double pitchToPercentage(int64_t pitch);   // ピッチ → パーセント
        static double pitchToMultiplier(int64_t pitch);   // ピッチ → 倍率

        // バイト操作
        static int64_t bytesToNumber(const uint8_t* buf, size_t start, size_t len);
    };
}
```

### ネットワークポート

| 定数 | 値 | 用途 |
|------|-----|------|
| `Ports::ANNOUNCEMENT` | 50000 | デバイス検出・番号ネゴシエーション |
| `Ports::BEAT` | 50001 | ビートパケット |
| `Ports::UPDATE` | 50002 | CDJ/ミキサー ステータス |

---

## フロー図

代表的な起動順・実行フローを以下にまとめています。

- `../../docs/flow-diagrams.md`

---

## Testing / テスト

### C++ Unit Tests / C++ユニットテスト

Build and run unit tests with Catch2:

```bash
cd build
cmake .. -DBEATLINK_BUILD_TESTS=ON
make beatlink_tests
./beatlink_tests           # Run all tests
./beatlink_tests -l        # List all tests
./beatlink_tests [Beat]    # Run only Beat tests
ctest                      # Run via CTest
```

Test coverage includes:
- **Packet parsing**: Beat, CdjStatus, MixerStatus, DeviceAnnouncement
- **Utility functions**: Pitch conversion, byte operations, time calculations
- **Data structures**: BeatGrid, CueList, Waveform, DataReference

### Python Tests / Pythonテスト

```bash
cd tests
python golden_test.py           # CLI and Python binding tests
python communication_test.py    # Network communication tests
python real_device_test.py      # Real device integration tests
```

### API Documentation / APIドキュメント

Generate Doxygen documentation:

```bash
doxygen Doxyfile
open docs/api/html/index.html
```

---

## Troubleshooting / トラブルシューティング

### Port in use / ポートが使用中

```
Failed to start DeviceFinder (port 50000 may be in use)
```

**原因**: 他のアプリケーション（rekordbox 等）がポートを使用中

**解決策**:
1. rekordbox や他の DJ Link アプリケーションを終了
2. `lsof -i :50000` でポートを使用しているプロセスを確認
3. 必要に応じて該当プロセスを終了

### デバイスが見つからない

**確認事項**:
1. PC と DJ 機器が同じネットワーク（サブネット）に接続されているか
2. ファイアウォールがポート 50000-50002 をブロックしていないか
3. DJ 機器の Link 機能が有効になっているか

### macOS での権限

macOS では、ネットワークアクセスの許可ダイアログが表示される場合があります。「許可」を選択してください。

### ビルドエラー

```
Asio not found
```

**解決策**: CMake が自動的にダウンロードしますが、ネットワーク接続が必要です。オフライン環境では、手動で Asio をインストールしてください：

```bash
# macOS (Homebrew)
brew install asio

# Ubuntu/Debian
sudo apt-get install libasio-dev
```

---

## 対応機器

### 動作確認済み

- CDJ-2000NXS2
- CDJ-3000
- XDJ-1000MK2
- XDJ-XZ
- DJM-900NXS2
- DJM-V10

### 対応予定

- Opus Quad（部分対応）
- rekordbox（PC/Mac）

---

## ライセンス

このライブラリは [Deep Symmetry Beat Link](https://github.com/Deep-Symmetry/beat-link) の C++ ポートです。

---

## バージョン履歴

### v0.1.0

- 初回リリース
- DeviceFinder: デバイス検出
- BeatFinder: ビート検出
- Dear ImGui GUI モニター
