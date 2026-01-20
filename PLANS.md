# Beat Link C++ - 実装計画 v3.0

## 概要

Beat Link C++ は Deep Symmetry Beat Link (Java) の C++20 移植プロジェクトです。
Pioneer DJ Link プロトコルを実装し、CDJ/XDJ/DJM 機器との通信を可能にします。

---

## 移植状況サマリー

### Java版クラス数: 85ファイル
### C++移植状況: **約90%完了**

| カテゴリ | Java | C++ | 状態 |
|---------|------|-----|------|
| Core (beatlink/) | 30 | 30 | ✅ 完了 |
| Data (data/) | 40 | 38 | ✅ 実装済み (UIスキップ) |
| DBServer (dbserver/) | 7 | 7 | ✅ 完了 |
| Python Bindings | - | 1 | ✅ 完了 (105 API) |
| Tests | 多数 | 少数 | ❌ 要拡充 |

---

## 実装完了コンポーネント

### Core (beatlink/)

| コンポーネント | ファイル | 行数 | 状態 |
|---------------|---------|------|------|
| DeviceFinder | DeviceFinder.cpp | 399 | ✅ |
| BeatFinder | BeatFinder.cpp | 459 | ✅ |
| VirtualCdj | VirtualCdj.cpp | 1736 | ✅ |
| VirtualRekordbox | VirtualRekordbox.cpp | 712 | ✅ |
| Beat | Beat.cpp | 35 | ✅ |
| CdjStatus | CdjStatus.cpp | 6 (ヘッダ主体) | ✅ |
| MixerStatus | MixerStatus.cpp | 6 (ヘッダ主体) | ✅ |
| DeviceAnnouncement | DeviceAnnouncement.cpp | 6 (ヘッダ主体) | ✅ |
| DeviceUpdate | DeviceUpdate.cpp | 6 (ヘッダ主体) | ✅ |
| MediaDetails | MediaDetails.cpp | 238 | ✅ |
| Metronome | Metronome.cpp | 97 | ✅ |
| PrecisePosition | PrecisePosition.cpp | 59 | ✅ |
| Util | Util.cpp | 6 (ヘッダ主体) | ✅ |

### Data Finders (data/)

| コンポーネント | ファイル | 行数 | 状態 |
|---------------|---------|------|------|
| MetadataFinder | MetadataFinder.cpp | 937 | ✅ |
| TimeFinder | TimeFinder.cpp | 607 | ✅ |
| WaveformFinder | WaveformFinder.cpp | 813 | ✅ |
| BeatGridFinder | BeatGridFinder.cpp | 394 | ✅ |
| ArtFinder | ArtFinder.cpp | 489 | ✅ |
| AnalysisTagFinder | AnalysisTagFinder.cpp | 541 | ✅ |
| SignatureFinder | SignatureFinder.cpp | 496 | ✅ |
| OpusProvider | OpusProvider.cpp | 482 | ✅ |
| CrateDigger | CrateDigger.cpp | 367 | ✅ |
| MenuLoader | MenuLoader.cpp | 692 | ✅ |

### Data Types (data/)

| コンポーネント | 状態 | 備考 |
|---------------|------|------|
| TrackMetadata | ✅ | Builder パターン実装済み |
| WaveformPreview | ✅ | ANLZ コンストラクタ追加済み |
| WaveformDetail | ✅ | ANLZ コンストラクタ追加済み |
| BeatGrid | ✅ | ANLZ対応済み |
| CueList | ✅ | Entry デフォルトコンストラクタ追加済み |
| AlbumArt | ✅ | stb_image で JPEG/PNG デコード実装済み |
| DataReference | ✅ | デフォルトコンストラクタ追加済み |
| SlotReference | ✅ | |
| DeckReference | ✅ | |
| SearchableItem | ✅ | |
| ColorItem | ✅ | |

### DBServer (dbserver/)

| コンポーネント | ファイル | 行数 | 状態 |
|---------------|---------|------|------|
| Client | Client.cpp | 280 | ✅ |
| ConnectionManager | ConnectionManager.cpp | 322 | ✅ |
| Message | Message.cpp | 465 | ✅ |
| Field | Field.cpp | 48 | ✅ |
| NumberField | NumberField.cpp | 74 | ✅ |
| StringField | StringField.cpp | 77 | ✅ |
| BinaryField | BinaryField.cpp | 47 | ✅ |
| DataReader | DataReader.cpp | 79 | ✅ |

### Parsers (generated/ & data/)

| コンポーネント | ファイル | 行数 | 状態 |
|---------------|---------|------|------|
| rekordbox_anlz | rekordbox_anlz.cpp | 557 | ✅ Kaitai生成 |
| rekordbox_pdb | rekordbox_pdb.cpp | 462 | ✅ Kaitai生成 |
| AnlzParser | AnlzParser.cpp | 353 | ✅ |
| PdbParser | PdbParser.cpp | 368 | ✅ |
| ZipArchive | ZipArchive.cpp | 215 | ✅ |
| SqliteConnection | SqliteConnection.cpp | 431 | ✅ |

### Listeners (include/beatlink/)

すべてのリスナーインターフェースがヘッダーファイルとして実装済み:

- BeatListener, DeviceAnnouncementListener, DeviceUpdateListener
- MasterListener, SyncListener, FaderStartListener, OnAirListener
- MasterHandoffListener, MediaDetailsListener, LifecycleListener
- PrecisePositionListener
- TrackMetadataListener, BeatGridListener, WaveformListener
- AlbumArtListener, AnalysisTagListener, SignatureListener
- TrackPositionListener, TrackPositionBeatListener
- MountListener, DatabaseListener, SQLiteConnectionListener

---

## 移植スキップ (Java UI コンポーネント)

以下はJava Swing固有のUIコンポーネントのため、C++への移植対象外:

| コンポーネント | 理由 |
|---------------|------|
| WaveformDetailComponent | Java Swing UI |
| WaveformPreviewComponent | Java Swing UI |
| OverlayPainter | Java Swing UI |
| RepaintDelegate | Java Swing UI |

※ C++側では beatlink_gui.cpp で ImGui ベースのGUIを独自実装済み

---

## 残作業フェーズ

### Phase 4: 品質向上 ✅ 完了

#### 4.1 アルバムアート画像パース
- [x] JPEG パーサー実装 (stb_image 使用)
- [x] PNG パーサー実装
- [x] AlbumArt::decode() / getDecodedPixels() / getDimensions() 実装

#### 4.2 C++20 警告対処
- [x] `codecvt` deprecated 警告の解消
  - 手動UTF-16 BE/LE → UTF-8変換実装 (StringField.cpp, CueList.cpp)
- [x] その他の deprecated 警告対処

#### 4.3 旧コンストラクタ移行
- [x] Beat コンストラクタ → `Beat::create()` 使用箇所移行
- [x] CdjStatus コンストラクタ → `CdjStatus::create()` 使用箇所移行
- [x] MixerStatus コンストラクタ → `MixerStatus::create()` 使用箇所移行
- [x] DeviceAnnouncement コンストラクタ → `DeviceAnnouncement::create()` 使用箇所移行

---

### Phase 5: Python Bindings 完成 ✅ 完了

#### 5.1 実装済み機能 (1300行以上)
```python
# 使用可能な機能
import beatlink_py as bl
bl.start_device_finder()
bl.start_beat_finder()
bl.start_virtual_cdj()
bl.start_metadata_finder()
bl.add_beat_listener(callback)
bl.get_track_metadata(player)
bl.get_waveform_preview(player)
bl.get_album_art(player)
bl.get_beat_grid(player)
bl.get_cue_list(player)
# 合計105個のAPI
```

#### 5.2 実装完了
- [x] clear_*_listeners() メソッド群 (リスナー削除)
- [x] VirtualCdj 制御メソッド (set_tempo, become_master, etc.)
- [x] WaveformPreview/WaveformDetail データ取得
- [x] AlbumArt 取得
- [x] CueList 取得
- [x] BeatGrid 取得
- [x] OpusProvider バインディング

#### 5.3 追加バインディング完了
```python
# 実装済み
bl.get_waveform_preview(player)  # → WaveformPreview
bl.get_waveform_detail(player)   # → WaveformDetail
bl.get_album_art(player)         # → AlbumArt (raw bytes)
bl.get_cue_list(player)          # → CueList
bl.get_beat_grid(player)         # → BeatGrid
bl.clear_beat_listeners()        # リスナー削除
```

#### 5.4 ドキュメント
- [ ] Python API ドキュメント (Sphinx)
- [x] 使用例スクリプト追加 (examples/)
  - beat_monitor.py: ビートモニター
  - track_info.py: トラック情報表示
  - waveform_export.py: 波形/アート出力
- [ ] README.md 更新

---

### Phase 6: テスト整備

#### 6.1 ユニットテスト
- [ ] Google Test または Catch2 導入
- [ ] パケットパーステスト
  - Beat パケット
  - CdjStatus パケット
  - MixerStatus パケット
  - DeviceAnnouncement パケット
- [ ] データ構造テスト
  - BeatGrid
  - CueList
  - WaveformPreview/Detail
  - TrackMetadata

#### 6.2 ファズテスト
- [ ] libFuzzer または AFL++ 導入
- [ ] パケットパーサーのファズテスト
- [ ] ANLZ/PDB パーサーのファズテスト

#### 6.3 統合テスト
- [ ] Python golden_test.py 拡充
- [ ] emulator.py 連携テスト
- [ ] 実機テスト手順書作成

#### 6.4 テストデータ
- [ ] テスト用パケットキャプチャ収集
- [ ] 各種rekordboxバージョンのANLZファイル
- [ ] 各種PDBファイルサンプル

---

### Phase 7: CI/CD 構築

#### 7.1 GitHub Actions
```yaml
# .github/workflows/build.yml
- macOS (arm64, x86_64)
- Linux (Ubuntu 22.04+)
- Windows (MSVC 2022)
```

#### 7.2 自動テスト
- [ ] ビルド検証
- [ ] ユニットテスト実行
- [ ] Python バインディングテスト
- [ ] コード品質チェック (clang-tidy)

#### 7.3 リリース自動化
- [ ] タグプッシュでリリース作成
- [ ] プラットフォーム別バイナリ
- [ ] Python wheel 生成

---

### Phase 8: ドキュメント整備

#### 8.1 API ドキュメント
- [ ] Doxygen コメント追加
- [ ] API リファレンス生成
- [ ] アーキテクチャ図

#### 8.2 ユーザーガイド
- [ ] クイックスタートガイド
- [ ] 設定オプション説明
- [ ] トラブルシューティング

#### 8.3 開発者ガイド
- [ ] 貢献ガイドライン
- [ ] コーディング規約
- [ ] ビルド手順

---

## ディレクトリ構成 (現状)

```
beat-link-cpp/
├── CMakeLists.txt
├── README.md
├── INSTRUCTION.md
├── PLANS.md
├── include/beatlink/
│   ├── BeatLink.hpp          # メインヘッダー (全インクルード)
│   ├── PacketTypes.hpp       # プロトコル定数
│   ├── Util.hpp              # ユーティリティ
│   ├── SafetyCurtain.hpp     # 出力リミッター
│   ├── HandlePool.hpp        # ハンドル管理
│   ├── ApiSchema.hpp         # API イントロスペクション
│   │
│   ├── DeviceReference.hpp
│   ├── DeviceAnnouncement.hpp
│   ├── DeviceUpdate.hpp
│   ├── Beat.hpp
│   ├── CdjStatus.hpp
│   ├── MixerStatus.hpp
│   ├── MediaDetails.hpp
│   ├── PlayerSettings.hpp
│   ├── Metronome.hpp
│   ├── PrecisePosition.hpp
│   ├── Snapshot.hpp
│   │
│   ├── DeviceFinder.hpp
│   ├── BeatFinder.hpp
│   ├── VirtualCdj.hpp
│   ├── VirtualRekordbox.hpp
│   │
│   ├── *Listener.hpp         # 各種リスナーインターフェース
│   │
│   ├── data/
│   │   ├── DataReference.hpp
│   │   ├── SlotReference.hpp
│   │   ├── DeckReference.hpp
│   │   ├── TrackMetadata.hpp
│   │   ├── BeatGrid.hpp
│   │   ├── CueList.hpp
│   │   ├── WaveformPreview.hpp
│   │   ├── WaveformDetail.hpp
│   │   ├── AlbumArt.hpp
│   │   ├── AnalysisTag.hpp
│   │   ├── SearchableItem.hpp
│   │   ├── ColorItem.hpp
│   │   ├── PlaybackState.hpp
│   │   │
│   │   ├── MetadataFinder.hpp
│   │   ├── TimeFinder.hpp
│   │   ├── WaveformFinder.hpp
│   │   ├── BeatGridFinder.hpp
│   │   ├── ArtFinder.hpp
│   │   ├── AnalysisTagFinder.hpp
│   │   ├── SignatureFinder.hpp
│   │   │
│   │   ├── OpusProvider.hpp
│   │   ├── CrateDigger.hpp
│   │   ├── MenuLoader.hpp
│   │   ├── Database.hpp
│   │   │
│   │   ├── MetadataProvider.hpp
│   │   ├── AnlzParser.hpp
│   │   ├── AnlzTypes.hpp
│   │   ├── PdbParser.hpp
│   │   ├── ZipArchive.hpp
│   │   ├── SqliteConnection.hpp
│   │   │
│   │   └── *Listener.hpp, *Update.hpp
│   │
│   └── dbserver/
│       ├── Client.hpp
│       ├── ConnectionManager.hpp
│       ├── Message.hpp
│       ├── Field.hpp
│       ├── NumberField.hpp
│       ├── StringField.hpp
│       ├── BinaryField.hpp
│       └── DataReader.hpp
│
├── src/
│   ├── *.cpp                 # Core 実装
│   ├── data/*.cpp            # Data 実装
│   ├── dbserver/*.cpp        # DBServer 実装
│   ├── generated/            # Kaitai 生成コード
│   │   ├── rekordbox_anlz.h/cpp
│   │   └── rekordbox_pdb.h/cpp
│   └── python_bindings.cpp
│
├── examples/
│   ├── simple_beat_listener.cpp
│   ├── beatlink_cli.cpp
│   └── beatlink_gui.cpp
│
├── tests/
│   ├── golden_test.py
│   ├── communication_test.py
│   ├── real_device_test.py
│   └── emulator.py
│
└── external/
    ├── beat-link/            # Java リファレンス
    └── crate-digger-cpp/     # .ksy 定義
```

---

## 優先順位まとめ

| 優先度 | Phase | 内容 | 状態 |
|--------|-------|------|------|
| ✅ | 0 | 安全性強化 (例外除去) | 完了 |
| ✅ | 1 | VirtualCdj | 完了 |
| ✅ | 1.5 | data/バグ修正 | 完了 |
| ✅ | 2 | デバイス管理強化 | 完了 |
| ✅ | 3 | OpusProvider | 完了 |
| ✅ | 4 | 品質向上 | 完了 |
| ✅ | 5 | Python Bindings 完成 | 完了 (105 API) |
| 🟢 | 6 | テスト整備 | 次フェーズ |
| 🔵 | 7 | CI/CD | 将来 |
| 🔵 | 8 | ドキュメント | 将来 |

---

## 技術仕様

### 依存ライブラリ

| ライブラリ | 用途 | バージョン |
|-----------|------|----------|
| asio | ネットワーク | standalone (non-Boost) |
| nanobind | Python バインディング | 最新 |
| miniz | ZIP 展開 | master |
| sqlite3 | DB アクセス | 3.45.0 |
| kaitai_struct | バイナリパース | 最新 |
| utf8proc | UTF-8 処理 | 最新 |
| imgui + glfw | GUI (オプション) | 最新 |

### ビルド要件

- CMake 3.15+
- C++20 対応コンパイラ
  - GCC 11+
  - Clang 14+
  - MSVC 2022+
- Python 3.8+ (バインディングビルド時)

### プロトコルポート

| ポート | 用途 |
|--------|------|
| 50000 | Device Announcement (UDP) |
| 50001 | Beat Broadcast (UDP) |
| 50002 | Device Status (UDP) |
| 12523 | DBServer Query (TCP) |

---

## 参考資料

- Java版 Beat Link: `external/beat-link/`
- DJ Link 解析: https://djl-analysis.deepsymmetry.org/
- Kaitai Struct: https://kaitai.io/
- INSTRUCTION.md: C++20 実装パターン

---

## 更新履歴

| 日付 | バージョン | 内容 |
|------|-----------|------|
| 2026-01-18 | v1.0 | 初版作成 |
| 2026-01-18 | v2.0 | Phase 3 完了、OpusProvider 実装 |
| 2026-01-19 | v3.0 | 全体移植状況の棚卸し、残作業整理 |
| 2026-01-20 | v4.0 | Phase 4-5 完了、Python Bindings 105 API実装、使用例スクリプト追加 |
