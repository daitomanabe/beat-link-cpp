# Beat Link C++

[![C++20](https://img.shields.io/badge/C%2B%2B-20-blue.svg)](https://isocpp.org/std/the-standard)
[![License: EPL-2.0](https://img.shields.io/badge/License-EPL--2.0-green.svg)](LICENSE.md)
[![Python 3.8+](https://img.shields.io/badge/Python-3.8%2B-blue.svg)](https://www.python.org/)

**[English README](README.md)**

Pioneer DJ Link プロトコルの C++20 実装。CDJ、XDJ、DJM 機器とのリアルタイム通信を実現します。

---

## 概要

**Beat Link C++** は、[Deep Symmetry](https://deepsymmetry.org) の [James Elliott](https://github.com/brunchboy) 氏が開発した [Java Beat Link](https://github.com/Deep-Symmetry/beat-link) ライブラリの C++20 移植版です。

本ポートは **[真鍋大度](https://daito.ws)** / **[Rhizomatiks](https://rhizomatiks.com)** により開発されました。ネイティブ C++ アプリケーションや Python スクリプトから DJ Link プロトコルを通じて Pioneer DJ 機器と通信することを可能にします。

### 機能一覧

| 機能 | 説明 |
|------|------|
| **デバイス検出** | ネットワーク上の CDJ、XDJ、DJM を自動検出 |
| **ビート同期** | リアルタイムのビートタイミング、BPM、小節位置 |
| **トラックメタデータ** | タイトル、アーティスト、アルバム、ジャンル、キー、長さ、レーティング |
| **キューポイント** | ホットキュー、メモリーポイント、ループ（カラー・コメント付き） |
| **波形** | プレビュー波形と詳細波形データ |
| **ビートグリッド** | ビート位置とテンポ変化 |
| **アルバムアート** | JPEG/PNG アートワークの取得 |
| **プレイヤー制御** | テンポマスター、シンク、ピッチコントロール |
| **Python バインディング** | フル機能の Python API（105以上の関数） |
| **クロスプラットフォーム** | macOS、Linux、Windows 対応 |

---

## 目次

1. [インストール](#インストール)
2. [クイックスタート](#クイックスタート)
3. [アーキテクチャ](#アーキテクチャ)
4. [C++ API リファレンス](#c-api-リファレンス)
5. [Python API リファレンス](#python-api-リファレンス)
6. [プロトコルリファレンス](#プロトコルリファレンス)
7. [サンプル](#サンプル)
8. [テスト](#テスト)
9. [トラブルシューティング](#トラブルシューティング)
10. [ライセンス](#ライセンス)

---

## インストール

### 動作要件

- **OS**: macOS 10.15 以降、Linux（Ubuntu 20.04 以降）、Windows 10 以降
- **コンパイラ**: C++20 対応
  - GCC 11 以降
  - Clang 14 以降
  - MSVC 2022 以降
- **CMake**: 3.15 以降
- **Python**: 3.8 以降（Python バインディング用）
- **ネットワーク**: DJ Link 機器と同じ LAN 内

### ソースからビルド

```bash
git clone https://github.com/daitomanabe/beat-link-cpp.git
cd beat-link-cpp
mkdir build && cd build

# 標準ビルド
cmake ..
make -j$(nproc)

# Python バインディング付き
cmake .. -DBEATLINK_BUILD_PYTHON=ON
make beatlink_py

# ユニットテスト付き
cmake .. -DBEATLINK_BUILD_TESTS=ON
make beatlink_tests
```

### ビルドオプション

| オプション | デフォルト | 説明 |
|----------|---------|------|
| `BEATLINK_BUILD_PYTHON` | OFF | Python バインディングをビルド |
| `BEATLINK_BUILD_TESTS` | OFF | ユニットテストをビルド |
| `BEATLINK_USE_SQLCIPHER` | OFF | 暗号化データベース用に SQLCipher を使用 |

### ビルド成果物

| ファイル | 説明 |
|---------|------|
| `libbeatlink.a` | スタティックライブラリ |
| `beatlink_example` | シンプルなビートリスナーのサンプル |
| `beatlink_gui` | GUI モニター（Dear ImGui 使用） |
| `beatlink_cli` | `--schema` 対応の CLI ツール |
| `beatlink_py.*.so` | Python モジュール |
| `beatlink_tests` | ユニットテスト |

---

## クイックスタート

### C++ - 最小サンプル

```cpp
#include <beatlink/BeatLink.hpp>
#include <iostream>
#include <thread>

int main() {
    using namespace beatlink;

    // シングルトンインスタンスを取得
    auto& deviceFinder = DeviceFinder::getInstance();
    auto& beatFinder = BeatFinder::getInstance();

    // コールバックを登録
    deviceFinder.addDeviceFoundListener([](const DeviceAnnouncement& d) {
        std::cout << "発見: " << d.getDeviceName()
                  << " (#" << d.getDeviceNumber() << ")\n";
    });

    beatFinder.addBeatListener([](const Beat& b) {
        std::cout << "ビート " << b.getBeatWithinBar() << "/4 @ "
                  << b.getEffectiveTempo() << " BPM\n";
    });

    // サービスを開始
    deviceFinder.start();
    beatFinder.start();

    // 60秒間実行
    std::this_thread::sleep_for(std::chrono::seconds(60));

    // クリーンアップ
    beatFinder.stop();
    deviceFinder.stop();
    return 0;
}
```

### Python - 最小サンプル

```python
import beatlink_py as bl
import time

def on_beat(beat):
    print(f"ビート {beat.beat_in_bar}/4 @ {beat.effective_bpm:.2f} BPM")

def on_device(device):
    print(f"発見: {device.device_name} (#{device.device_number})")

bl.add_beat_listener(on_beat)
bl.add_device_found_listener(on_device)

bl.start_device_finder()
bl.start_beat_finder()

try:
    while True:
        time.sleep(1)
except KeyboardInterrupt:
    pass

bl.stop_beat_finder()
bl.stop_device_finder()
bl.clear_all_listeners()
```

---

## アーキテクチャ

### コンポーネント概要

```
┌─────────────────────────────────────────────────────────────────┐
│                   Beat Link C++ アーキテクチャ                    │
├─────────────────────────────────────────────────────────────────┤
│                                                                  │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐          │
│  │ DeviceFinder │  │  BeatFinder  │  │  VirtualCdj  │          │
│  │  (UDP 50000) │  │  (UDP 50001) │  │  (UDP 50002) │          │
│  └──────┬───────┘  └──────┬───────┘  └──────┬───────┘          │
│         │                 │                 │                    │
│         ▼                 ▼                 ▼                    │
│  ┌─────────────────────────────────────────────────────┐        │
│  │                   リスナーシステム                    │        │
│  │  DeviceAnnouncementListener, BeatListener,          │        │
│  │  DeviceUpdateListener, MasterListener 等            │        │
│  └─────────────────────────────────────────────────────┘        │
│                                                                  │
│  ┌─────────────────────────────────────────────────────┐        │
│  │                   データファインダー                  │        │
│  │  MetadataFinder, WaveformFinder, BeatGridFinder,    │        │
│  │  ArtFinder, TimeFinder, SignatureFinder             │        │
│  └──────────────────────┬──────────────────────────────┘        │
│                         │                                        │
│                         ▼                                        │
│  ┌─────────────────────────────────────────────────────┐        │
│  │              DBServer (TCP 12523)                    │        │
│  │  プレイヤーのデータベースからトラックメタデータを取得    │        │
│  └──────────────────────┬──────────────────────────────┘        │
│                         │                                        │
│                         ▼                                        │
│  ┌─────────────────────────────────────────────────────┐        │
│  │                    データ型                          │        │
│  │  TrackMetadata, BeatGrid, CueList, WaveformPreview, │        │
│  │  WaveformDetail, AlbumArt, DataReference            │        │
│  └─────────────────────────────────────────────────────┘        │
│                                                                  │
└─────────────────────────────────────────────────────────────────┘
```

### サービス起動順序

```cpp
// フル機能を使用するための推奨起動順序:

// 1. デバイス検出（必須）
DeviceFinder::getInstance().start();

// 2. ビート監視（オプション）
BeatFinder::getInstance().start();

// 3. Virtual CDJ - プレイヤーとしてネットワークに参加（メタデータ取得に必須）
VirtualCdj::getInstance().start();

// 4. メタデータサービス（VirtualCdj が必要）
data::MetadataFinder::getInstance().start();
data::BeatGridFinder::getInstance().start();
data::WaveformFinder::getInstance().start();
data::ArtFinder::getInstance().start();
data::TimeFinder::getInstance().start();
```

---

## C++ API リファレンス

### コアクラス

#### DeviceFinder

ネットワーク上の DJ Link デバイスを検出します（UDP ポート 50000）。

```cpp
class DeviceFinder {
    static DeviceFinder& getInstance();

    bool start();
    void stop();
    bool isRunning() const;

    std::vector<DeviceAnnouncement> getCurrentDevices() const;

    void addDeviceFoundListener(DeviceAnnouncementListenerPtr);
    void addDeviceLostListener(DeviceAnnouncementListenerPtr);
    void removeDeviceFoundListener(DeviceAnnouncementListenerPtr);
    void removeDeviceLostListener(DeviceAnnouncementListenerPtr);
};
```

#### BeatFinder

プレイヤーからビートパケットを受信します（UDP ポート 50001）。

```cpp
class BeatFinder {
    static BeatFinder& getInstance();

    bool start();
    void stop();
    bool isRunning() const;

    void addBeatListener(BeatListenerPtr);
    void removeBeatListener(BeatListenerPtr);
};
```

#### VirtualCdj

仮想プレイヤーとして DJ Link ネットワークに参加します（UDP ポート 50002）。
メタデータ取得に必要です。

```cpp
class VirtualCdj {
    static VirtualCdj& getInstance();

    bool start();
    void stop();
    bool isRunning() const;

    int getDeviceNumber() const;
    void setDeviceNumber(int number);

    void setTempo(double bpm);
    void becomeTempMaster();
    void sendSyncCommand(bool sync);

    void addUpdateListener(DeviceUpdateListenerPtr);
    void addMasterListener(MasterListenerPtr);
};
```

### データ型

#### Beat

```cpp
class Beat {
    int getDeviceNumber() const;
    std::string getDeviceName() const;

    int getBpm() const;              // BPM * 100（例: 12850 = 128.50 BPM）
    int getPitch() const;            // 生のピッチ値
    double getEffectiveTempo() const; // ピッチ補正後の BPM

    int getBeatWithinBar() const;    // 1-4
    bool isBeatWithinBarMeaningful() const;

    int64_t getNextBeat() const;     // 次のビートまでの ms
    int64_t getNextBar() const;      // 次の小節までの ms
    int64_t getSecondBeat() const;
    int64_t getFourthBeat() const;
    int64_t getEighthBeat() const;
};
```

#### CdjStatus

```cpp
class CdjStatus {
    int getDeviceNumber() const;
    std::string getDeviceName() const;

    // 再生状態
    bool isPlaying() const;
    bool isAtCue() const;
    bool isTrackLoaded() const;
    PlayState getPlayState() const;

    // テンポ
    int getBpm() const;
    int getPitch() const;
    double getEffectiveTempo() const;
    int getBeatWithinBar() const;

    // シンク
    bool isTempoMaster() const;
    bool isSynced() const;
    bool isOnAir() const;

    // トラックソース
    int getTrackSourcePlayer() const;
    TrackSourceSlot getTrackSourceSlot() const;
    TrackType getTrackType() const;
    int getRekordboxId() const;
};
```

#### TrackMetadata

```cpp
class TrackMetadata {
    std::string getTitle() const;
    std::string getArtist() const;
    std::string getAlbum() const;
    std::string getGenre() const;
    std::string getLabel() const;
    std::string getComment() const;
    std::string getKey() const;
    std::string getOriginalArtist() const;
    std::string getRemixer() const;
    std::string getDateAdded() const;

    int getTempo() const;           // BPM * 100
    int getDuration() const;        // 秒
    int getRating() const;          // 0-5
    int getYear() const;
    int getTrackNumber() const;
    int getBitrate() const;
    int getPlayCount() const;

    int getColorId() const;
    std::string getColorName() const;
};
```

#### CueList

```cpp
class CueList {
    struct Entry {
        int hotCueNumber;           // 0 = メモリーポイント、1-8 = ホットキュー
        bool isLoop;
        int64_t cueTimeMs;
        int64_t loopEndMs;          // ループの場合のみ
        int colorId;
        std::string comment;
    };

    int getHotCueCount() const;
    int getMemoryPointCount() const;
    int getLoopCount() const;
    const std::vector<Entry>& getEntries() const;
};
```

#### BeatGrid

```cpp
class BeatGrid {
    int getBeatCount() const;
    int getBpm(int beatNumber) const;         // そのビートでの BPM * 100
    int getBeatWithinBar(int beatNumber) const;
    int64_t getTimeWithinTrack(int beatNumber) const;  // ms

    int findBeatAtTime(int64_t timeMs) const;
};
```

#### WaveformPreview / WaveformDetail

```cpp
class WaveformPreview {
    int getSegmentCount() const;
    int getMaxHeight() const;
    bool isColor() const;
    std::span<const uint8_t> getData() const;
};

class WaveformDetail {
    int getFrameCount() const;
    bool isColor() const;
    std::span<const uint8_t> getData() const;
};
```

### データファインダー

```cpp
namespace data {
    // プレイヤーのトラックメタデータを取得
    MetadataFinder::getInstance().getLatestMetadataFor(int player);

    // ビートグリッドを取得
    BeatGridFinder::getInstance().getLatestBeatGridFor(int player);

    // 波形を取得
    WaveformFinder::getInstance().getLatestPreviewFor(int player);
    WaveformFinder::getInstance().getLatestDetailFor(int player);

    // アルバムアートを取得
    ArtFinder::getInstance().getLatestArtFor(int player);
}
```

### ユーティリティ関数

```cpp
namespace beatlink {
    class Util {
        // ピッチ変換
        static double pitchToPercentage(int64_t pitch);
        static double pitchToMultiplier(int64_t pitch);
        static int percentageToPitch(double percentage);

        // 時間変換
        static int64_t halfFrameToTime(int64_t halfFrame);
        static int64_t timeToHalfFrame(int64_t timeMs);

        // バイト操作
        static int64_t bytesToNumber(const uint8_t* buf, size_t start, size_t len);
        static std::string formatMac(const std::array<uint8_t, 6>& mac);
    };
}
```

---

## Python API リファレンス

### モジュール: `beatlink_py`

```python
import beatlink_py as bl

# バージョン
print(bl.__version__)  # "0.2.0"
```

### サービス制御

```python
# デバイス検出
bl.start_device_finder()        # デバイス検出を開始
bl.stop_device_finder()         # デバイス検出を停止
bl.is_device_finder_running()   # 実行中か確認

# ビート監視
bl.start_beat_finder()
bl.stop_beat_finder()
bl.is_beat_finder_running()

# Virtual CDJ（メタデータ取得に必要）
bl.start_virtual_cdj()
bl.stop_virtual_cdj()
bl.is_virtual_cdj_running()

# メタデータサービス
bl.start_metadata_finder()
bl.stop_metadata_finder()
bl.start_beat_grid_finder()
bl.stop_beat_grid_finder()
bl.start_waveform_finder()
bl.stop_waveform_finder()
bl.start_art_finder()
bl.stop_art_finder()

# Opus プロバイダー（オフラインモード）
bl.start_opus_provider()
bl.stop_opus_provider()
```

### データ取得

```python
# 接続中のデバイスを取得
devices = bl.get_devices()
for d in devices:
    print(f"{d.device_name} #{d.device_number} @ {d.address}")

# トラックメタデータを取得（プレイヤー 1-4）
metadata = bl.get_track_metadata(1)
if metadata:
    print(f"タイトル: {metadata.title}")
    print(f"アーティスト: {metadata.artist}")
    print(f"BPM: {metadata.tempo / 100:.2f}")
    print(f"キー: {metadata.key}")

# CDJ ステータスを取得
status = bl.get_cdj_status(1)
if status:
    print(f"再生中: {status.is_playing}")
    print(f"マスター: {status.is_tempo_master}")
    print(f"BPM: {status.effective_tempo:.2f}")

# キューリストを取得
cues = bl.get_cue_list(1)
if cues:
    for entry in cues.entries:
        print(f"キュー @ {entry.cue_time_ms}ms - {entry.color_name}")

# ビートグリッドを取得
grid = bl.get_beat_grid(1)
if grid:
    print(f"ビート数: {grid.beat_count}")
    for beat in grid.beats[:10]:
        print(f"ビート {beat.beat_number}: {beat.bpm/100:.2f} BPM @ {beat.time_ms}ms")

# 波形を取得
preview = bl.get_waveform_preview(1)
if preview:
    print(f"セグメント数: {preview.segment_count}")
    print(f"カラー: {preview.is_color}")

# アルバムアートを取得
art = bl.get_album_art(1)
if art:
    print(f"サイズ: {art.width}x{art.height}")
    # ファイルに保存
    with open('artwork.jpg', 'wb') as f:
        f.write(bytes(art.raw_bytes))
```

### イベントリスナー

```python
# ビートリスナー
def on_beat(beat):
    print(f"ビート {beat.beat_in_bar}/4 @ {beat.effective_bpm:.2f} BPM")

bl.add_beat_listener(on_beat)
bl.remove_beat_listener(on_beat)
bl.clear_beat_listeners()

# デバイスリスナー
def on_device_found(device):
    print(f"発見: {device.device_name}")

def on_device_lost(device):
    print(f"ロスト: {device.device_name}")

bl.add_device_found_listener(on_device_found)
bl.add_device_lost_listener(on_device_lost)
bl.clear_device_found_listeners()
bl.clear_device_lost_listeners()

# メタデータリスナー
def on_metadata(update):
    if update.metadata:
        print(f"プレイヤー {update.player}: {update.metadata.title}")

bl.add_track_metadata_listener(on_metadata)
bl.clear_track_metadata_listeners()

# すべてのリスナーをクリア
bl.clear_all_listeners()
```

### VirtualCdj 制御

```python
# デバイス番号の取得/設定
num = bl.get_device_number()
bl.set_device_number(5)

# テンポ制御
bl.set_tempo(128.0)
bl.become_tempo_master()
bl.send_sync_command(True)
```

### Python データ型

```python
# PyBeat
beat.device_number      # int
beat.device_name        # str
beat.bpm               # int (BPM * 100)
beat.effective_bpm     # float
beat.pitch             # int
beat.beat_in_bar       # int (1-4)
beat.next_beat_ms      # int

# PyDeviceAnnouncement
device.device_number   # int
device.device_name     # str
device.address         # str

# PyTrackMetadata
metadata.title         # str
metadata.artist        # str
metadata.album         # str
metadata.genre         # str
metadata.key           # str
metadata.tempo         # int (BPM * 100)
metadata.duration      # int (秒)
metadata.rating        # int (0-5)
metadata.year          # int

# PyCdjStatus
status.is_playing      # bool
status.is_paused       # bool
status.is_cued         # bool
status.is_tempo_master # bool
status.is_synced       # bool
status.is_on_air       # bool
status.effective_tempo # float
status.pitch           # int

# PyCueList
cues.hot_cue_count     # int
cues.memory_point_count # int
cues.loop_count        # int
cues.entries           # list[PyCueEntry]

# PyCueEntry
entry.hot_cue_number   # int
entry.is_loop          # bool
entry.cue_time_ms      # int
entry.loop_time_ms     # int
entry.color_name       # str
entry.comment          # str

# PyBeatGrid
grid.beat_count        # int
grid.beats             # list[PyBeatGridEntry]

# PyBeatGridEntry
beat.beat_number       # int
beat.beat_within_bar   # int
beat.bpm               # int (BPM * 100)
beat.time_ms           # int

# PyWaveformPreview
preview.segment_count  # int
preview.max_height     # int
preview.is_color       # bool
preview.data           # list[int]

# PyAlbumArt
art.width              # int
art.height             # int
art.format             # str ("jpeg" または "png")
art.raw_bytes          # list[int]
```

---

## プロトコルリファレンス

### ネットワークポート

| ポート | プロトコル | 用途 |
|-------|----------|------|
| 50000 | UDP | デバイスアナウンスと検出 |
| 50001 | UDP | ビートブロードキャスト |
| 50002 | UDP | デバイスステータス更新 |
| 12523 | TCP | DBServer メタデータクエリ |

### パケットタイプ

| オフセット | サイズ | 説明 |
|----------|-------|------|
| 0x00 | 10 | マジックヘッダー "Qspt1WmJOL" |
| 0x0a | 1 | パケットタイプ |
| 0x0b | 20 | デバイス名 |
| 0x21 | 1 | デバイス番号 |

### ピッチ値

```cpp
// ピッチエンコーディング:
// 0x000000 = 停止 (0%)
// 0x100000 = 通常速度 (100%)
// 0x200000 = 倍速 (200%)

double percentage = (pitch - 0x100000) * 100.0 / 0x100000;
double multiplier = pitch / static_cast<double>(0x100000);
```

### 参考資料

- [DJ Link Analysis](https://djl-analysis.deepsymmetry.org/) - プロトコルの詳細ドキュメント
- [オリジナル Beat Link (Java)](https://github.com/Deep-Symmetry/beat-link)
- [Crate Digger](https://github.com/Deep-Symmetry/crate-digger) - データベースパース

---

## サンプル

### C++ - フルメタデータ取得

```cpp
#include <beatlink/BeatLink.hpp>
#include <iostream>
#include <thread>

using namespace beatlink;

int main() {
    // すべてのサービスを開始
    DeviceFinder::getInstance().start();
    VirtualCdj::getInstance().start();
    data::MetadataFinder::getInstance().start();
    data::BeatGridFinder::getInstance().start();

    std::this_thread::sleep_for(std::chrono::seconds(5));

    // プレイヤー 1 のメタデータを取得
    auto metadata = data::MetadataFinder::getInstance().getLatestMetadataFor(1);
    if (metadata) {
        std::cout << "タイトル: " << metadata->getTitle() << "\n";
        std::cout << "アーティスト: " << metadata->getArtist() << "\n";
        std::cout << "BPM: " << metadata->getTempo() / 100.0 << "\n";

        auto cueList = metadata->getCueList();
        if (cueList) {
            for (const auto& cue : cueList->getEntries()) {
                std::cout << "キュー @ " << cue.cueTimeMs << "ms\n";
            }
        }
    }

    // クリーンアップ
    data::BeatGridFinder::getInstance().stop();
    data::MetadataFinder::getInstance().stop();
    VirtualCdj::getInstance().stop();
    DeviceFinder::getInstance().stop();
    return 0;
}
```

### Python スクリプト

`examples/` ディレクトリを参照:

| スクリプト | 説明 |
|----------|------|
| `beat_monitor.py` | リアルタイムビート可視化 |
| `track_info.py` | トラックメタデータ、キューポイント、ビートグリッドの表示 |
| `waveform_export.py` | 波形とアルバムアートのファイルエクスポート |

---

## テスト

### C++ ユニットテスト

```bash
cmake .. -DBEATLINK_BUILD_TESTS=ON
make beatlink_tests
./beatlink_tests             # すべてのテストを実行
./beatlink_tests [Beat]      # Beat テストのみ実行
ctest --output-on-failure    # CTest 経由で実行
```

### Python テスト

```bash
cd tests
python golden_test.py           # API と CLI テスト
python communication_test.py    # ネットワークテスト
python real_device_test.py      # デバイス統合テスト
```

### ドキュメント生成

```bash
doxygen Doxyfile
open docs/api/html/index.html
```

---

## トラブルシューティング

### ポートが使用中

```
Failed to start DeviceFinder (port 50000 may be in use)
```

**解決策**: rekordbox などの DJ Link アプリケーションを終了してください。

```bash
# ポートを使用しているプロセスを確認
lsof -i :50000
```

### デバイスが見つからない

1. PC と DJ 機器が同じネットワークサブネット内にあることを確認
2. ファイアウォール設定を確認（UDP 50000-50002、TCP 12523 を許可）
3. 機器で DJ Link が有効になっていることを確認

### macOS ネットワーク権限

macOS では、プロンプトが表示されたらネットワークアクセスを許可してください。

### ビルドエラー

```bash
# Asio が見つからない場合は手動でインストール:
brew install asio          # macOS
apt install libasio-dev    # Ubuntu/Debian
```

---

## 対応機器

### テスト済み

- CDJ-2000NXS2、CDJ-3000
- XDJ-1000MK2、XDJ-XZ、XDJ-RX3
- DJM-900NXS2、DJM-V10
- Opus Quad（部分対応）

### 動作するはず

- すべての DJ Link 対応 Pioneer DJ 機器
- rekordbox（PC/Mac）

---

## ライセンス

本プロジェクトは、オリジナルの Java Beat Link ライブラリと同じ **Eclipse Public License v2.0**（EPL-2.0）の下でライセンスされています。

**オリジナル Java 実装:**
Copyright (c) 2016-2024 [Deep Symmetry, LLC](https://deepsymmetry.org)

**C++20 移植版:**
Copyright (c) 2024-2026 [真鍋大度](https://daito.ws) / [Rhizomatiks](https://rhizomatiks.com)

ライセンス全文は [LICENSE.md](LICENSE.md) をご覧ください。

---

## 謝辞

- **James Elliott** と **Deep Symmetry** - オリジナルの [Beat Link](https://github.com/Deep-Symmetry/beat-link) ライブラリ
- [DJ Link Analysis](https://djl-analysis.deepsymmetry.org/) ドキュメント
- リバースエンジニアリングコミュニティ

---

## バージョン履歴

| バージョン | 日付 | 変更内容 |
|----------|------|---------|
| 0.2.0 | 2026-01 | Python バインディング（105 API）、ユニットテスト、ドキュメント |
| 0.1.0 | 2024-12 | 初回リリース: DeviceFinder、BeatFinder、GUI モニター |
