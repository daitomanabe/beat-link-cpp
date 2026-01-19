# Beat-Link C++ マニュアル

Pioneer DJ Link プロトコルライブラリの C++ 実装です。
CDJ、XDJ、DJM などの Pioneer DJ 機器とネットワーク経由で通信し、BPM、ビート、再生状態などの情報を取得できます。

## 目次

1. [必要環境](#必要環境)
2. [ビルド方法](#ビルド方法)
3. [アプリケーション](#アプリケーション)
4. [ライブラリの使い方](#ライブラリの使い方)
5. [API リファレンス](#api-リファレンス)
6. [フロー図](#フロー図)
7. [トラブルシューティング](#トラブルシューティング)

---

## 必要環境

- **OS**: macOS, Linux, Windows
- **コンパイラ**: C++17 対応 (GCC 7+, Clang 5+, MSVC 2017+)
- **CMake**: 3.14 以上
- **ネットワーク**: DJ Link 機器と同じネットワークに接続

### 自動取得される依存ライブラリ

以下のライブラリは CMake が自動的にダウンロードします：

- Asio (Standalone, Non-Boost)
- GLFW (GUI用)
- Dear ImGui (GUI用)

---

## ビルド方法

### 基本的なビルド手順

```bash
# プロジェクトディレクトリに移動
cd cpp

# ビルドディレクトリを作成
mkdir build
cd build

# CMake 設定（依存ライブラリの自動ダウンロード含む）
cmake ..

# ビルド実行
make -j4
```

### ビルド成果物

ビルドが完了すると、以下のファイルが生成されます：

| ファイル | 説明 |
|----------|------|
| `libbeatlink.a` | 静的ライブラリ |
| `beatlink_example` | コマンドライン版サンプル |
| `beatlink_gui` | GUI版モニター（Dear ImGui） |

---

## アプリケーション

### GUI版モニター (beatlink_gui)

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

## トラブルシューティング

### ポートが使用中

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
