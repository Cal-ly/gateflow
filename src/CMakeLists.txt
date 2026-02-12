# --- Simulation library (pure logic, no Raylib) ---
add_library(gateflow_simulation
    simulation/gate.cpp
    simulation/wire.cpp
    simulation/circuit.cpp
    simulation/circuit_builder.cpp
    simulation/nand_decompose.cpp
)
target_include_directories(gateflow_simulation PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_features(gateflow_simulation PUBLIC cxx_std_17)

# --- Timing library (depends on simulation, no Raylib) ---
add_library(gateflow_timing
    timing/propagation_scheduler.cpp
)
target_include_directories(gateflow_timing PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(gateflow_timing PUBLIC gateflow_simulation)
target_compile_features(gateflow_timing PUBLIC cxx_std_17)

# --- Rendering library (depends on timing + simulation + Raylib) ---
add_library(gateflow_rendering
    rendering/layout_engine.cpp
    rendering/gate_renderer.cpp
    rendering/wire_renderer.cpp
    rendering/animation_state.cpp
)
target_include_directories(gateflow_rendering PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(gateflow_rendering PUBLIC gateflow_timing raylib)
target_compile_features(gateflow_rendering PUBLIC cxx_std_17)

# --- UI library (depends on rendering + timing + simulation + Raylib) ---
add_library(gateflow_ui
    ui/input_panel.cpp
    ui/info_panel.cpp
)
target_include_directories(gateflow_ui PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(gateflow_ui PUBLIC gateflow_rendering)
target_compile_features(gateflow_ui PUBLIC cxx_std_17)

# --- Main executable (links UI, which pulls in everything) ---
add_executable(gateflow main.cpp)
target_link_libraries(gateflow PRIVATE gateflow_ui)

# Compiler warnings for our own targets (not third-party)
if(NOT EMSCRIPTEN)
    foreach(tgt gateflow_simulation gateflow_timing gateflow_rendering gateflow_ui gateflow)
        target_compile_options(${tgt} PRIVATE -Wall -Wextra -Wpedantic)
    endforeach()
endif()

# --- Emscripten / WASM build settings ---
if(EMSCRIPTEN)
    set(WASM_SHELL "${CMAKE_CURRENT_SOURCE_DIR}/../web/shell.html")

    # Emscripten link flags for the main executable
    target_link_options(gateflow PRIVATE
        -sUSE_GLFW=3                     # Raylib uses GLFW
        -sALLOW_MEMORY_GROWTH=1          # Dynamic heap
        -sASYNCIFY                        # Allows emscripten_sleep / main_loop
        -sFORCE_FILESYSTEM=1             # Required for Raylib file I/O
        --shell-file ${WASM_SHELL}       # Custom HTML shell
    )

    # Set output to .html so Emscripten generates the page
    set_target_properties(gateflow PROPERTIES
        SUFFIX ".html"
        OUTPUT_NAME "index"
    )

    # Suppress warnings that flood from Emscripten system headers in our code
    target_compile_options(gateflow_rendering PRIVATE -Wno-unused-parameter)
    target_compile_options(gateflow_ui PRIVATE -Wno-unused-parameter)
endif()
