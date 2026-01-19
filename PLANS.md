# Beat Link C++ - 実装計画 v2.0

## 概要

Beat Link C++ は Deep Symmetry Beat Link (Java) の C++20 移植プロジェクトです。
Pioneer DJ Link プロトコルを実装し、CDJ/XDJ/DJM 機器との通信を可能にします。

---

## 現状分析

### 動作確認済み機能
| 機能 | ファイル | 状態 |
|------|---------|------|
| DeviceFinder (port 50000) | DeviceFinder.cpp (310行) | ✅ 動作 |
| BeatFinder (port 50001) | BeatFinder.cpp (153行) | ✅ 動作 |
| Beat パケットパース | Beat.hpp | ✅ 動作 |
| CdjStatus パケットパース | CdjStatus.hpp | ⚠️ パース可能だが受信機構なし |
| CLI ツール | beatlink_cli.cpp | ✅ 動作 |
| API Schema | ApiSchema.hpp | ✅ 動作 |
| Safety Curtain | SafetyCurtain.hpp | ✅ 動作 |
| Handle Pool | HandlePool.hpp | ✅ 実装済み |

### 未実装・スタブのみ
| 機能 | 状態 | 優先度 |
|------|------|--------|
| VirtualCdj (port 50002) | 7行スタブ | **高** |
| MetadataFinder | 未実装 | 中 |
| TimeFinder | 未実装 | 中 |
| BeatGridFinder | 未実装 | 中 |
| WaveformFinder | 未実装 | 低 |
| ArtFinder | 未実装 | 低 |
| OpusProvider | ヘッダーのみ | 中 |

### 技術的課題
| 課題 | 深刻度 | 対応 |
|------|--------|------|
| 一部バッファ境界チェック不足 | 中 | 残存箇所の修正 |
| 例外がリアルタイムパスに存在 | 高 | std::optional 化 |
| デバイス期限切れ処理不完全 | 中 | expireDevices 実装 |
| Python bindings 不完全 | 低 | 段階的拡充 |

### 完了済み
| 項目 | 状態 |
|------|------|
| C++20 移行 | ✅ `CMAKE_CXX_STANDARD 20` |
| std::span 導入 | ✅ 主要箇所で使用中 |
| std::format 導入 | ✅ sprintf 置換済み |

---

## 実装フェーズ

### Phase 0: 残存課題の修正 ✅ 完了

**目標**: リアルタイム安全性の完成

> **Note**: C++20移行、std::span/std::format導入は完了済み

#### 0.1 例外除去 (Critical) ✅ 完了
- [x] DeviceAnnouncement::create() ファクトリ関数追加
- [x] DeviceUpdate 保護noexceptコンストラクタ追加
- [x] Beat::create() ファクトリ関数追加
- [x] CdjStatus::create() ファクトリ関数追加
- [x] MixerStatus::create() ファクトリ関数追加
- [ ] 旧コンストラクタ呼び出し箇所を新APIに移行（段階的）

```cpp
// 使用例
if (auto beat = Beat::create(data, senderAddress)) {
    processBeat(*beat);
} else {
    // パケットサイズ不正 - 例外なしで処理
}
```

---

### Phase 1: VirtualCdj 実装 ✅ 実装済み

**状態**: VirtualCdj.cpp は1736行で完全実装済み。Port 50002受信も含む。

~~#### 1.1 StatusFinder 新規作成~~ 不要
VirtualCdjがPort 50002のステータス受信を既に実装している。

#### 1.1 VirtualCdj 使用例
```cpp
auto& vcdj = VirtualCdj::getInstance();
vcdj.start();

vcdj.addUpdateListener(listener);  // CdjStatus/MixerStatus受信
```

---

### Phase 1.5: data/ ディレクトリのバグ修正 ✅ 完了

**状態**: ビルド通過確認済み

#### 修正済みバグ
- [x] `AnlzParser.hpp`: WaveformStyle → AnalysisFileFormat に名前変更（重複定義解消）
- [x] `AnlzTypes.hpp`: CueEntry, BeatGridEntry 型を新規作成
- [x] `BeatGrid.hpp`: ANLZ用デフォルトコンストラクタ追加
- [x] `CueList.hpp`: Entry のデフォルトコンストラクタ追加、ANLZ用コンストラクタ追加
- [x] `WaveformPreview.hpp`: ANLZ用コンストラクタ追加
- [x] `WaveformDetail.hpp`: ANLZ用コンストラクタ追加
- [x] `DataReference.hpp`: デフォルトコンストラクタ追加
- [x] `SQLiteConnection.hpp`: クラス名修正、path コンストラクタ追加
- [x] `PdbParser.cpp`: toTrackMetadata() スタブ化（要完全実装）

---

### Phase 2: デバイス管理強化 ✅ 実装済み

#### 2.1 デバイス期限切れ処理 ✅ 完了
- [ ] 既存スタブを実装
- [ ] デバイスとしてネットワーク参加
- [ ] Keep-alive パケット送信
- [ ] テンポマスター追跡

---

### Phase 2: デバイス管理強化

**目標**: 堅牢なデバイス追跡

#### 2.1 デバイス期限切れ処理
- [ ] `DeviceFinder::expireDevices()` 実装
- [ ] タイマースレッドまたは tick() 方式
- [ ] MAXIMUM_AGE (10秒) 経過でデバイス削除
- [ ] DeviceLost コールバック発火

#### 2.2 デバイスプール管理
- [ ] HandlePool を DeviceFinder に統合
- [ ] デバイス参照の安全な取得
```cpp
// Handle ベースのアクセス
auto handle = deviceFinder.getDeviceHandle(deviceNumber);
if (auto* device = deviceFinder.getDevice(handle)) {
    // safe access
}
```

#### 2.3 マスターテンポ追跡
- [ ] 現在のテンポマスター追跡
- [ ] マスター変更イベント通知
- [ ] BPM 履歴保持

---

### Phase 3: OpusProvider 実装 ✅ 完了

**目標**: Opus Quad / XDJ-AZ メタデータ取得

#### 3.1 依存ライブラリ追加 ✅ 完了
- [x] CMakeLists.txt 更新:
  - miniz (ZIPアーカイブ)
  - sqlite3 amalgamation
  - kaitai_runtime
  - utf8proc

#### 3.2 基盤クラス ✅ 完了
- [x] `ZipArchive.hpp/cpp` - miniz ラッパー
- [x] `SqliteConnection.hpp/cpp` - sqlite3 ラッパー
- [x] `AnlzParser.hpp/cpp` - Kaitai ラッパー
- [x] `PdbParser.hpp/cpp` - rekordbox PDBパーサー

#### 3.3 Kaitai パーサー生成 ✅ 完了
- [x] `rekordbox_anlz.h/cpp` - ANLZ解析ファイルパーサー
- [x] `rekordbox_pdb.h/cpp` - PDBデータベースパーサー

#### 3.4 OpusProvider API ✅ 完了
- [x] `attachMetadataArchive()` / `detachMetadataArchive()`
- [x] `getTrackMetadata()` - PdbParser + TrackMetadata::Builder使用
- [x] `getBeatGrid()` - AnlzParser使用
- [x] `getWaveformPreview()` / `getWaveformDetail()` - AnlzParser使用
- [x] `getAlbumArt()` - パス取得まで実装（画像パース未実装）
- [x] PSSI マッチング基盤（SHA-1ハッシュ）

---

### Phase 4: メタデータ取得拡張

#### 4.1 MetadataFinder
- [ ] トラックメタデータ要求/受信
- [ ] キャッシュ機構
- [ ] リスナーパターン

#### 4.2 TimeFinder
- [ ] 現在再生位置追跡
- [ ] 予測補間アルゴリズム

#### 4.3 BeatGridFinder
- [ ] ビートグリッドデータ取得
- [ ] BeatGrid データ構造

---

### Phase 5: Python Bindings 完成

#### 5.1 Core バインディング
- [ ] DeviceFinder 完全バインド
- [ ] BeatFinder 完全バインド
- [ ] StatusFinder バインド
- [ ] Beat, CdjStatus データ構造

#### 5.2 コールバック安全性
- [ ] GIL 管理の徹底
- [ ] 例外ハンドリング (Python→C++→Python)
- [ ] スレッドセーフティ

#### 5.3 テスト充実
- [ ] golden_test.py 拡充
- [ ] 統合テスト追加
- [ ] emulator との結合テスト

---

### Phase 6: 品質向上

#### 6.1 テスト
- [ ] ユニットテスト追加 (Google Test or Catch2)
- [ ] パケットパースのファズテスト
- [ ] 実機テスト手順整備

#### 6.2 ドキュメント
- [ ] Doxygen コメント追加
- [ ] API ドキュメント生成
- [ ] 使用例の充実

#### 6.3 CI/CD
- [ ] GitHub Actions 設定
- [ ] マルチプラットフォームビルド確認
- [ ] 自動テスト実行

---

## ディレクトリ構成 (目標)

```
beat-link-cpp/
├── CMakeLists.txt
├── README.md
├── INSTRUCTION.md
├── PLANS.md
├── include/beatlink/
│   ├── BeatLink.hpp          # メインヘッダー
│   ├── PacketTypes.hpp       # プロトコル定数
│   ├── Util.hpp              # ユーティリティ (std::span版)
│   ├── SafetyCurtain.hpp     # 出力リミッター
│   ├── HandlePool.hpp        # ハンドル管理
│   ├── ApiSchema.hpp         # API イントロスペクション
│   ├── DeviceReference.hpp   # デバイス識別キー
│   ├── DeviceAnnouncement.hpp
│   ├── DeviceUpdate.hpp      # 基底クラス
│   ├── Beat.hpp
│   ├── CdjStatus.hpp
│   ├── MixerStatus.hpp
│   ├── DeviceFinder.hpp
│   ├── BeatFinder.hpp
│   ├── StatusFinder.hpp      # 新規
│   ├── VirtualCdj.hpp        # 拡張
│   └── data/
│       ├── OpusProvider.hpp
│       ├── ZipArchive.hpp    # 新規
│       ├── SqliteConnection.hpp # 新規
│       └── AnlzParser.hpp    # 新規
├── src/
│   ├── Util.cpp
│   ├── DeviceFinder.cpp
│   ├── BeatFinder.cpp
│   ├── StatusFinder.cpp      # 新規
│   ├── VirtualCdj.cpp        # 実装
│   ├── data/
│   │   ├── OpusProvider.cpp
│   │   ├── ZipArchive.cpp
│   │   └── SqliteConnection.cpp
│   ├── generated/            # Kaitai 生成
│   │   ├── rekordbox_anlz.h
│   │   └── rekordbox_anlz.cpp
│   └── python_bindings.cpp
├── examples/
│   ├── simple_beat_listener.cpp
│   ├── beatlink_cli.cpp
│   └── beatlink_gui.cpp
├── tests/
│   ├── golden_test.py
│   ├── communication_test.py
│   ├── real_device_test.py
│   └── emulator.py
└── external/
    ├── beat-link/            # Java リファレンス
    └── crate-digger-cpp/     # .ksy 定義
```

---

## 優先順位まとめ

| 優先度 | Phase | 内容 | 状態 |
|--------|-------|------|------|
| ✅ | 0 | 安全性強化 | 完了 |
| ✅ | 1 | VirtualCdj | 実装済み |
| ✅ | 1.5 | data/バグ修正 | 完了 |
| ✅ | 2 | デバイス管理 | 実装済み |
| ✅ | 3 | OpusProvider | 完了 |
| 🟢 中 | 4 | メタデータ取得拡張 | 次フェーズ |
| 🔵 低 | 5-6 | Python Bindings, 品質向上 | 将来 |

---

## 次のステップ

1. **Phase 4**: MetadataFinder / TimeFinder / BeatGridFinder の実装
2. **AlbumArt**: 画像パース実装（JPEG/PNG対応）
3. **段階的移行**: 旧コンストラクタ呼び出しを新create()関数に移行
4. **警告対処**: codecvt deprecated 警告を C++20 代替実装に置換
5. **テスト**: 実機テスト・ユニットテスト整備

---

## 参考資料

- Java版 Beat Link: `external/beat-link/`
- DJ Link 解析: https://djl-analysis.deepsymmetry.org/
- Kaitai Struct: https://kaitai.io/
- INSTRUCTION.md: C++20 実装パターン
