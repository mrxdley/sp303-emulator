cmake_minimum_required(VERSION 3.20)
project(sp303 VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)

# ─── Core library ─────────────────────────────────────────────────────────────

add_library(sp303 src/sp303.cpp)
target_include_directories(sp303 PUBLIC include PRIVATE src)
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

# ─── Renderer ─────────────────────────────────────────────────────────────────

add_executable(sp303_renderer renderer/main.cpp)
target_link_libraries(sp303_renderer PRIVATE sp303 raylib nlohmann_json::nlohmann_json)
target_compile_options(sp303_renderer PRIVATE -Wall -Wextra)
