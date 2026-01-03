cmake_minimum_required(VERSION 3.15)
project(beatlink VERSION 0.2.0 LANGUAGES CXX)

# C++17 Standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Option to build Python bindings
option(BEATLINK_BUILD_PYTHON "Build Python bindings with nanobind" OFF)

include(FetchContent)

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
    src/VirtualCdj.cpp
)

# Create library
add_library(beatlink ${BEATLINK_SOURCES})
target_include_directories(beatlink PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${ASIO_INCLUDE_DIR}
)

# Platform-specific settings
if(APPLE)
    target_compile_definitions(beatlink PRIVATE _DARWIN_C_SOURCE)
elseif(WIN32)
    target_compile_definitions(beatlink PRIVATE _WIN32_WINNT=0x0601)
    target_link_libraries(beatlink ws2_32)
endif()

# Console example application
add_executable(beatlink_example examples/simple_beat_listener.cpp)
target_link_libraries(beatlink_example beatlink)

# GUI application with Dear ImGui
add_executable(beatlink_gui examples/beatlink_gui.cpp)
target_link_libraries(beatlink_gui beatlink imgui)
target_include_directories(beatlink_gui PRIVATE ${imgui_SOURCE_DIR})

# Threading support
find_package(Threads REQUIRED)
target_link_libraries(beatlink Threads::Threads)
target_link_libraries(beatlink_example Threads::Threads)
target_link_libraries(beatlink_gui Threads::Threads)

# macOS specific
if(APPLE)
    target_link_libraries(beatlink_gui "-framework Cocoa" "-framework IOKit" "-framework CoreVideo")
endif()

# CLI tool with --schema support
add_executable(beatlink_cli examples/beatlink_cli.cpp)
target_link_libraries(beatlink_cli beatlink)
target_link_libraries(beatlink_cli Threads::Threads)

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

# Installation
install(TARGETS beatlink
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)
install(DIRECTORY include/beatlink DESTINATION include)
