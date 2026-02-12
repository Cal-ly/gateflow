# Gateflow

**Interactive visual simulator showing how a CPU performs integer addition at the logic-gate level.**

Enter two numbers (0–99), press **Run**, and watch signals propagate through a 7-bit ripple-carry adder in real time. See exactly how XOR, AND, OR, and NAND gates work together to produce a result — then toggle **NAND View** to see every gate decomposed into universal NAND gates.

> **[Try it live →](https://cally.github.io/gateflow)**  *(GitHub Pages — runs in any modern browser)*

---

## Features

- 🔢 **Two numeric inputs** (0–99) with instant circuit re-propagation
- ⚡ **Animated signal flow** — signals travel gate-by-gate through the carry chain
- 🔀 **NAND decomposition toggle** — see the same adder built entirely from NAND gates
- ⏯️ **Playback controls** — Run, Pause, Step, Reset, adjustable speed slider
- 📊 **Binary readouts** — live bit-by-bit resolution of inputs and sum with pending/resolved coloring
- ⌨️ **Keyboard shortcuts** — Space (pause/play), → (step), R (reset)
- 🌐 **Runs in browser** — compiled to WebAssembly via Emscripten, ~200 KB WASM

---

## Architecture

```
src/
├── simulation/    # Layer 1: Pure logic (Gate, Wire, Circuit, builders, NAND decomp)
├── timing/        # Layer 2: PropagationScheduler — depth-based timing, no rendering
├── rendering/     # Layer 3: Layout engine, gate/wire renderers, animation state
├── ui/            # Layer 4: Input panel, info panel — custom-drawn with Raylib
└── main.cpp       # Entry point — wires all layers together, Emscripten support
```

**Strict layering:** each layer only depends on layers below it. The simulation layer has zero rendering dependencies and is fully unit-tested.

| Layer | Library | Dependencies |
|-------|---------|-------------|
| Simulation | `gateflow_simulation` | None (pure C++17) |
| Timing | `gateflow_timing` | simulation |
| Rendering | `gateflow_rendering` | timing + Raylib |
| UI | `gateflow_ui` | rendering + Raylib |

---

## Building

### Prerequisites

- **C++17** compiler (GCC 12+, Clang 15+)
- **CMake** 3.20+
- **Raylib 5.0** and **Catch2 v3.5** are fetched automatically via CMake FetchContent

### Native (Linux)

```bash
cmake -B build -S . -DCMAKE_BUILD_TYPE=Release
cmake --build build

# Run the app
./build/src/gateflow

# Run tests (37 tests, 309 assertions)
./build/tests/gateflow_tests
```

### WebAssembly

Requires the [Emscripten SDK](https://emscripten.org/docs/getting_started/downloads.html):

```bash
source /path/to/emsdk/emsdk_env.sh
emcmake cmake -B build-web -S . -DCMAKE_BUILD_TYPE=Release -DPLATFORM=Web
cmake --build build-web

# Serve locally (WASM requires HTTP, not file://)
cd build-web/src
python3 -m http.server 8080
# Open http://localhost:8080/index.html
```

Output files in `build-web/src/`: `index.html`, `index.js`, `index.wasm` (~200 KB).

---

## Usage

| Control | Action |
|---------|--------|
| **A / B fields** | Type a number 0–99, press Enter or click away |
| **Run** | Start propagation from the beginning |
| **Pause** | Pause / resume animation |
| **Step** | Advance one gate-depth level |
| **Reset** | Restart propagation |
| **Speed slider** | 0.5× to 20× propagation speed |
| **NAND toggle** | Rebuild circuit using only NAND gates |
| **Space** | Toggle pause/play |
| **→ (Right arrow)** | Step one depth |
| **R** | Reset and replay |

---

## How It Works

1. **Circuit construction** — `build_ripple_carry_adder(7)` creates 7 full/half adders wired in a carry chain. Each bit position gets XOR + AND gates for sum and carry.

2. **Topological sort** — Kahn's algorithm orders all gates so each gate's inputs are computed before it evaluates. This is the evaluation order.

3. **Propagation** — All gates evaluate in topological order, producing the final output instantly (used to determine the correct result).

4. **Depth scheduling** — Each gate is assigned a depth (longest path from any input). The `PropagationScheduler` reveals gates level-by-level over time, creating the visual effect of signals "flowing" through the circuit.

5. **NAND decomposition** — `decompose_to_nand()` replaces every AND/OR/XOR/NOT gate with equivalent NAND-only subcircuits in-place, preserving all wire connections.

---

## Project Structure

```
gateflow/
├── CMakeLists.txt              # Top-level build (FetchContent for Raylib + Catch2)
├── src/
│   ├── CMakeLists.txt          # Library and executable targets
│   ├── main.cpp                # Entry point (native + Emscripten)
│   ├── simulation/             # Gate, Wire, Circuit, builders, NAND decompose
│   ├── timing/                 # PropagationScheduler
│   ├── rendering/              # Layout, gate/wire renderers, animation state
│   └── ui/                     # Input panel, info panel
├── tests/                      # Catch2 unit tests (37 tests, 309 assertions)
├── web/
│   └── shell.html              # Custom Emscripten HTML shell
└── docs/
    ├── CURRENT_STATE.md        # Detailed project state
    └── LL_LI.md                # Lessons learned / identified
```

---

## Tech Stack

- **C++17** — modern features (string_view, unique_ptr, structured bindings, constexpr)
- **Raylib 5.0** — rendering, input, window management
- **Catch2 v3.5** — unit testing framework
- **CMake** — build system with FetchContent dependency management
- **Emscripten** — C++ → WebAssembly compilation
- **No external UI libraries** — all UI drawn with Raylib primitives

---

## License

[MIT](LICENSE)
