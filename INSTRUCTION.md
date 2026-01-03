# C++20 Real-time Protocol Library Implementation Guide

このドキュメントは、C++20 でリアルタイムネットワークプロトコルライブラリを実装する際のパターンとテンプレートを説明します。

**Based on**: INTRODUCTION_JAVA_TO_CPP.md (Rhizomatiks/Manabe Workflow)

---

## Task Levels

- **MUST**: 必須。欠けると再利用可能な基盤にならない
- **RECOMMENDED**: 推奨。品質・安定性が大きく向上
- **OPTIONAL**: 状況次第

---

## 目次

1. [設計思想](#設計思想)
2. [プロジェクト構成](#プロジェクト構成)
3. [C++20 Features](#c20-features)
4. [デザインパターン](#デザインパターン)
5. [AI Agent Integration](#ai-agent-integration)
6. [Safety Curtain](#safety-curtain)
7. [Python Bindings](#python-bindings)
8. [ネットワーク実装](#ネットワーク実装)
9. [ビルドシステム](#ビルドシステム)
10. [チェックリスト](#チェックリスト)

---

## 設計思想

### 1.1 Goal: Porting is not Translation (MUST)

目的は、複数作品・複数案件で使い回せる **Modern C++20 資産** を作ること。

- 元コードの構造ではなく「契約（Input/Output/Invariant）」を移植する
- **C++20 Concepts** でテンプレートの要件をコードで明示（AIの誤実装を防ぐ）
- Core ライブラリは **Headless**（GUI/Window システムに依存しない）

### 1.2 Determinism First (MUST)

再現性を保証するための設計:

- **Time Injection**: 時間は引数 `ticks` (int64) として渡す。内部で `std::chrono` を呼ばない
- **Random Injection**: 乱数シードを外部から注入可能に
- **No Magic Numbers**: 定数は明示的に定義

### 1.3 Universal Adapter Design (MUST)

Core と I/O を分離:

```
┌─────────────────────────────────────────┐
│  Adapters (I/O Layer)                   │
│  ┌─────────┐ ┌─────────┐ ┌───────────┐ │
│  │ Python  │ │   CLI   │ │ OSC/LSL   │ │
│  │nanobind │ │  JSONL  │ │TouchDesign│ │
│  └────┬────┘ └────┬────┘ └─────┬─────┘ │
│       └───────────┼────────────┘       │
│                   ▼                     │
│  ┌─────────────────────────────────┐   │
│  │         Core (Pure C++20)        │   │
│  │   No Framework Headers           │   │
│  │   Standard Library Only          │   │
│  └─────────────────────────────────┘   │
└─────────────────────────────────────────┘
```

---

## プロジェクト構成

### 推奨ディレクトリ構造

```
project/
├── CMakeLists.txt
├── README.md
├── INSTRUCTION.md
├── include/
│   └── projectname/
│       ├── ProjectName.hpp      # メインヘッダー
│       ├── Util.hpp             # バイト操作、std::span
│       ├── PacketTypes.hpp      # 定数、Version
│       ├── HandlePool.hpp       # Handle Pattern
│       ├── SafetyCurtain.hpp    # 出力リミッター
│       ├── ApiSchema.hpp        # describe_api()
│       └── ...
├── src/
│   ├── Core.cpp
│   ├── python_bindings.cpp      # nanobind
│   └── ...
├── examples/
│   ├── simple_example.cpp
│   ├── cli_tool.cpp             # --schema サポート
│   └── gui_example.cpp
└── tests/
    └── golden_tests.py          # Python経由のテスト
```

---

## C++20 Features

### std::span (MUST)

ポインタ+サイズ渡しを全廃。配列アクセスは必ず `std::span` を使う。

```cpp
#include <span>
#include <optional>

// BAD: 生ポインタ
int64_t bytesToNumber(const uint8_t* buffer, size_t start, size_t length);

// GOOD: std::span with bounds checking
[[nodiscard]] std::optional<int64_t> bytesToNumber(
    std::span<const uint8_t> buffer,
    size_t start,
    size_t length
) noexcept {
    if (start + length > buffer.size()) {
        return std::nullopt;  // エラーは例外ではなく optional で返す
    }
    int64_t result = 0;
    for (size_t i = start; i < start + length; ++i) {
        result = (result << 8) + buffer[i];
    }
    return result;
}
```

### Concepts (MUST)

テンプレートの要件をコードで明示:

```cpp
#include <concepts>

// Concept: バイトバッファとして使える型
template<typename T>
concept ByteBuffer = requires(T t) {
    { t.data() } -> std::convertible_to<const uint8_t*>;
    { t.size() } -> std::convertible_to<size_t>;
};

// Concept: ビート受信可能な型
template<typename T>
concept BeatReceiver = requires(T t, double bpm, int beatInBar) {
    { t.onBeat(bpm, beatInBar) } -> std::same_as<void>;
};

// Concept: JSON シリアライズ可能
template<typename T>
concept JsonSerializable = requires(T t) {
    { t.toJson() } -> std::convertible_to<std::string>;
};

// Concept を使った関数
template<ByteBuffer Buffer>
bool validatePacket(const Buffer& buffer) {
    return buffer.size() >= MIN_PACKET_SIZE;
}
```

### std::format (MUST)

`sprintf` 禁止。文字列構築は `std::format` を使用:

```cpp
#include <format>

// BAD
char buf[256];
sprintf(buf, "Device %d: %s", num, name.c_str());

// GOOD
std::string msg = std::format("Device {}: {}", num, name);

// JSONL ログ出力
std::string jsonLog = std::format(
    R"({{"event":"beat","device":{},"bpm":{:.2f}}})",
    deviceNumber, bpm
);
```

### std::source_location (RECOMMENDED)

ログ出力時のファイル/行情報:

```cpp
#include <source_location>

void logError(
    const std::string& message,
    const std::source_location& loc = std::source_location::current()
) {
    std::cerr << std::format("{}:{}: {}", loc.file_name(), loc.line(), message);
}
```

### [[nodiscard]] と noexcept (MUST)

```cpp
// 戻り値を無視すると警告
[[nodiscard]] bool start();

// 例外を投げないことを保証
[[nodiscard]] static constexpr double pitchToPercentage(int64_t pitch) noexcept;
```

---

## デザインパターン

### Handle Pattern (MUST)

オブジェクトはポインタではなく「整数ID」で管理:

```cpp
// Handle 型定義
using Handle = uint32_t;
constexpr Handle INVALID_HANDLE = 0;

// 世代カウンタ付きハンドル（use-after-free 検出）
struct GenerationalHandle {
    Handle id = INVALID_HANDLE;
    uint32_t generation = 0;

    [[nodiscard]] bool isValid() const noexcept {
        return id != INVALID_HANDLE;
    }
};

// オブジェクトプール
template<typename T>
class HandlePool {
public:
    [[nodiscard]] GenerationalHandle allocate(T value);
    [[nodiscard]] T* get(GenerationalHandle handle) noexcept;
    bool release(GenerationalHandle handle) noexcept;

    template<typename Func>
    void forEach(Func&& func);  // 全オブジェクトに対して実行

private:
    std::vector<PoolEntry<T>> entries_;
    std::mutex mutex_;
};

// 使用例
HandlePool<Device> devicePool;
auto handle = devicePool.allocate(Device{...});

// JSON/OSC でそのまま送れる
sendOSC("/device/id", handle.id);

// アクセス
if (auto* dev = devicePool.get(handle)) {
    // use dev
}
```

**理由**:
- ID は JSON/OSC でそのまま扱える
- シリアライズ容易
- Python バインディングでも安全

### No Exceptions in Process Loop (MUST)

リアルタイム処理ループ内での例外送出禁止:

```cpp
// BAD
void process() {
    if (error) throw std::runtime_error("Error!");
}

// GOOD
std::optional<Result> process() noexcept {
    if (error) return std::nullopt;
    return Result{...};
}

// または error code
enum class ProcessError { None, InvalidData, Timeout };
std::pair<Result, ProcessError> process() noexcept;
```

### No Allocation in Process Loop (MUST)

メインループ内での `new`, `malloc` 禁止:

```cpp
// BAD
void tick() {
    auto* data = new uint8_t[1024];  // NG!
}

// GOOD: 事前確保
class Processor {
    std::array<uint8_t, 1024> buffer_;  // 事前確保
public:
    void tick() {
        // buffer_ を再利用
    }
};
```

### Singleton Pattern

```cpp
class DeviceFinder {
public:
    static DeviceFinder& getInstance() {
        static DeviceFinder instance;
        return instance;
    }

    DeviceFinder(const DeviceFinder&) = delete;
    DeviceFinder& operator=(const DeviceFinder&) = delete;

    [[nodiscard]] bool start();
    void stop();
    [[nodiscard]] bool isRunning() const noexcept { return running_.load(); }

private:
    DeviceFinder() = default;
    ~DeviceFinder() { stop(); }

    std::atomic<bool> running_{false};
};
```

### Callback/Listener Pattern

```cpp
using BeatCallback = std::function<void(const Beat&)>;

class BeatFinder {
public:
    void addBeatListener(BeatCallback callback) {
        std::lock_guard<std::mutex> lock(listenersMutex_);
        listeners_.push_back(std::move(callback));
    }

private:
    void notifyListeners(const Beat& beat) {
        std::vector<BeatCallback> listeners;
        {
            std::lock_guard<std::mutex> lock(listenersMutex_);
            listeners = listeners_;  // コピーしてロック解放
        }
        for (const auto& listener : listeners) {
            listener(beat);  // 例外は呼び出し側で処理
        }
    }

    std::mutex listenersMutex_;
    std::vector<BeatCallback> listeners_;
};
```

---

## AI Agent Integration

### describe_api() (MUST)

AI エージェントがバイナリの仕様を理解できるよう、自己記述 API を実装:

```cpp
struct ApiSchema {
    std::string name;
    std::string version;
    std::string description;
    std::vector<CommandInfo> commands;
    std::vector<IoInfo> inputs;
    std::vector<IoInfo> outputs;

    [[nodiscard]] std::string toJson() const;
};

// コマンド `{"cmd": "describe_api"}` または CLI `--schema` で呼び出し可能
[[nodiscard]] ApiSchema describe_api();
[[nodiscard]] std::string describe_api_json();
```

### CLI with --schema (MUST)

```cpp
int main(int argc, char* argv[]) {
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--schema") == 0) {
            std::cout << describe_api_json() << std::endl;
            return 0;
        }
    }
    // ...
}
```

### JSONL Structured Logging (MUST)

機械可読なログ形式:

```cpp
struct LogEntry {
    LogLevel level;
    std::string message;
    std::string source;  // "file:line"
    int64_t timestamp_ms;

    [[nodiscard]] std::string toJsonl() const {
        return std::format(
            R"({{"level":"{}","message":"{}","source":"{}","timestamp_ms":{}}})",
            levelToString(level), message, source, timestamp_ms
        );
    }
};

// 使用
std::cout << LogEntry{
    LogLevel::Error,
    "Failed to bind socket",
    std::format("{}:{}", __FILE__, __LINE__),
    getCurrentTimeMs()
}.toJsonl() << std::endl;
```

---

## Safety Curtain

### Output Limiter (MUST)

物理ハードウェア保護のための出力制限レイヤ:

```cpp
struct SafetyLimits {
    static constexpr double MIN_BPM = 20.0;
    static constexpr double MAX_BPM = 300.0;
    static constexpr double MIN_PITCH_PERCENT = -100.0;
    static constexpr double MAX_PITCH_PERCENT = 100.0;
};

class SafetyCurtain {
public:
    // NaN/Inf もハンドリング
    [[nodiscard]] static constexpr double clampSafe(
        double value,
        double min_val,
        double max_val,
        double default_val
    ) noexcept {
        if (!std::isfinite(value)) {
            return default_val;
        }
        return std::clamp(value, min_val, max_val);
    }

    [[nodiscard]] static constexpr double sanitizeBpm(double bpm) noexcept {
        return clampSafe(bpm, SafetyLimits::MIN_BPM, SafetyLimits::MAX_BPM, 120.0);
    }

    [[nodiscard]] static constexpr double sanitizePitchPercent(double pitch) noexcept {
        return clampSafe(pitch, SafetyLimits::MIN_PITCH_PERCENT,
                        SafetyLimits::MAX_PITCH_PERCENT, 0.0);
    }
};

// 使用: AI やセンサーからの入力を必ず通す
void processExternalInput(double bpm) {
    double safeBpm = SafetyCurtain::sanitizeBpm(bpm);
    // safeBpm は常に安全な範囲内
}
```

---

## Python Bindings

### nanobind (MUST)

NumPy 互換で高性能な Python バインディング:

```cpp
#include <nanobind/nanobind.h>
#include <nanobind/stl/string.h>
#include <nanobind/stl/vector.h>

namespace nb = nanobind;

NB_MODULE(beatlink_py, m) {
    m.doc() = "Beat Link C++ Python bindings";
    m.attr("__version__") = Version::STRING;

    // API introspection
    m.def("describe_api", &describe_api_json,
          "Get API schema as JSON for AI agent introspection");

    // Data classes
    nb::class_<PyBeat>(m, "Beat")
        .def_ro("device_number", &PyBeat::device_number)
        .def_ro("bpm", &PyBeat::bpm)
        .def_ro("beat_in_bar", &PyBeat::beat_in_bar);

    // Functions
    m.def("start_device_finder", []() {
        return DeviceFinder::getInstance().start();
    });

    m.def("add_beat_listener", [](nb::callable callback) {
        BeatFinder::getInstance().addBeatListener([callback](const Beat& beat) {
            nb::gil_scoped_acquire acquire;
            callback(beatToPyBeat(beat));
        });
    });
}
```

### CMake 設定

```cmake
option(BUILD_PYTHON "Build Python bindings" OFF)

if(BUILD_PYTHON)
    find_package(Python 3.8 COMPONENTS Interpreter Development.Module REQUIRED)
    FetchContent_Declare(nanobind
        GIT_REPOSITORY https://github.com/wjakob/nanobind.git
        GIT_TAG v2.0.0)
    FetchContent_MakeAvailable(nanobind)

    nanobind_add_module(projectname_py src/python_bindings.cpp)
    target_link_libraries(projectname_py PRIVATE projectname)
endif()
```

---

## ネットワーク実装

### Asio (Standalone) UDP 受信

```cpp
class UdpReceiver {
public:
    [[nodiscard]] bool start(uint16_t port) {
        try {
            socket_ = std::make_unique<asio::ip::udp::socket>(ioContext_);
            socket_->open(asio::ip::udp::v4());
            socket_->set_option(asio::socket_base::reuse_address(true));
            socket_->bind(asio::ip::udp::endpoint(asio::ip::udp::v4(), port));

            running_.store(true);
            receiverThread_ = std::thread(&UdpReceiver::receiverLoop, this);
            return true;
        } catch (...) {
            return false;  // 例外を外に出さない
        }
    }

private:
    void receiverLoop() {
        std::array<uint8_t, 512> buffer;  // 事前確保
        asio::ip::udp::endpoint senderEndpoint;

        while (running_.load()) {
            asio::error_code ec;
            size_t length = socket_->receive_from(
                asio::buffer(buffer), senderEndpoint, 0, ec);

            if (ec == asio::error::would_block) {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                continue;
            }

            if (!ec && length > 0) {
                // std::span で渡す
                processPacket(std::span<const uint8_t>(buffer.data(), length));
            }
        }
    }

    virtual void processPacket(std::span<const uint8_t> data) = 0;
};
```

---

## ビルドシステム

### CMakeLists.txt (C++20)

```cmake
cmake_minimum_required(VERSION 3.15)
project(projectname VERSION 0.2.0 LANGUAGES CXX)

# C++20 Strict
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Options
option(BUILD_PYTHON "Build Python bindings with nanobind" OFF)

include(FetchContent)

# Asio
FetchContent_Declare(asio
    GIT_REPOSITORY https://github.com/chriskohlhoff/asio.git
    GIT_TAG asio-1-28-0)
FetchContent_MakeAvailable(asio)
add_definitions(-DASIO_STANDALONE)

# Library
add_library(${PROJECT_NAME} src/Core.cpp)
target_include_directories(${PROJECT_NAME} PUBLIC include)
target_compile_features(${PROJECT_NAME} PUBLIC cxx_std_20)

# CLI with --schema
add_executable(${PROJECT_NAME}_cli examples/cli_tool.cpp)
target_link_libraries(${PROJECT_NAME}_cli ${PROJECT_NAME})

# Python bindings
if(BUILD_PYTHON)
    # ... nanobind setup
endif()
```

---

## チェックリスト

### C++20 必須項目 (MUST)

- [ ] `std::span` で配列アクセス（生ポインタ禁止）
- [ ] `std::format` で文字列構築（sprintf 禁止）
- [ ] Concepts でテンプレート制約を明示
- [ ] `[[nodiscard]]` を戻り値のある関数に付与
- [ ] `noexcept` をリアルタイムパスに付与

### Design Patterns (MUST)

- [ ] Handle Pattern（整数IDでオブジェクト管理）
- [ ] Safety Curtain（出力リミッター実装）
- [ ] Time Injection（時間を引数で渡す）
- [ ] No Exceptions in Process Loop
- [ ] No Allocation in Process Loop

### AI Integration (MUST)

- [ ] `describe_api()` 実装
- [ ] CLI `--schema` オプション
- [ ] JSONL ログ出力

### Python (MUST)

- [ ] nanobind バインディング準備
- [ ] `std::span<float>` を numpy 互換で渡せる設計

### Deliverables

- [ ] Core Library (Pure C++20)
- [ ] CLI Tool (JSONL I/O, --schema)
- [ ] Python Module (nanobind)
- [ ] Golden Tests (Python から Core を検証)

---

## 参考資料

- [C++ Core Guidelines](https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines)
- [C++20 Standard](https://en.cppreference.com/w/cpp/20)
- [Asio Documentation](https://think-async.com/Asio/)
- [nanobind](https://github.com/wjakob/nanobind)
- [Dear ImGui](https://github.com/ocornut/imgui)
