# Beat Link C++ テストマニュアル

## 実機CDJ/XDJを使用したテスト手順

### 前提条件

- **対応機器**: CDJ-2000NXS, CDJ-2000NXS2, CDJ-3000, XDJ-RX, XDJ-RX2, XDJ-XZ など
- **ネットワーク**: Mac と CDJ が同じネットワーク（LAN）に接続されていること
- **ソフトウェア**: Rekordbox を**終了**していること（ポート競合を避けるため）

### ネットワーク構成

```
┌─────────────┐      ┌─────────────┐      ┌─────────────┐
│   CDJ-1     │      │   CDJ-2     │      │    Mac      │
│  (Player 1) │      │  (Player 2) │      │ Beat Link   │
└──────┬──────┘      └──────┬──────┘      └──────┬──────┘
       │                    │                    │
       └────────────────────┴────────────────────┘
                    LAN / Switch / Router
```

---

## テスト手順

### Step 1: Rekordbox を終了

DJ Link ポート（50000, 50001, 50002）を解放するため、Rekordbox を完全に終了してください。

確認コマンド:
```bash
lsof -i :50000
```
何も表示されなければOKです。

### Step 2: CDJ の電源を入れる

1. CDJ/XDJ の電源を入れる
2. LAN ケーブルで同じネットワークに接続
3. CDJ の Link 設定が有効になっていることを確認

### Step 3: Beat Link CLI でデバイス検出

```bash
cd "/Users/rhizomacbookproretina30/Rhizomatiks Dropbox/daito manabe/dm_web/sub-domain/daito-lab/beat-link-cpp/implementation/beat-link-cpp/cpp/build"

# JSON形式で出力
./beatlink_cli --listen --json
```

期待される出力:
```json
{"event":"device_found","device_number":1,"device_name":"CDJ-3000","address":"192.168.1.x"}
{"event":"beat","device_number":1,"bpm":128.00,"beat_in_bar":1}
{"event":"beat","device_number":1,"bpm":128.00,"beat_in_bar":2}
...
```

### Step 4: Python でテスト

```bash
cd "/Users/rhizomacbookproretina30/Rhizomatiks Dropbox/daito manabe/dm_web/sub-domain/daito-lab/beat-link-cpp/implementation/beat-link-cpp/cpp/tests"

python3 real_device_test.py
```

---

## テストスクリプト

### real_device_test.py

以下のスクリプトを使用して実機テストを行います:

```python
#!/usr/bin/env python3
"""
Real CDJ/XDJ Device Test

Tests Beat Link C++ library with actual Pioneer DJ equipment.
Requires CDJ/XDJ connected to the same network.
"""

import sys
import time
from pathlib import Path

# Add build directory to path
build_dir = Path(__file__).parent.parent / "build"
sys.path.insert(0, str(build_dir))

import beatlink_py as bl

def main():
    print("=" * 60)
    print("Beat Link C++ - Real Device Test")
    print("=" * 60)
    print()
    print("Prerequisites:")
    print("  - Rekordbox is CLOSED")
    print("  - CDJ/XDJ is powered on and connected to network")
    print("  - A track is loaded and playing (for beat detection)")
    print()

    devices_found = []
    beats_received = []

    def on_device_found(device):
        devices_found.append(device)
        print(f"[DEVICE FOUND] {device.device_name} #{device.device_number} @ {device.address}")

    def on_device_lost(device):
        print(f"[DEVICE LOST] {device.device_name} #{device.device_number}")

    def on_beat(beat):
        beats_received.append(beat)
        print(f"[BEAT] {beat.effective_bpm:6.2f} BPM | Beat {beat.beat_in_bar}/4 | Device #{beat.device_number}")

    # Register listeners
    bl.add_device_found_listener(on_device_found)
    bl.add_device_lost_listener(on_device_lost)
    bl.add_beat_listener(on_beat)

    # Start finders
    print("Starting device finder...")
    if not bl.start_device_finder():
        print("ERROR: Failed to start device finder!")
        print("       Is Rekordbox still running?")
        return 1
    print("Device finder started on port 50000")

    print("Starting beat finder...")
    if not bl.start_beat_finder():
        print("ERROR: Failed to start beat finder!")
        bl.stop_device_finder()
        return 1
    print("Beat finder started on port 50001")

    print()
    print("-" * 60)
    print("Listening for 30 seconds... (Press Ctrl+C to stop early)")
    print("-" * 60)
    print()

    try:
        time.sleep(30)
    except KeyboardInterrupt:
        print("\nStopped by user")

    # Stop finders
    bl.stop_beat_finder()
    bl.stop_device_finder()

    # Summary
    print()
    print("=" * 60)
    print("Test Results")
    print("=" * 60)
    print(f"Devices found: {len(devices_found)}")
    for d in devices_found:
        print(f"  - {d.device_name} #{d.device_number} @ {d.address}")

    print(f"Beats received: {len(beats_received)}")
    if beats_received:
        bpms = [b.effective_bpm for b in beats_received]
        print(f"  - Average BPM: {sum(bpms)/len(bpms):.2f}")
        print(f"  - Min BPM: {min(bpms):.2f}")
        print(f"  - Max BPM: {max(bpms):.2f}")

    print()
    if devices_found and beats_received:
        print("SUCCESS: Real device communication verified!")
        return 0
    elif devices_found:
        print("PARTIAL: Devices found but no beats received.")
        print("         Is a track playing on the CDJ?")
        return 1
    else:
        print("FAILED: No devices found.")
        print("        Check network connection and CDJ power.")
        return 1

if __name__ == "__main__":
    sys.exit(main())
```

---

## GUI でテスト

Beat Link GUI を使用してビジュアルに確認:

```bash
cd "/Users/rhizomacbookproretina30/Rhizomatiks Dropbox/daito manabe/dm_web/sub-domain/daito-lab/beat-link-cpp/implementation/beat-link-cpp/cpp/build"

./beatlink_gui
```

GUI の操作:
1. **Start** ボタンをクリック
2. 検出されたデバイスが表示される
3. CDJ で曲を再生すると BPM とビート位置が更新される

---

## トラブルシューティング

### 問題: "Failed to start device finder"

**原因**: ポートが使用中

**解決策**:
```bash
# 何がポートを使っているか確認
lsof -i :50000

# Rekordbox が動いていれば終了する
```

### 問題: デバイスが検出されない

**確認事項**:
1. CDJ と Mac が同じネットワークにあるか
2. CDJ の Link 機能が有効か
3. ファイアウォールが UDP 50000-50002 をブロックしていないか

**デバッグ**:
```bash
# ネットワーク上のブロードキャストをキャプチャ
sudo tcpdump -i any udp port 50000 -X
```

### 問題: ビートが受信されない

**確認事項**:
1. CDJ で曲が再生されているか
2. CDJ が Play 状態か（Cue で止まっていないか）

---

## 自動テストの実行

### エミュレーターテスト（Rekordbox不要）

```bash
cd "/Users/rhizomacbookproretina30/Rhizomatiks Dropbox/daito manabe/dm_web/sub-domain/daito-lab/beat-link-cpp/implementation/beat-link-cpp/cpp/tests"

# 全テスト実行
python3 golden_test.py && python3 communication_test.py
```

### 期待される結果

```
Golden Tests:      95 passed, 0 failed
Communication Tests: 27 passed, 0 failed
```

---

## プロトコル詳細

### 使用ポート

| ポート | プロトコル | 用途 |
|--------|-----------|------|
| 50000  | UDP       | デバイス検出・アナウンス |
| 50001  | UDP       | ビートパケット |
| 50002  | UDP       | ステータス更新 |

### パケット形式

**Device Announcement (54 bytes)**
```
Offset  Size  Description
0x00    10    Magic header: 51 73 70 74 31 57 6d 4a 4f 4c
0x0a    1     Packet type: 0x06 (keep-alive) or 0x0a (hello)
0x0c    20    Device name (ASCII, null-padded)
0x24    1     Device number (1-4)
0x26    4     IP address
```

**Beat Packet (96 bytes)**
```
Offset  Size  Description
0x00    10    Magic header
0x0a    1     Packet type: 0x28
0x21    1     Device number
0x55    3     Pitch value (24-bit, 1048576 = neutral)
0x5a    2     BPM * 100 (e.g., 12800 = 128.00 BPM)
0x5c    1     Beat within bar (1-4)
```

---

## 連絡先

問題が発生した場合は、以下の情報を添えて報告してください:
- macOS バージョン
- CDJ/XDJ モデル
- エラーメッセージ
- `lsof -i :50000` の出力
