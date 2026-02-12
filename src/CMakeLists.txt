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
    rendering/app_font.cpp
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

        if(GATEFLOW_ENABLE_SANITIZERS AND CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
            target_compile_options(${tgt} PRIVATE
                $<$<CONFIG:Debug>:-fsanitize=address,undefined>
                $<$<CONFIG:Debug>:-fno-omit-frame-pointer>
            )
            target_link_options(${tgt} PRIVATE
                $<$<CONFIG:Debug>:-fsanitize=address,undefined>
            )
        endif()
    endforeach()
endif()

# --- Copy font resources next to executable so it can be loaded at runtime ---
if(NOT EMSCRIPTEN)
    add_custom_command(TARGET gateflow POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory
            $<TARGET_FILE_DIR:gateflow>/resources/fonts
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${CMAKE_CURRENT_SOURCE_DIR}/../resources/fonts/Hack-Regular.ttf
            $<TARGET_FILE_DIR:gateflow>/resources/fonts/Hack-Regular.ttf
        COMMENT "Copying font resources"
    )
endif()

# --- Emscripten / WASM build settings ---
if(EMSCRIPTEN)
    set(WASM_SHELL "${CMAKE_CURRENT_SOURCE_DIR}/../web/shell.html")

    # Emscripten link flags for the main executable
    target_link_options(gateflow PRIVATE
        -sUSE_GLFW=3                     # Raylib uses GLFW
        -sGL_ENABLE_GET_PROC_ADDRESS     # Enable glfwGetProcAddress() 
        -sALLOW_MEMORY_GROWTH=1          # Dynamic heap
        --shell-file ${WASM_SHELL}       # Custom HTML shell
        --preload-file ${CMAKE_CURRENT_SOURCE_DIR}/../resources/fonts/Hack-Regular.ttf@resources/fonts/Hack-Regular.ttf
    )

    if(GATEFLOW_EMSCRIPTEN_ENABLE_ASYNCIFY)
        target_link_options(gateflow PRIVATE -sASYNCIFY)
    endif()

    if(GATEFLOW_EMSCRIPTEN_ENABLE_FILESYSTEM)
        target_link_options(gateflow PRIVATE -sFORCE_FILESYSTEM=1)
    endif()

    # Set output to .html so Emscripten generates the page
    set_target_properties(gateflow PROPERTIES
        SUFFIX ".html"
        OUTPUT_NAME "index"
    )

    # Suppress warnings that flood from Emscripten system headers in our code
    target_compile_options(gateflow_rendering PRIVATE -Wno-unused-parameter)
    target_compile_options(gateflow_ui PRIVATE -Wno-unused-parameter)
endif()
