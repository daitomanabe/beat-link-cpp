cmake_minimum_required(VERSION 3.15)
project(beatlink VERSION 0.2.0 LANGUAGES CXX C)

# Allow older CMake versions in dependencies
set(CMAKE_POLICY_DEFAULT_CMP0048 NEW)
if(POLICY CMP0135)
    cmake_policy(SET CMP0135 NEW)  # Use timestamps from download time
endif()

# C++17 Standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Options
option(BEATLINK_BUILD_PYTHON "Build Python bindings with nanobind" OFF)
option(BEATLINK_BUILD_TESTS "Build unit tests with Catch2" OFF)
option(BEATLINK_USE_SQLCIPHER "Use SQLCipher for encrypted database support" OFF)

include(FetchContent)

# =============================================================================
# OpusProvider Dependencies
# =============================================================================

# miniz - ZIP library
# Use a newer tag that supports CMake 3.5+
FetchContent_Declare(
    miniz
    GIT_REPOSITORY https://github.com/richgel999/miniz.git
    GIT_TAG        master
    CMAKE_ARGS -DCMAKE_POLICY_VERSION_MINIMUM=3.5
)
set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BUILD_FUZZERS OFF CACHE BOOL "" FORCE)
set(AMALGAMATE_SOURCES OFF CACHE BOOL "" FORCE)
set(INSTALL_PROJECT OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(miniz)

# sqlite3 amalgamation
FetchContent_Declare(
    sqlite3
    URL https://www.sqlite.org/2024/sqlite-amalgamation-3450000.zip
)
FetchContent_MakeAvailable(sqlite3)
add_library(sqlite3 STATIC ${sqlite3_SOURCE_DIR}/sqlite3.c)
target_include_directories(sqlite3 PUBLIC ${sqlite3_SOURCE_DIR})
target_compile_definitions(sqlite3 PRIVATE SQLITE_ENABLE_FTS5 SQLITE_ENABLE_JSON1)

# Kaitai Struct C++ Runtime
FetchContent_Declare(
    kaitai_runtime
    GIT_REPOSITORY https://github.com/kaitai-io/kaitai_struct_cpp_stl_runtime.git
    GIT_TAG        0.10.1
)
FetchContent_MakeAvailable(kaitai_runtime)
# kaitai_struct_cpp_stl_runtime target is created by the fetched CMakeLists.txt

# utf8proc - Unicode normalization
FetchContent_Declare(
    utf8proc
    GIT_REPOSITORY https://github.com/JuliaStrings/utf8proc.git
    GIT_TAG        v2.9.0
)
set(UTF8PROC_INSTALL OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(utf8proc)

# stb_image - Image loading (header-only)
FetchContent_Declare(
    stb
    GIT_REPOSITORY https://github.com/nothings/stb.git
    GIT_TAG        master
)
FetchContent_MakeAvailable(stb)
# Create interface library for stb_image
add_library(stb_image INTERFACE)
target_include_directories(stb_image INTERFACE ${stb_SOURCE_DIR})

# fmt - String formatting library (C++17 compatible std::format alternative)
FetchContent_Declare(
    fmt
    GIT_REPOSITORY https://github.com/fmtlib/fmt.git
    GIT_TAG        10.2.1
)
FetchContent_MakeAvailable(fmt)

# SQLCipher (optional)
if(BEATLINK_USE_SQLCIPHER)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(SQLCIPHER REQUIRED sqlcipher)
    add_compile_definitions(BEATLINK_HAS_SQLCIPHER)
endif()

# Find Asio (standalone, non-Boost)
find_path(ASIO_INCLUDE_DIR asio.hpp
    HINTS
        /usr/local/include
        /opt/homebrew/include
        ${ASIO_ROOT}
)

if(NOT ASIO_INCLUDE_DIR)
    message(STATUS "Asio not found. Attempting to fetch...")
    FetchContent_Declare(
        asio
        GIT_REPOSITORY https://github.com/chriskohlhoff/asio.git
        GIT_TAG asio-1-28-0
    )
    FetchContent_MakeAvailable(asio)
    set(ASIO_INCLUDE_DIR ${asio_SOURCE_DIR}/asio/include)
endif()

message(STATUS "Asio include dir: ${ASIO_INCLUDE_DIR}")

# Define ASIO_STANDALONE to use standalone Asio without Boost
add_definitions(-DASIO_STANDALONE)

# Fetch GLFW
FetchContent_Declare(
    glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG 3.3.8
)
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(glfw)

# Fetch Dear ImGui
FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG v1.90.1
)
FetchContent_MakeAvailable(imgui)

# ImGui library
set(IMGUI_SOURCES
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_demo.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
)

add_library(imgui STATIC ${IMGUI_SOURCES})
target_include_directories(imgui PUBLIC
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/backends
)
target_link_libraries(imgui PUBLIC glfw)

# Find OpenGL
find_package(OpenGL REQUIRED)
target_link_libraries(imgui PUBLIC OpenGL::GL)

# Library sources
set(BEATLINK_SOURCES
    src/Util.cpp
    src/DeviceAnnouncement.cpp
    src/DeviceFinder.cpp
    src/Beat.cpp
    src/BeatFinder.cpp
    src/DeviceUpdate.cpp
    src/CdjStatus.cpp
    src/MixerStatus.cpp
    src/Metronome.cpp
    src/PrecisePosition.cpp
    src/MediaDetails.cpp
    src/data/SlotReference.cpp
    src/data/CueList.cpp
    src/data/TrackMetadata.cpp
    src/data/WaveformPreview.cpp
    src/data/WaveformDetail.cpp
    src/data/MetadataFinder.cpp
    src/data/BeatGridFinder.cpp
    src/data/WaveformFinder.cpp
    src/data/ArtFinder.cpp
    src/data/AnalysisTagFinder.cpp
    src/data/CrateDigger.cpp
    src/data/MenuLoader.cpp
    src/data/SignatureFinder.cpp
    src/data/TimeFinder.cpp
    src/data/OpusProvider.cpp
    src/data/ZipArchive.cpp
    src/data/SqliteConnection.cpp
    src/data/AnlzParser.cpp
    src/data/PdbParser.cpp
    src/data/AlbumArt.cpp
    src/dbserver/DataReader.cpp
    src/dbserver/Field.cpp
    src/dbserver/NumberField.cpp
    src/dbserver/StringField.cpp
    src/dbserver/BinaryField.cpp
    src/dbserver/Message.cpp
    src/dbserver/Client.cpp
    src/dbserver/ConnectionManager.cpp
    src/VirtualRekordbox.cpp
    src/VirtualCdj.cpp
)

# Kaitai generated parsers (generated from .ksy files)
set(KAITAI_GENERATED_SOURCES
    src/generated/rekordbox_anlz.cpp
    src/generated/rekordbox_pdb.cpp
)

# Create library
add_library(beatlink ${BEATLINK_SOURCES} ${KAITAI_GENERATED_SOURCES})
target_include_directories(beatlink PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src/generated
    ${ASIO_INCLUDE_DIR}
)

# Link OpusProvider dependencies
target_link_libraries(beatlink PUBLIC
    miniz
    sqlite3
    kaitai_struct_cpp_stl_runtime
    utf8proc
    stb_image
    fmt::fmt
)

# SQLCipher linking (optional)
if(BEATLINK_USE_SQLCIPHER)
    target_include_directories(beatlink PRIVATE ${SQLCIPHER_INCLUDE_DIRS})
    target_link_libraries(beatlink PRIVATE ${SQLCIPHER_LIBRARIES})
endif()

# Platform-specific settings
if(APPLE)
    target_compile_definitions(beatlink PRIVATE _DARWIN_C_SOURCE)
elseif(WIN32)
    target_compile_definitions(beatlink PRIVATE _WIN32_WINNT=0x0601)
    target_link_libraries(beatlink PUBLIC ws2_32)
endif()

# Threading support
find_package(Threads REQUIRED)
target_link_libraries(beatlink PUBLIC Threads::Threads)

# Console example application
add_executable(beatlink_example examples/simple_beat_listener.cpp)
target_link_libraries(beatlink_example PRIVATE beatlink Threads::Threads)

# GUI application with Dear ImGui
add_executable(beatlink_gui examples/beatlink_gui.cpp)
target_link_libraries(beatlink_gui PRIVATE beatlink imgui Threads::Threads)
target_include_directories(beatlink_gui PRIVATE ${imgui_SOURCE_DIR})

# macOS specific
if(APPLE)
    target_link_libraries(beatlink_gui PRIVATE "-framework Cocoa" "-framework IOKit" "-framework CoreVideo")
endif()

# CLI tool with --schema support
add_executable(beatlink_cli examples/beatlink_cli.cpp)
target_link_libraries(beatlink_cli PRIVATE beatlink Threads::Threads)

# Python bindings (nanobind)
if(BEATLINK_BUILD_PYTHON)
    find_package(Python 3.8 COMPONENTS Interpreter Development.Module REQUIRED)

    FetchContent_Declare(
        nanobind
        GIT_REPOSITORY https://github.com/wjakob/nanobind.git
        GIT_TAG v2.0.0
    )
    FetchContent_MakeAvailable(nanobind)

    nanobind_add_module(beatlink_py src/python_bindings.cpp)
    target_link_libraries(beatlink_py PRIVATE beatlink)
    target_include_directories(beatlink_py PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
endif()

# Unit tests with Catch2
if(BEATLINK_BUILD_TESTS)
    FetchContent_Declare(
        Catch2
        GIT_REPOSITORY https://github.com/catchorg/Catch2.git
        GIT_TAG v3.5.0
    )
    FetchContent_MakeAvailable(Catch2)

    # Test executable
    add_executable(beatlink_tests
        tests/test_main.cpp
        tests/test_beat.cpp
        tests/test_cdj_status.cpp
        tests/test_util.cpp
        tests/test_data_structures.cpp
    )
    target_link_libraries(beatlink_tests PRIVATE beatlink Catch2::Catch2WithMain)
    target_include_directories(beatlink_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)

    # Enable CTest
    include(CTest)
    include(Catch)
    catch_discover_tests(beatlink_tests)
endif()

# Installation
install(TARGETS beatlink
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)
install(DIRECTORY include/beatlink DESTINATION include)
