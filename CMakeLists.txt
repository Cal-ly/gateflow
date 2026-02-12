cmake_minimum_required(VERSION 3.20)
project(gateflow LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Raylib's CMakeLists uses cmake_minimum_required(VERSION 2.x) which CMake 4.x
# rejects. This policy shim allows it to configure.
set(CMAKE_POLICY_VERSION_MINIMUM 3.5 CACHE STRING "" FORCE)

# Compiler warnings (suppress for Emscripten third-party builds via compile options on our targets only)
if(NOT EMSCRIPTEN)
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# --- Third-party dependencies via FetchContent ---
include(FetchContent)

# Raylib (fetched for the main target; not linked to tests)
FetchContent_Declare(
    raylib
    GIT_REPOSITORY https://github.com/raysan5/raylib.git
    GIT_TAG 5.0
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(raylib)

# --- Project targets ---
add_subdirectory(src)

# Tests are only built for native (not Emscripten)
if(NOT EMSCRIPTEN)
    # Catch2 v3 (test framework)
    FetchContent_Declare(
        Catch2
        GIT_REPOSITORY https://github.com/catchorg/Catch2.git
        GIT_TAG v3.5.2
        GIT_SHALLOW TRUE
    )
    FetchContent_MakeAvailable(Catch2)

    add_subdirectory(tests)
endif()
