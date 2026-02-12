# Gateflow — Project Specification

## Overview

**Gateflow** is an interactive visual simulator that shows how a CPU performs integer addition at the logic-gate level. Users input two numbers (0–99) and watch signals propagate through logic gates in real-time, seeing exactly how NAND, XOR, AND, and OR gates activate to produce a result.

The project serves a dual purpose: an educational tool that demystifies digital logic, and a well-engineered C++ portfolio piece demonstrating clean architecture, modern practices, and WASM deployment.

**Repository:** `gateflow`
**Language:** C++17 (minimum), C++20 preferred where supported by Emscripten
**Framework:** Raylib 5.x
**Build System:** CMake
**Target Platforms:** Linux native (primary dev), WebAssembly (primary distribution)

---

## Architecture

The project follows a strict three-layer architecture. Each layer is a separate directory with no upward dependencies.

```
src/
├── simulation/    # Layer 1: Pure logic — no rendering, no I/O
├── timing/        # Layer 2: Clock and propagation — depends on simulation
├── rendering/     # Layer 3: Visualization — depends on simulation + timing
├── ui/            # Input handling, panels, controls — depends on rendering
└── main.cpp       # Entry point, wires layers together
```

### Layer 1: Simulation (`src/simulation/`)

This is the core domain model. It must be fully testable without any graphics context.

#### Gate Model

Every logic gate is represented as a node in a directed acyclic graph (DAG):

```
Gate:
  - id: unique identifier
  - type: enum { NAND, AND, OR, XOR, NOT, BUFFER }
  - inputs: vector of Wire* (typically 2, 1 for NOT/BUFFER)
  - output: Wire*
  - state: bool (current output value)
  - dirty: bool (inputs changed, needs re-evaluation)
```

#### Wire Model

Wires connect gate outputs to gate inputs:

```
Wire:
  - id: unique identifier
  - source: Gate* (the gate driving this wire, nullptr for primary inputs)
  - destinations: vector of Gate*> (gates receiving this signal)
  - value: bool
  - previous_value: bool (for edge detection / animation)
```

#### Circuit Model

A `Circuit` owns a collection of gates and wires, forming a complete functional unit:

```
Circuit:
  - gates: vector<unique_ptr<Gate>>
  - wires: vector<unique_ptr<Wire>>
  - input_wires: vector<Wire*> (external inputs, ordered)
  - output_wires: vector<Wire*> (external outputs, ordered)
  - propagation_queue: queue for ordered evaluation
```

**Key method: `Circuit::propagate()`**
- Uses topological order (computed once at circuit construction, stored)
- Evaluates each gate in order, updating wire values
- Returns a `PropagationResult` containing which gates/wires changed state
- This is the core data that the rendering layer consumes

#### Circuit Builder

Factory functions that construct specific circuits from primitives:

- `build_half_adder()` → Circuit with 2 inputs (A, B), 2 outputs (Sum, Carry)
- `build_full_adder()` → Circuit with 3 inputs (A, B, Cin), 2 outputs (Sum, Cout)
- `build_ripple_carry_adder(int bits)` → Circuit chaining N full adders
- Future: `build_subtractor(int bits)`, `build_multiplier(int bits)`

Each builder composes smaller circuits. A full adder is built from two half adders + an OR gate. A ripple-carry adder chains full adders by connecting Cout[i] to Cin[i+1].

**Extensibility contract:** New operations are added by writing new builder functions. The simulation layer, timing layer, and rendering layer require zero modifications — they work on the generic Circuit/Gate/Wire abstractions.

#### NAND Decomposition

Every gate type can optionally be decomposed into NAND-only equivalents:

- NOT = NAND(A, A)
- AND = NOT(NAND(A, B)) = NAND(NAND(A,B), NAND(A,B))
- OR = NAND(NOT(A), NOT(B))
- XOR = NAND(NAND(A, NAND(A,B)), NAND(B, NAND(A,B)))

Provide a function `decompose_to_nand(Circuit&)` that replaces all non-NAND gates with their NAND equivalents, preserving connectivity. This enables a toggle between "logical view" and "NAND-only view."

### Layer 2: Timing (`src/timing/`)

Manages the temporal dimension of signal propagation.

#### Propagation Scheduler

In a real ripple-carry adder, signals don't all resolve simultaneously. The carry must propagate through each full adder sequentially. This layer models that.

```
PropagationScheduler:
  - circuit: Circuit* (the circuit being simulated)
  - gate_depth: map<Gate*, int> (topological depth from inputs)
  - current_depth: int (how far propagation has reached)
  - max_depth: int (deepest gate in the circuit)
  - speed: float (depths per second, user-adjustable)
  - mode: enum { REALTIME, PAUSED, STEP }
```

**Tick behavior:**
- Each frame, advance `current_depth` based on elapsed time and speed
- Gates at depth <= current_depth are "resolved" (their true output is visible)
- Gates at depth > current_depth show their previous state (signal hasn't reached them yet)
- When current_depth >= max_depth, propagation is complete

This creates the visual effect of signals "flowing" left-to-right through the adder, with the carry chain being the critical (longest) path.

#### Clock Signal

Model a simple clock signal for educational purposes:

```
ClockSignal:
  - frequency: float (Hz, user-adjustable)
  - phase: float (0.0 to 1.0 within current cycle)
  - high: bool (current level)
```

The clock signal is displayed as a waveform at the top of the visualization. A full addition happens within one clock cycle. The clock exists to show timing relationships, not to drive the simulation mechanically.

### Layer 3: Rendering (`src/rendering/`)

All drawing code lives here. Uses Raylib's 2D drawing primitives.

#### Layout Engine

Computes screen positions for all visual elements. The layout must be deterministic — given a circuit, the positions are always the same.

```
LayoutEngine:
  - compute_layout(Circuit&) -> Layout
  - Layout contains:
    - gate_positions: map<Gate*, Rectangle>
    - wire_paths: map<Wire*, vector<Vector2>> (polyline waypoints)
    - input_positions: vector<Vector2>
    - output_positions: vector<Vector2>
    - bounding_box: Rectangle
```

**Layout strategy for a ripple-carry adder:**
- Full adders arranged left-to-right, bit 0 on the right (LSB)
- Within each full adder, gates arranged top-to-bottom
- Carry wire flows left across the top connecting adjacent full adders
- Input bits enter from the top, output bits exit at the bottom
- Generous spacing — readability over compactness

For extensibility, the layout engine should work generically on any Circuit. Specific layouts for known circuit types (adder, subtractor) can be provided as specializations.

#### Gate Renderer

Draws individual gates with visual state:

- **Inactive (0):** Gray fill, muted outline
- **Active (1):** Bright green fill, highlighted outline
- **Pending (not yet resolved by propagation):** Dim/translucent, subtle pulse animation
- Gate type label shown inside or above (AND, XOR, NAND, etc.)
- Optional: truth table tooltip on hover — highlight the active row

#### Wire Renderer

Draws wires connecting gates:

- **Carrying 0:** Thin, dark gray
- **Carrying 1:** Thicker, bright green (or bright color matching the "active" theme)
- **Signal transition animation:** Brief pulse/glow effect traveling along the wire when value changes
- Wire corners should use clean right-angle routing (Manhattan routing), not diagonal lines

#### Animation System

```
AnimationState:
  - gate_animations: map<Gate*, GateAnim>
  - wire_animations: map<Wire*, WireAnim>

GateAnim:
  - activation_time: float (when this gate last changed state)
  - current_alpha: float (for fade-in/fade-out)
  - pulse_phase: float (for pending state pulse)

WireAnim:
  - signal_position: float (0.0–1.0, how far the signal pulse has traveled along the wire)
  - active: bool
```

Animation updates happen every frame based on delta time. The propagation scheduler's current_depth drives which gates/wires are animating.

### UI Layer (`src/ui/`)

#### Input Panel

- Two numeric inputs (0–99) for operands A and B
- "Run" button to start propagation
- Speed slider (controls PropagationScheduler::speed)
- Pause/Step/Reset controls
- Toggle: "Logical view" vs "NAND-only view"

#### Information Panel

- Binary representation of inputs and output, updating as bits resolve
- Current clock cycle phase
- Decimal result (appears when propagation completes)
- Brief explanatory text for what's currently happening ("Carry propagating from bit 2 to bit 3...")

#### Clock Waveform Display

- Horizontal waveform strip along the top
- Shows clock high/low transitions
- Vertical marker showing current position within the cycle
- Annotation showing which phase of the addition cycle we're in

---

## Build System

### CMake Structure

```
CMakeLists.txt              # Top-level: project config, platform detection
├── src/CMakeLists.txt      # Main executable
├── libs/CMakeLists.txt     # Third-party dependencies (raylib via FetchContent)
└── tests/CMakeLists.txt    # Test executable
```

### Dependencies

- **Raylib 5.x:** Fetched via CMake FetchContent. Do NOT vendor or submodule.
- **Catch2 v3 (or doctest):** For unit tests. Also via FetchContent.
- No other external dependencies.

### Build Targets

```bash
# Native Linux build
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build

# WASM build (requires Emscripten SDK)
emcmake cmake -B build-web -DPLATFORM=Web
cmake --build build-web
```

### Emscripten / WASM Considerations

- Raylib has first-class Emscripten support. Use `PLATFORM=Web` flag.
- The main loop must use `emscripten_set_main_loop()` instead of a while loop. Abstract this behind a platform macro or compile-time branch in main.cpp.
- File I/O is not available in WASM — all data must be in-memory (this project has no file I/O needs).
- Emscripten generates a `.html`, `.js`, and `.wasm` file. The HTML shell can be customized for styling.
- Target a canvas size of 1280x720 as default, with responsive scaling.

---

## File and Directory Structure

```
gateflow/
├── CMakeLists.txt
├── README.md
├── LICENSE                     # MIT
├── .gitignore
├── .clang-format               # Consistent formatting
├── src/
│   ├── CMakeLists.txt
│   ├── main.cpp
│   ├── simulation/
│   │   ├── gate.hpp / gate.cpp
│   │   ├── wire.hpp / wire.cpp
│   │   ├── circuit.hpp / circuit.cpp
│   │   ├── circuit_builder.hpp / circuit_builder.cpp
│   │   └── nand_decompose.hpp / nand_decompose.cpp
│   ├── timing/
│   │   ├── propagation_scheduler.hpp / propagation_scheduler.cpp
│   │   └── clock_signal.hpp / clock_signal.cpp
│   ├── rendering/
│   │   ├── layout_engine.hpp / layout_engine.cpp
│   │   ├── gate_renderer.hpp / gate_renderer.cpp
│   │   ├── wire_renderer.hpp / wire_renderer.cpp
│   │   └── animation.hpp / animation.cpp
│   └── ui/
│       ├── input_panel.hpp / input_panel.cpp
│       ├── info_panel.hpp / info_panel.cpp
│       └── clock_display.hpp / clock_display.cpp
├── tests/
│   ├── CMakeLists.txt
│   ├── test_gate.cpp
│   ├── test_circuit.cpp
│   ├── test_circuit_builder.cpp
│   ├── test_nand_decompose.cpp
│   └── test_propagation.cpp
├── web/
│   └── shell.html              # Custom Emscripten HTML shell
└── docs/
    └── architecture.md         # High-level architecture overview
```

---

## Coding Standards

### Style

- Use `.clang-format` with a consistent style (recommend: BasedOnStyle: LLVM, IndentWidth: 4)
- Header guards: `#pragma once`
- Naming: `PascalCase` for types, `snake_case` for functions and variables, `UPPER_SNAKE` for constants and macros
- Prefer `enum class` over plain `enum`
- Use `std::unique_ptr` for ownership, raw pointers for non-owning references
- No `using namespace std;` in headers, acceptable in `.cpp` files

### Modern C++ Practices

- Use `constexpr` for compile-time computable values
- Use `std::optional` for values that may not exist
- Use `std::string_view` for non-owning string parameters
- Prefer range-based for loops
- Use structured bindings where they improve readability
- Use `[[nodiscard]]` on functions whose return values should not be ignored (e.g., `propagate()`)
- Mark single-argument constructors `explicit`

### Documentation

- Every public class and function gets a Doxygen-style comment (`///` or `/** */`)
- Focus comments on **why**, not **what** — the code should be readable on its own
- README.md should include: project description, screenshots/gif, build instructions for both native and WASM, usage instructions, architecture overview
- Keep `docs/architecture.md` in sync with the actual implementation

### Testing

- Unit test the simulation layer thoroughly — this is pure logic with no dependencies
- Test each gate type in isolation
- Test half adder and full adder truth tables exhaustively
- Test the full ripple-carry adder with boundary values: 0+0, 0+99, 99+0, 50+49, 99+99 (overflow case — output is 198, fits in 8 bits)
- Test NAND decomposition produces identical outputs for all input combinations
- Test topological sort ordering
- The timing and rendering layers are harder to unit test — focus integration testing there

---

## Implementation Phases

### Phase 1: Simulation Foundation

**Goal:** Build and fully test the simulation layer with no graphics.

Tasks:
1. Set up the CMake project structure with Raylib via FetchContent and Catch2 for tests
2. Implement Gate, Wire, and Circuit classes
3. Implement topological sort for evaluation order
4. Build circuit_builder: half_adder, full_adder, ripple_carry_adder(7)
5. Implement NAND decomposition
6. Write comprehensive unit tests for all of the above
7. Verify: all tests pass, `ripple_carry_adder(7)` correctly computes A+B for all valid inputs

**Success criteria:** Running the test suite validates that the simulation produces correct results for all gate types, all builder functions, and NAND decomposition equivalence.

### Phase 2: Layout and Static Rendering

**Goal:** Display the circuit on screen with correct positions but no animation.

Tasks:
1. Implement the layout engine for ripple-carry adder
2. Implement gate renderer (draw gates as labeled rectangles with state coloring)
3. Implement wire renderer (Manhattan routing with state coloring)
4. Hard-code two input values, run full propagation, render the final state
5. Verify visually that the layout is readable and correct

**Success criteria:** Opening the application shows a correctly laid-out 7-bit ripple-carry adder with all gates colored according to their final computed state for the hard-coded inputs.

### Phase 3: Animation and Timing

**Goal:** Add real-time propagation visualization.

Tasks:
1. Implement PropagationScheduler with depth-based timing
2. Implement animation state tracking (fade-in, pulse, signal travel)
3. Wire up the scheduler to drive rendering state per-frame
4. Add the clock waveform display
5. Implement speed control (hardcoded for now)

**Success criteria:** When the application starts, signals visibly propagate through the circuit from inputs to outputs, with the carry chain being the longest-delay path. Gates light up in correct causal order.

### Phase 4: UI and Interactivity

**Goal:** Add user controls for a complete interactive experience.

Tasks:
1. Implement input panel (two number inputs, 0–99)
2. Implement run/pause/step/reset controls
3. Implement speed slider
4. Implement logical/NAND view toggle
5. Implement info panel with binary/decimal readouts
6. Add explanatory text that updates based on propagation state

**Success criteria:** A user can input two numbers, press "Run," and watch the complete addition process with full control over playback. Toggling NAND view restructures the visualization correctly.

### Phase 5: WASM Build and Polish

**Goal:** Deployable web version and final polish.

Tasks:
1. Set up Emscripten build configuration in CMake
2. Create custom HTML shell with styling
3. Handle main loop differences (emscripten_set_main_loop)
4. Test in multiple browsers (Chrome, Firefox, Safari)
5. Add responsive canvas scaling
6. Performance optimize if needed (WASM often needs draw call reduction)
7. Write README with build instructions, screenshots, and architecture overview
8. Set up GitHub Pages deployment for the WASM build

**Success criteria:** The project is accessible via a URL, loads quickly, and runs smoothly in major browsers. The GitHub repository has a polished README with visuals.

---

## Design Decisions and Rationale

### Why a ripple-carry adder (not carry-lookahead)?
The ripple-carry adder is pedagogically ideal because the carry propagation delay is *visible* — users can see why it's slow and intuitively understand the motivation for faster adder designs. A carry-lookahead adder would be a great Tier 3 addition for comparison.

### Why 7 bits (not 8)?
0–99 requires 7 bits (max value 99 = 1100011). Using exactly the bits needed avoids confusion. However, the output may need 8 bits (99+99=198 = 11000110), so the adder should actually be 7-bit inputs with an 8-bit output (the 8th bit comes from the final carry-out).

### Why topological sort instead of event-driven simulation?
For circuits of this size (a 7-bit adder has ~35 gates in logical view, ~100+ in NAND view), topological evaluation is simpler, faster, and easier to debug than an event queue. Event-driven simulation is only necessary for circuits with feedback (flip-flops, latches), which this project doesn't have.

### Why Raylib over SDL2?
Raylib provides a higher-level API (DrawRectangle, DrawLine, DrawText vs. SDL's renderer abstraction), has excellent Emscripten support out of the box, and produces smaller WASM binaries. SDL2 offers more control but adds complexity without corresponding benefit for this project's needs.

### Why not Dear ImGui?
ImGui could be added later for the control panels, but for the initial implementation, Raylib's built-in GUI functions (raygui) or simple custom-drawn UI elements are sufficient. ImGui adds a dependency and build complexity that isn't justified until the UI becomes significantly more complex.

---

## Extensibility Notes

The architecture explicitly supports future operations without modifying existing code:

- **Subtraction:** New builder function `build_subtractor(int bits)`. Reuses the ripple-carry adder with a NOT gate on each B-input bit and carry-in set to 1 (two's complement). The layout engine handles it generically.

- **Multiplication:** New builder function `build_array_multiplier(int bits)`. Composes multiple adders with AND gates for partial products. Requires the timing layer to handle multi-cycle operations (shift-and-add), which means extending PropagationScheduler to support sequential phases. The rendering layer needs no changes — it just draws more gates.

- **Division:** New builder function `build_restoring_divider(int bits)`. The most complex, involving subtractors, comparators, and a control FSM. Would require significant timing layer extensions.

- **Carry-lookahead adder:** New builder function for comparison with ripple-carry. Same interface, different internal structure. Could show both side-by-side to visualize the speed difference.

Each extension is a new circuit builder + potentially new layout specialization. The simulation core, timing engine, and rendering pipeline remain unchanged.
