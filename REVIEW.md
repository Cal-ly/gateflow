# Gateflow MVP Consolidation Review

Date: 2026-02-12

This review was prepared against the project intent in docs/gateflow-spec.md and .archive/CLAUDE_CODE_GUIDELINES.md, and a full pass over the current codebase.

---

## 1) Executive Summary

Gateflow is in a strong MVP state:
- Architecture layering is mostly clean and aligned with intent (simulation → timing → rendering → ui).
- Core simulation correctness is well-covered by unit tests.
- Native + WASM builds are functional.
- The app is already educationally useful and coherent.

The consolidation priority is not feature expansion first; it is hardening:
1. Tighten graph integrity and connectivity invariants in simulation.
2. Remove frame-time inefficiencies in rendering.
3. Improve layout/wire routing correctness for fan-out and non-RCA circuits.
4. Add focused tests for timing/rendering logic (non-visual assertions).
5. Trim WASM flags and clarify build-size/perf tradeoffs.

---

## 2) Alignment Check vs Spec/Guidelines

### What is already aligned well

- **Layer boundaries:** simulation and timing contain no Raylib includes.
- **Ownership model:** uses `std::unique_ptr` for owned gates/wires; non-owning raw pointers for graph links.
- **Topological propagation model:** implemented and tested.
- **NAND decomposition:** in-place transformation exists and is integration-tested.
- **MVP UI controls:** input, run/pause/step/reset, speed slider, NAND toggle present.
- **WASM path:** Emscripten branch in main loop exists and works.

### Gaps or drift against written intent

- **Clock waveform display** from the spec is not implemented yet (acceptable for MVP, but should be tracked as post-consolidation).
- **License mismatch in docs:** spec text says MIT expectation, repository currently ships AGPL-3.0 license text. Decide and align docs + legal files consistently.

---

## 3) Consolidation Improvements (Where + Why)

## A. Simulation Robustness and Invariants

### A1. Enforce graph consistency checks (high priority)
**Where:** src/simulation/circuit.cpp, src/simulation/circuit.hpp

**Issue:** `finalize()` validates acyclicity via topological size match, but does not verify full bidirectional consistency (gate inputs <-> wire destinations, wire source <-> gate output).

**Why it matters:** This project transforms graph structure in `decompose_to_nand()`. A debug-time consistency validator would catch subtle corruption early.

**Optimization:** Add a `validate_connectivity()` pass (debug-only or optional) called from `finalize()`.

---

### A2. Prevent accidental multi-driver wires (high priority)
**Where:** src/simulation/circuit.cpp (`connect`)

**Issue:** A wire source can be reassigned if `connect()` is called again with another non-null source.

**Why it matters:** Digital logic assumes single-driver wires in this simulator model. Silent reassignment can hide bugs.

**Optimization:** Add runtime guards in `connect()`:
- reject changing `wire->source` when already set to a different gate,
- reject setting more than one output wire for a gate unless explicitly intended.

---

### A3. Reduce per-gate allocation in propagation (medium priority)
**Where:** src/simulation/circuit.cpp (`propagate`)

**Issue:** `std::vector<bool> input_values` is constructed for each gate, every propagate call.

**Why it matters:** For small circuits it is fine, but NAND view and future larger circuits multiply this cost.

**Optimization options:**
- keep a reusable scratch buffer sized to max fan-in,
- or add a non-allocating evaluate path for 1- and 2-input gates (current dominant case).

---

## B. Timing Layer Correctness and API Quality

### B1. Clarify or remove `PlaybackMode::STEP` state (low/medium)
**Where:** src/timing/propagation_scheduler.hpp/.cpp

**Issue:** `STEP` exists in the enum but operationally stepping is mostly a request while paused.

**Why it matters:** Avoid ambiguous mode semantics for maintainability.

**Optimization:** Either:
- remove `STEP` mode and keep step as action, or
- fully formalize `STEP` behavior and transitions.

---

### B2. Clamp speed inside scheduler API (low)
**Where:** src/timing/propagation_scheduler.hpp

**Issue:** UI clamps speed, scheduler does not.

**Why it matters:** API should defend itself for non-UI callers/tests.

**Optimization:** clamp in `set_speed()` too.

---

## C. Rendering Performance and Fidelity

### C1. Remove O(G*W) output lookup in gate rendering (high priority)
**Where:** src/rendering/gate_renderer.cpp

**Issue:** For every gate, rendering scans all wires to find its output value.

**Why it matters:** This is avoidable per-frame overhead.

**Optimization:** Use `gate->get_output()` directly and read value from that wire.

---

### C2. Fan-out wire rendering is incomplete (high priority)
**Where:** src/rendering/layout_engine.cpp, src/rendering/wire_renderer.cpp

**Issue:** Layout stores one path per wire (`map<const Wire*, vector<Vec2>>`), but wires may have multiple destinations. Current comment acknowledges first-destination bias.

**Why it matters:** Logical correctness of visualization. Fan-out should be visibly complete.

**Optimization:** Change layout representation to support multiple routed branches per wire, for example:
- `map<const Wire*, vector<vector<Vec2>>>` (polyline per branch), or
- explicit trunk + branch segments.

---

### C3. Precompute polyline lengths for signal animation (medium)
**Where:** src/rendering/wire_renderer.cpp

**Issue:** `path_length()` and segment traversal are recomputed repeatedly each frame.

**Why it matters:** Simple caching can reduce per-frame math cost, especially in NAND mode.

**Optimization:** Store path metrics in `Layout` at compute time (total length + cumulative segment lengths).

---

### C4. Add deterministic non-RCA layout tests (medium)
**Where:** src/rendering/layout_engine.cpp + new tests

**Issue:** generic fallback layout exists but is untested.

**Why it matters:** Extensibility goal depends on layout behaving for future circuits.

**Optimization:** unit tests for layout invariants (non-overlap, stable ordering, complete wire routing coverage).

---

## D. UI/Interaction Hardening

### D1. Avoid exact float equality for slider updates (low)
**Where:** src/ui/input_panel.cpp

**Issue:** `if (new_val != value)` compares floats directly.

**Why it matters:** small jitter can trigger noisy updates.

**Optimization:** compare with epsilon.

---

### D2. Decouple panel vertical stacking (low)
**Where:** src/main.cpp + ui panels

**Issue:** info panel y-position is fixed (`330.0f`).

**Why it matters:** brittle if input panel height changes.

**Optimization:** have `draw_input_panel` return panel height, then place info panel dynamically.

---

## E. Build / Tooling / Delivery

### E1. Re-evaluate WASM flags for size/perf (medium)
**Where:** src/CMakeLists.txt

**Issue:** `-sASYNCIFY` and `-sFORCE_FILESYSTEM=1` are enabled by default.

**Why it matters:** both can increase output size/startup cost; current app does not appear to need filesystem or async stack rewrites.

**Optimization:** benchmark with/without flags and keep only needed ones.

---

### E2. Add sanitizers for Debug native builds (medium)
**Where:** top-level CMakeLists.txt

**Issue:** no sanitizer profile currently configured.

**Why it matters:** graph transformations and pointer-heavy structures benefit from ASan/UBSan in consolidation phase.

**Optimization:** optional `-DGATEFLOW_ENABLE_SANITIZERS=ON` path for Debug.

---

## F. Tests and Quality Gates

### F1. Add timing-layer unit tests (high priority)
**Where:** tests/

**Issue:** scheduler behavior is not directly tested.

**Why it matters:** timing behavior drives pedagogy and UX state.

**Test targets:**
- depth computation on known DAGs,
- step progression correctness,
- `is_gate_resolved` and `wire_signal_progress` boundaries.

---

### F2. Add rendering-logic tests (medium)
**Where:** tests/

**Issue:** rendering is mostly visual/manual verification.

**Why it matters:** consolidation should protect against regressions.

**Test targets:**
- layout deterministic output for fixed circuits,
- complete route coverage for all wire destinations,
- stable bounding box assertions.

---

### F3. Add one end-to-end model test for UI-driven rebuild/reset behavior (medium)
**Where:** tests/ (non-Raylib logic harness)

**Issue:** state transitions in `main.cpp` are robust but untested.

**Why it matters:** prevents regressions in mode/scheduler transitions during future changes.

---

## 4) Recommended Consolidation Plan (Practical Sequence)

### Pass 1 (Safety + correctness)
1. Add graph invariant validation in simulation.
2. Add connect() guards for single-driver semantics.
3. Add scheduler tests.

### Pass 2 (Performance + fidelity)
4. Fix gate rendering O(G*W) lookup.
5. Implement multi-destination wire routing/rendering.
6. Cache wire path metrics.

### Pass 3 (Tooling + polish)
7. Add sanitizer option in CMake.
8. Reassess Emscripten flags and document chosen rationale.
9. Remove brittle hardcoded panel stacking.

---

## 5) Future Ideas (Aligned with Spec and Guidelines)

These ideas stay inside the architecture and extension model defined in the spec.

1. **Clock waveform module completion**
   - Implement `ClockSignal` + top waveform panel as an educational timing aid.

2. **Circuit metadata for layout specialization**
   - Tag gates with logical role (`bit_index`, `adder_stage_role`) at builder time.
   - Keeps layout robust without depending on insertion order.

3. **Alternative adder comparison mode**
   - Add carry-lookahead builder and side-by-side visualization against ripple-carry.
   - Excellent pedagogical extension using same simulation/rendering abstractions.

4. **Subtractor builder (two’s complement)**
   - `build_subtractor(int bits)` using existing primitives and carry-in.
   - Minimal architecture churn; high educational value.

5. **Structured simulation trace export (in-memory)**
   - Create per-depth signal snapshots that info panel and future analytics can consume.
   - Enables richer narration (“what changed this depth?”) without coupling layers.

6. **Property-based testing for builders/decomposition**
   - Randomized DAG and input generation for equivalence checks.
   - Useful long-term guardrail as new builders arrive.

---

## 6) Closing

The MVP foundation is solid. Consolidation should focus on invariant enforcement, rendering correctness/performance, and targeted tests for timing/layout behavior. After this hardening pass, the project is well-positioned for spec-aligned educational extensions (clock display, subtractor, architecture comparisons) without violating current layering discipline.
