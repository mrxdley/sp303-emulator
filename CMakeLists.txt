cmake_minimum_required(VERSION 3.20)
project(sp303 VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)

# ─── Core library ─────────────────────────────────────────────────────────────

add_library(sp303
    core/sp303.cpp
    core/pattern_sequencer.cpp
    core/memory_card.cpp)
target_include_directories(sp303 PUBLIC core PRIVATE core)
target_compile_options(sp303 PRIVATE -Wall -Wextra -Wpedantic)

if(CMAKE_SYSTEM_NAME STREQUAL "Android")
    target_compile_definitions(sp303 PRIVATE SP303_ANDROID)
endif()

# ─── Smoke test ───────────────────────────────────────────────────────────────

add_executable(sp303_test tests/smoke.cpp)
target_link_libraries(sp303_test PRIVATE sp303)

# ─── Raylib ───────────────────────────────────────────────────────────────────

FetchContent_Declare(
    raylib
    GIT_REPOSITORY https://github.com/raysan5/raylib.git
    GIT_TAG        5.5
    GIT_SHALLOW    TRUE
)
set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BUILD_GAMES    OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(raylib)

# ─── nlohmann/json ────────────────────────────────────────────────────────────

FetchContent_Declare(
    nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG        v3.11.3
    GIT_SHALLOW    TRUE
)
set(JSON_BuildTests OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(nlohmann_json)

# ─── miniaudio ────────────────────────────────────────────────────────────────

FetchContent_Declare(
    miniaudio
    GIT_REPOSITORY https://github.com/mackron/miniaudio.git
    GIT_TAG        0.11.21
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(miniaudio)

add_library(miniaudio_iface INTERFACE)
target_include_directories(miniaudio_iface INTERFACE ${miniaudio_SOURCE_DIR})

# ─── Audio adapter ────────────────────────────────────────────────────────────

add_library(sp303_audio
    audio/sp303_audio.cpp
    audio/sample_ops.cpp
    audio/effect.cpp
    audio/effects/filter_drive.cpp
    audio/effects/pitch.cpp
    audio/effects/delay.cpp)
target_include_directories(sp303_audio PUBLIC audio PRIVATE core)
target_link_libraries(sp303_audio PRIVATE miniaudio_iface)

if(UNIX AND NOT APPLE AND NOT ANDROID)
    target_link_libraries(sp303_audio PRIVATE dl pthread m)
endif()
if(APPLE)
    target_link_libraries(sp303_audio PRIVATE
        "-framework CoreAudio"
        "-framework AudioToolbox"
        "-framework AudioUnit")
endif()

# ─── Renderer ─────────────────────────────────────────────────────────────────

add_executable(sp303_renderer
    renderer/main.cpp
    renderer/controller.cpp
    renderer/project_io.cpp)
target_link_libraries(sp303_renderer PRIVATE
    sp303 sp303_audio raylib nlohmann_json::nlohmann_json)
target_include_directories(sp303_renderer PRIVATE renderer)
target_compile_options(sp303_renderer PRIVATE -Wall -Wextra -Wno-stringop-truncation)
