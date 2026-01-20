# Contributing to Beat Link C++

Thank you for your interest in contributing to Beat Link C++! This document provides guidelines and information for contributors.

## Development Environment

### Requirements

- **Compiler**: C++20 compatible
  - GCC 11+
  - Clang 14+
  - MSVC 2022+
- **CMake**: 3.15+
- **Python**: 3.8+ (for bindings and tests)
- **Git**: For version control

### Setting Up

```bash
# Clone the repository
git clone https://github.com/daitomanabe/beat-link-cpp.git
cd beat-link-cpp

# Create build directory
mkdir build && cd build

# Configure with all options
cmake .. \
    -DBEATLINK_BUILD_PYTHON=ON \
    -DBEATLINK_BUILD_TESTS=ON

# Build
make -j$(nproc)

# Run tests
ctest
```

## Code Style

### C++ Guidelines

1. **Use C++20 features appropriately**
   - `std::span` for non-owning views
   - `std::optional` for nullable returns
   - Designated initializers for structs
   - `constexpr` where possible

2. **Follow existing patterns**
   - Factory functions: `static std::optional<T> create(...)` for packet parsing
   - Singletons: `getInstance()` for finders
   - Listeners: `add*Listener()` / `remove*Listener()`

3. **Memory safety**
   - No raw `new`/`delete` - use smart pointers
   - Prefer value types and references
   - Use `std::span` instead of pointer + size

4. **Error handling**
   - Return `std::optional` or `std::expected` instead of throwing
   - Use factory methods for validation

5. **Naming conventions**
   - Classes: `PascalCase`
   - Functions/methods: `camelCase`
   - Constants: `UPPER_SNAKE_CASE`
   - Member variables: `camelCase_` (trailing underscore)

### Example Code Style

```cpp
class ExampleClass {
public:
    // Factory method for validated construction
    [[nodiscard]] static std::optional<ExampleClass> create(
        std::span<const uint8_t> data
    ) noexcept {
        if (data.size() < MIN_SIZE) {
            return std::nullopt;
        }
        return ExampleClass(data, NoValidateTag{});
    }

    // Getters
    int getValue() const { return value_; }
    std::string_view getName() const { return name_; }

private:
    struct NoValidateTag {};

    ExampleClass(std::span<const uint8_t> data, NoValidateTag) noexcept
        : value_(static_cast<int>(Util::bytesToNumber(data.data(), 0, 4)))
        , name_(reinterpret_cast<const char*>(data.data() + 4))
    {}

    static constexpr size_t MIN_SIZE = 32;

    int value_;
    std::string name_;
};
```

## Project Structure

```
beat-link-cpp/
├── include/beatlink/       # Public headers
│   ├── BeatLink.hpp        # Main include header
│   ├── *.hpp               # Core classes
│   ├── data/               # Data types and finders
│   └── dbserver/           # DB server protocol
├── src/                    # Implementation files
│   ├── *.cpp               # Core implementations
│   ├── data/               # Data implementations
│   ├── dbserver/           # DB server implementations
│   ├── generated/          # Kaitai-generated parsers
│   └── python_bindings.cpp # Python bindings
├── examples/               # Example applications
├── tests/                  # Test files
└── external/               # Git submodules
```

## Adding New Features

### Adding a New Packet Type

1. Create header in `include/beatlink/`:
   ```cpp
   // NewPacket.hpp
   #pragma once
   #include "DeviceUpdate.hpp"

   namespace beatlink {
   class NewPacket : public DeviceUpdate {
       // ...
   };
   }
   ```

2. Add implementation in `src/NewPacket.cpp`

3. Update `CMakeLists.txt`:
   ```cmake
   set(BEATLINK_SOURCES
       # ... existing sources
       src/NewPacket.cpp
   )
   ```

4. Add tests in `tests/test_new_packet.cpp`

5. Add Python bindings in `src/python_bindings.cpp`

### Adding Python Bindings

Use nanobind to expose C++ classes:

```cpp
// In python_bindings.cpp

// Struct for Python
struct PyNewData {
    int value;
    std::string name;
};

// Conversion function
PyNewData newDataToPy(const NewData& data) {
    return PyNewData{
        .value = data.getValue(),
        .name = data.getName()
    };
}

// In NB_MODULE
nb::class_<PyNewData>(m, "NewData")
    .def_ro("value", &PyNewData::value)
    .def_ro("name", &PyNewData::name);

m.def("get_new_data", [](int player) -> std::optional<PyNewData> {
    auto data = NewDataFinder::getInstance().getLatestFor(player);
    if (!data) { return std::nullopt; }
    return newDataToPy(*data);
}, "Get new data for a player");
```

## Testing

### Writing Unit Tests

Use Catch2 for C++ tests:

```cpp
// tests/test_example.cpp
#include <catch2/catch_all.hpp>
#include <beatlink/Example.hpp>

TEST_CASE("Example::create with valid data", "[Example]") {
    std::array<uint8_t, 32> data{};
    // ... setup data

    auto result = Example::create(std::span(data));

    REQUIRE(result.has_value());
    CHECK(result->getValue() == expected);
}

TEST_CASE("Example::create with invalid data", "[Example]") {
    std::array<uint8_t, 10> shortData{};

    auto result = Example::create(std::span(shortData));

    REQUIRE_FALSE(result.has_value());
}
```

### Running Tests

```bash
# Build with tests
cmake .. -DBEATLINK_BUILD_TESTS=ON
make beatlink_tests

# Run all tests
./beatlink_tests

# Run specific tests
./beatlink_tests [Beat]
./beatlink_tests [CdjStatus]

# Run with verbose output
./beatlink_tests -s

# Use CTest
ctest --output-on-failure
```

## Pull Request Process

1. **Create a branch**: `feature/description` or `fix/description`
2. **Make changes**: Follow code style guidelines
3. **Add tests**: Cover new functionality
4. **Update documentation**: README, comments, PLANS.md if needed
5. **Run tests**: Ensure all tests pass
6. **Create PR**: Describe changes and link related issues

### Commit Message Format

```
type: short description

Longer description if needed.
Explain what and why, not how.

Co-Authored-By: Your Name <email@example.com>
```

Types: `feat`, `fix`, `docs`, `test`, `refactor`, `chore`

## Protocol Reference

- **DJ Link Analysis**: https://djl-analysis.deepsymmetry.org/
- **Java Beat Link**: `external/beat-link/`
- **Kaitai Struct definitions**: `external/crate-digger-cpp/`

### Network Ports

| Port  | Protocol | Purpose |
|-------|----------|---------|
| 50000 | UDP      | Device announcement |
| 50001 | UDP      | Beat packets |
| 50002 | UDP      | Status updates |
| 12523 | TCP      | DB server queries |

## License

This project is licensed under EPL-2.0, consistent with the original Java Beat Link library.

## Questions?

- Open an issue for bugs or feature requests
- Check existing issues before creating new ones
- Reference the Java Beat Link implementation when in doubt
