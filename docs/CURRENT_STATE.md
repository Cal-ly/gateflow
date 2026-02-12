# Gateflow — Current Project State

> Last updated: 12 February 2026, end of Phase 6 — Responsive UI & Polish

## Project Overview

Interactive visual simulator showing how a CPU performs integer addition (0–99) at the logic-gate level. C++17, Raylib 5.x, CMake, targeting Linux native + WebAssembly.

## Completed: Phase 1 — Simulation Foundation ✅

### Build System
- Top-level CMake with FetchContent for Raylib 5.0 and Catch2 v3.5.2
- `gateflow_simulation` static library (pure logic, no Raylib dependency)
- `gateflow_timing` static library (links simulation, no Raylib dependency)
- `gateflow_rendering` static library (links timing + Raylib)
- `gateflow_ui` static library (links rendering + Raylib)
- `gateflow` executable (links ui → rendering → timing → simulation + Raylib)
- `gateflow_tests` executable (links simulation + Catch2 only)
- CMake policy shim for Raylib compatibility with CMake 4.x
- `.clang-format` (LLVM-based, 4-space indent) applied to all C++ sources

### Simulation Layer (`src/simulation/`)
- **Gate model** — `GateType` enum (NAND, AND, OR, XOR, NOT, BUFFER), pure `evaluate()` function, `Gate` class
- **Wire model** — `Wire` class with value/previous_value tracking, source gate, destination list, `remove_destination()` for safe disconnection
- **Circuit model** — Owns gates/wires via `unique_ptr`, builder API (add_gate, add_wire, connect, mark_input/output, finalize), Kahn's topological sort, `propagate()` returning `PropagationResult`
- **Circuit builders** — `build_half_adder()`, `build_full_adder()`, `build_ripple_carry_adder(n)` (bit 0 uses half adder, bits 1+ use full adders with carry chain)
- **NAND decomposition** — `decompose_to_nand()` replaces all gate types in-place with NAND equivalents, properly managing bidirectional wire/gate connectivity

### Test Suite (37 tests, 309 assertions, all passing)
- `test_gate.cpp` — All gate types exhaustive truth tables + invalid input rejection
- `test_circuit.cpp` — NOT circuit, chained gates, propagation result tracking, topo order, error handling
- `test_circuit_builder.cpp` — Half adder (4 rows), full adder (8 rows), 7-bit RCA boundary values + 20 random pairs, 1-bit and 4-bit adders
- `test_nand_decompose.cpp` — Each gate type individually, half adder, full adder, 7-bit RCA spot checks, all verified NAND-only post-decomposition
- `test_propagation.cpp` — Topological ordering, diamond circuits, fan-out, multiple propagations, wire change detection

### Quality
- Zero warnings with `-Wall -Wextra -Wpedantic` (our code)
- No raw `new`/`delete`
- No Raylib headers in simulation layer
- clang-format applied

## Completed: Phase 2 — Layout and Static Rendering ✅

### Rendering Layer (`src/rendering/`)
- **Layout engine** (`layout_engine.hpp/cpp`) — `compute_layout(const Circuit&)` produces a `Layout` struct with gate rectangles, wire polyline paths, input/output positions, and bounding box. RCA-specific layout: columns right-to-left (bit 0 rightmost), gates top-to-bottom within each column, half adder at bit 0 (2 gates), full adders at bits 1+ (5 gates each). Generic depth-based fallback for non-RCA circuits.
- **Gate renderer** (`gate_renderer.hpp/cpp`) — `draw_gates()` draws rounded rectangles with gate type label centered. Gray fill for inactive (output=0), green fill for active (output=1). `draw_io_labels()` draws input (A0–A6, B0–B6) and output (S0–S6, Cout) labels with colored dots.
- **Wire renderer** (`wire_renderer.hpp/cpp`) — `draw_wires()` draws polyline segments with Manhattan routing. Thin dark gray for carrying 0, thick bright green for carrying 1. Dots at waypoints for visual clarity.

### Main Application
- Builds a 7-bit ripple-carry adder, hard-codes inputs 42 + 37 = 79
- Propagates circuit fully, computes layout, renders static final state
- Raylib window (1280×720, 60 FPS), dark background, centered circuit
- Title shows "42 + 37 = 79", footer shows controls and circuit type
- Layout uses 40 pixels per logical unit scaling

## Completed: Phase 3 — Animation and Timing ✅

### Timing Layer (`src/timing/`)
- **PropagationScheduler** (`propagation_scheduler.hpp/cpp`) — Computes gate depths via longest-path from inputs. `PlaybackMode` enum: REALTIME, PAUSED, STEP. `tick(dt)` advances `current_depth_` at configurable speed (depths/sec). Query functions: `is_gate_resolved()`, `is_wire_resolved()`, `gate_resolve_fraction()`, `wire_signal_progress()`. Controls: `toggle_pause()`, `step()`, `reset()`, `set_speed()`, `is_complete()`.

### Animation State (`src/rendering/`)
- **AnimationState** (`animation_state.hpp/cpp`) — Per-gate animation: alpha fade-in, pulse phase for pending gates, resolved flag. Per-wire animation: signal_progress (0→1 travel), resolved flag. `update(dt, scheduler)` drives visual state from scheduler timing.
- **Gate renderer updated** — Three visual states: pending (dim + pulse animation), resolved-inactive (gray), resolved-active (green). Color lerping for smooth transitions.
- **Wire renderer updated** — Signal pulse dot traveling along polyline path, partial coloring of resolved portion, pending wires rendered dim.

### Main Application (Phase 3 state)
- Keyboard controls: SPACE (pause/play), RIGHT (step), R (reset), +/- (speed)
- HUD shows mode (PLAYING/PAUSED), speed, depth progress, completion indicator

## Completed: Phase 4 — UI and Interactivity ✅

### UI Layer (`src/ui/`)
- **Input Panel** (`input_panel.hpp/cpp`) — `UIState` struct holds input_a (0–99), input_b (0–99), speed (0.5–20.0), is_running, show_nand, editing state, cursor positions, slider drag state. `UIAction` struct returns per-frame events: inputs_changed, run/pause/step/reset pressed, nand_toggled, speed_changed. Custom-drawn UI: number fields with keyboard input, Run/Pause/Step/Reset buttons, speed slider, NAND toggle — all with Raylib primitives (no raygui/ImGui).
- **Info Panel** (`info_panel.hpp/cpp`) — Binary readouts for A, B, and Sum. A/B bits always shown as resolved. Sum bits colored by output wire resolution state (green=1, gray=0, dim=pending). Decimal result displayed on propagation completion. Status text: "Ready", "Processing bit N", "Carry propagating through bit N... [depth/max]", "Propagation complete".

### Main Application (Phase 4 — current)
- `AppState` struct bundles circuit, layout, scheduler, animation, result, and camera offset
- `rebuild_app_state()` performs full rebuild: circuit → optional NAND decompose → set inputs → propagate → layout → scheduler → animation → center camera
- `reset_propagation()` lightweight path: re-set inputs → propagate → reset scheduler/anim (no rebuild)
- UIAction dispatch: NAND toggle → full rebuild; input change or Run → reset propagation; Pause → toggle; Step → single depth; Reset → restart; Speed → update scheduler
- Keyboard shortcuts (SPACE/RIGHT/R) still work when not editing text fields
- Circuit renders in left/center area, UI panels stacked on right side
- HUD at bottom-left: mode indicator, depth counter, NAND view badge

## Completed: Phase 5 — WASM Build and Polish ✅

### Emscripten Build Configuration
- Top-level CMakeLists.txt conditionally skips Catch2 and test target for Emscripten builds
- Compiler warnings applied per-target (not globally) to avoid noise from Emscripten system headers
- src/CMakeLists.txt adds Emscripten link flags: `-sUSE_GLFW=3`, `-sALLOW_MEMORY_GROWTH=1`, `-sASYNCIFY`, `-sFORCE_FILESYSTEM=1`
- Custom HTML shell via `--shell-file web/shell.html`
- Output set to `index.html` for direct web serving
- Build: `emcmake cmake -B build-web -S . -DCMAKE_BUILD_TYPE=Release -DPLATFORM=Web`

### main.cpp Emscripten Support
- Frame logic extracted into `frame_tick(FrameState&)` function callable from both native and WASM
- `FrameState` struct bundles all mutable state (UIState + AppState)
- `#ifdef __EMSCRIPTEN__` guard uses `emscripten_set_main_loop_arg()` with void* callback
- Native build uses standard `while (!WindowShouldClose())` loop — zero overhead

### Custom HTML Shell (`web/shell.html`)
- Dark-themed page matching the app's visual style
- Responsive canvas scaling (CSS transforms, preserves 1280×720 internal resolution)
- Loading overlay with spinner and download progress bar
- Header with project name, footer with GitHub link

### GitHub Actions Deployment (`.github/workflows/deploy.yml`)
- Automated WASM build on push to `main` branch
- Uses `mymindstorm/setup-emsdk@v14` for Emscripten toolchain
- Builds and deploys `index.html`, `index.js`, `index.wasm` to GitHub Pages
- Concurrency group prevents parallel deployments

### README.md
- Full project description with feature list
- Architecture diagram and layer dependency table
- Build instructions for both native Linux and WebAssembly
- Usage table with all controls and keyboard shortcuts
- "How It Works" section explaining the simulation pipeline
- Tech stack, project structure, license

### Build Artifacts
- WASM binary: ~200 KB
- JavaScript glue: ~170 KB
- HTML shell: ~3 KB

## All Phases Complete ✅

The project is feature-complete across all 6 phases.
- Phases 1–5: Simulation, rendering, animation, UI, WASM
- Phase 6: Responsive UI, custom font, web canvas, polish

## Completed: Phase 6 — Responsive UI & Polish ✅

### Custom Font System (`src/rendering/app_font.hpp/cpp`)
- Bundled Hack Regular TTF (system font) into `resources/fonts/`
- Centralised `init_app_font()` / `cleanup_app_font()` with fallback chain: bundled → system → Raylib default
- `DrawAppText()` / `MeasureAppText()` wrappers replace all raw Raylib `DrawText()` / `MeasureText()` calls across 5 source files
- Font loaded at 48px base with bilinear filtering for crisp rendering at all sizes

### Responsive UI Scale System (`src/ui/ui_scale.hpp/cpp`)
- `UIScale` struct: master factor, panel_w, margin, 4 font sizes, 6 spacing values, 6 viewport values
- `update_ui_scale(screen_w, screen_h)` called each frame — computes metrics from window dimensions
- Two-tier scaling: **panel factor** capped at 1.0 (fonts/buttons/spacing never upscale beyond 720p baseline), **viewport factor** uncapped (circuit area, title, HUD scale freely)
- Panel width: 30% of screen, clamped 280–500px
- Master factor: blend of height-dominant (70%) and width-based (30%) scaling, clamped 0.6–1.8

### Proportional Panel Layout
- Input panel and info (result) panel use `ui_scale()` for all metrics — no hardcoded pixel sizes
- `draw_info_panel()` pre-computes height from scaled row arithmetic; draws background first, content on top
- `draw_explanation_panel()` accepts `available_h` parameter — fills remaining vertical space below input + result panels
- All three panels dynamically stack: input → result → explanation, with explanation absorbing leftover height

### Responsive Circuit Viewport
- `circuit_padding`, `max_ppu` (max pixels-per-unit) scale with viewport factor
- Title font, HUD font, progress bar height/font all scale with viewport factor
- `refit_circuit()` uses scaled padding and max_ppu for circuit fitting
- Size-change detection uses `last_w`/`last_h` tracking (works for both native `IsWindowResized()` and Emscripten canvas changes)

### Web Canvas Responsiveness
- `shell.html` now sets **actual canvas resolution** (`canvas.width`/`canvas.height`) to browser viewport size, not just CSS scaling of a fixed buffer
- Raylib's `GetScreenWidth()`/`GetScreenHeight()` returns real viewport dimensions in WASM
- Resize debounced (80ms) to avoid excessive redraws during window drag
- CSS `min-width: 900px` / `min-height: 500px` on canvas container
- Desktop: `SetWindowMinSize(900, 500)` already enforced

### GitHub Pages Deployment Fix
- Switched from `--preload-file` to `--embed-file` for font bundling — eliminates separate `index.data` fetch that caused loading hang
- Deploy workflow copies `index.data` with `|| true` safety net

### Visual Fixes
- Input node label overlap: A labels right-aligned, B labels left-aligned, font reduced to 14px
- UTF-8 glyph rendering: replaced `✓`/`→`/`·` with ASCII equivalents (`ok`/`..`/`-`) — Hack Regular doesn't include all Unicode glyphs in the loaded range

## File Structure

```
gateflow/
├── CMakeLists.txt
├── .clang-format
├── .gitignore
├── README.md
├── LICENSE
├── CLAUDE_CODE_GUIDELINES.md
├── gateflow-spec.md
├── gateflow-enhancements.md
├── resources/
│   └── fonts/
│       └── Hack-Regular.ttf      ← Bundled custom font
├── docs/
│   ├── CURRENT_STATE.md          ← this file
│   ├── gateflow-spec.md
│   └── LL_LI.md                  ← lessons learned / identified
├── src/
│   ├── CMakeLists.txt
│   ├── main.cpp                  ← Entry point (native + Emscripten)
│   ├── rendering/
│   │   ├── animation_state.hpp / animation_state.cpp
│   │   ├── app_font.hpp / app_font.cpp   ← Custom font management
│   │   ├── layout_engine.hpp / layout_engine.cpp
│   │   ├── gate_renderer.hpp / gate_renderer.cpp
│   │   └── wire_renderer.hpp / wire_renderer.cpp
│   ├── simulation/
│   │   ├── gate.hpp / gate.cpp
│   │   ├── wire.hpp / wire.cpp
│   │   ├── circuit.hpp / circuit.cpp
│   │   ├── circuit_builder.hpp / circuit_builder.cpp
│   │   └── nand_decompose.hpp / nand_decompose.cpp
│   ├── timing/
│   │   └── propagation_scheduler.hpp / propagation_scheduler.cpp
│   └── ui/
│       ├── input_panel.hpp / input_panel.cpp
│       ├── info_panel.hpp / info_panel.cpp
│       └── ui_scale.hpp / ui_scale.cpp   ← Responsive UI scaling
├── web/
│   └── shell.html                ← Custom Emscripten HTML shell (responsive)
├── .github/
│   └── workflows/
│       └── deploy.yml            ← GitHub Pages WASM deployment
└── tests/
    ├── CMakeLists.txt
    ├── test_gate.cpp
    ├── test_circuit.cpp
    ├── test_circuit_builder.cpp
    ├── test_nand_decompose.cpp
    └── test_propagation.cpp
```

## Quick Build & Test

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --target gateflow_tests -j$(nproc)
./build/tests/gateflow_tests
```
