# Gateflow — Lessons Learned & Lessons Identified

> **Lessons Learned (LL):** Knowledge confirmed through experience — we encountered the issue and resolved it.
> **Lessons Identified (LI):** Knowledge noted for future awareness — risks or patterns we observed that should inform later phases.

---

## Phase 1: Simulation Foundation

### Lessons Learned

**LL-001 — CMake 4.x rejects old `cmake_minimum_required` in dependencies**
When using CMake 4.x (we have 4.2.3), third-party projects fetched via `FetchContent` that declare `cmake_minimum_required(VERSION 2.x)` (like Raylib 5.0) will fail to configure. The fix is to set `CMAKE_POLICY_VERSION_MINIMUM` to 3.5 in the top-level CMakeLists.txt before fetching dependencies. This is a compatibility shim that will eventually become unnecessary as upstream libraries update.

**LL-002 — `sed` global substitution inside helper functions causes infinite recursion**
When using `sed -i 's/old/new/g'` to refactor function calls, the replacement is applied everywhere — including inside the newly written helper function itself. In our case, `disconnect_inputs()` was meant to call `gate->clear_inputs()` internally, but `sed` replaced that call with a recursive `disconnect_inputs(gate)`, causing a stack overflow (SIGSEGV). **Mitigation:** Never use blind global find-replace on code. Apply refactoring manually or use targeted replacements with sufficient context to avoid self-referential substitutions.

**LL-003 — Wire destination lists must be kept in sync when decomposing gates**
The NAND decomposition initially failed with "circuit contains a cycle" because:
1. `gate->clear_inputs()` only clears the gate's own input vector — it does **not** remove the gate from each input wire's `destinations_` list.
2. New gates created during decomposition had input wires set via `gate->add_input(wire)` but the wire's `destinations_` was never updated with `wire->add_destination(gate)`.

This left Kahn's algorithm unable to propagate through the new gates (they were never in any wire's destination list, so their in-degree was never decremented). **Resolution:** Introduced `disconnect_inputs()` to properly clean both sides, and `link_input()`/`link_output()` helpers that always update both the gate and wire together.

**LL-004 — `[[nodiscard]]` on `propagate()` creates noise in test code**
Marking `Circuit::propagate()` as `[[nodiscard]]` is correct for production code (callers should usually examine the `PropagationResult`). However, in tests that only care about final output values, every `propagate()` call generates a warning. **Resolution:** Use explicit `(void)circuit.propagate();` casts in tests where the result is intentionally discarded. This is idiomatic C++ and preserves the `[[nodiscard]]` safety for production callers.

**LL-005 — Multi-line comment warning from ASCII art with trailing backslash**
C++ treats a backslash `\` at the end of a line comment as a line continuation, even inside `//` comments. An ASCII art diagram using `\` for branching (`//   /     \`) triggers `-Wcomment`. **Resolution:** Replace `\` with `|` or other characters in comment-based diagrams.

### Lessons Identified

**LI-001 — Dual-sided graph updates are an ongoing risk**
The Gate/Wire graph is a bidirectional structure where gates reference wires and wires reference gates. Any operation that modifies connectivity (decomposition, future circuit composition, potential gate removal) must update **both sides** of every link. Consider whether a centralised `connect()`/`disconnect()` API on `Circuit` should be the only way to modify connectivity, making it impossible to create one-sided links.

**LI-002 — `const auto& inputs = gate->get_inputs()` is a dangling-reference hazard**
During NAND decomposition, we capture `const auto& inputs` as a reference to the gate's internal vector, then later call `disconnect_inputs(gate)` which clears that vector. The reference becomes dangling. This worked only because we copied the Wire* pointers into local variables **before** the disconnect call. This pattern is fragile. In future modifications, prefer copying the inputs vector up front: `auto inputs = gate->get_inputs();` (value copy).

**LI-003 — FetchContent dependency versions should be pinned and tested early**
Raylib 5.0 works with the CMake policy shim today, but upgrading Raylib or CMake could break the build. When Phase 5 (WASM) arrives, Emscripten bundles its own CMake toolchain which may behave differently. Pin exact Git tags and test the WASM build configuration early to catch incompatibilities before the rendering layer is built on top.

**LI-004 — Topological sort doesn't validate wire consistency**
The current `finalize()` only checks that the topological sort covers all gates (no cycle). It does **not** verify that every gate's input wires have the gate in their destination list, or that every wire's source gate actually has that wire as output. A consistency-check function (debug-only) would catch decomposition bugs much earlier than the symptom of "wrong output for 99+99."

**LI-005 — Test coverage for NAND decomposition of compound circuits is critical**
The individual gate decomposition tests all passed while the 7-bit ripple-carry adder decomposition silently produced wrong results. The bug only surfaced in compound circuits where internal wires have fan-out to multiple gates. Always include at least one multi-gate integration test when validating transformations.

---

## Phase 2: Layout and Static Rendering

### Lessons Learned

**LL-006 — clang-format destroys CMakeLists.txt if passed as input**
clang-format treats `.txt` files as C/C++ and aggressively reformats them, inserting spaces in paths (`simulation/gate.cpp` → `simulation / gate.cpp`), adding indentation, and breaking CMake syntax completely. The build system becomes non-functional. **Resolution:** Never pass CMake files to clang-format. Only pass `*.hpp` and `*.cpp` files. Consider a shell wrapper: `find src tests -name '*.cpp' -o -name '*.hpp' | xargs clang-format -i`.

**LL-007 — Raylib 5.0 `DrawRectangleRoundedLines` takes 5 parameters including lineThick**
The function signature is `DrawRectangleRoundedLines(Rectangle, float roundness, int segments, float lineThick, Color)`. A common mistake is omitting `lineThick` (thinking it matches `DrawRectangleRounded` which takes 4 params). The compiler error message ("cannot convert Color to float") is cryptic — it's the Color argument landing in the `lineThick` float slot.

**LL-008 — `gate_type_name()` returns `string_view`, not `const char*`**
The `gate_type_name()` constexpr function returns `std::string_view`, but Raylib text functions (`DrawText`, `MeasureText`) require `const char*`. Since the underlying string_views point to string literals, calling `.data()` is safe. Store the string_view first, then extract `.data()` — don't pass `.data()` inline without keeping the view alive.

**LL-009 — `build_ripple_carry_adder()` returns `unique_ptr<Circuit>`, not `Circuit`**
All circuit builder factory functions return `std::unique_ptr<Circuit>`. Attempting to assign to a plain `Circuit` fails with a cryptic "conversion from unique_ptr to non-scalar type" error. Use `auto circuit_ptr = build_ripple_carry_adder(7); auto& circuit = *circuit_ptr;` for ergonomic access.

### Lessons Identified

**LI-006 — Layout depends on gate creation order matching builder structure**
The current RCA layout assumes gates are created in a fixed order by the builder: bit 0 gets gates[0..1], bit 1 gets gates[2..6], etc. If the builder changes its creation order, the layout will silently misplace gates. Consider either: (a) tagging gates with metadata (bit index, role) during construction, or (b) inferring structure from the circuit graph topology rather than relying on creation order.

**LI-007 — Wire routing lacks channel allocation for complex circuits**
The current Manhattan routing uses midpoint Y-coordinates for horizontal segments. With many wires, multiple paths will overlap on the same horizontal channels. Phase 3 or 4 should implement proper wire channel allocation — assigning unique horizontal tracks in the space between gate rows to prevent visual overlap.

**LI-008 — No automated visual regression tests exist**
Phase 2 rendering can only be verified by visually inspecting the window. Any future changes to layout constants, wire routing, or gate drawing could break the visual output without any test catching it. Consider: (a) screenshot comparison tests, (b) Raylib's `TakeScreenshot()` function, or (c) at minimum, unit tests on the layout engine verifying expected gate positions and wire paths.

---

## Phase 3: Animation and Timing

### Lessons Learned

*No new major issues encountered — the timing and animation layers were pure C++ with no Raylib API surprises.*

### Lessons Identified

**LI-009 — PropagationScheduler is tightly coupled to Circuit pointer lifetime**
`PropagationScheduler` stores a raw `const Circuit*` passed at construction. If the circuit is rebuilt (e.g., NAND toggle changes the unique_ptr), the scheduler must also be recreated. This is handled correctly in Phase 4's `rebuild_app_state()`, but future refactors should be careful not to hold stale scheduler pointers after circuit reconstruction.

---

## Phase 4: UI and Interactivity

### Lessons Learned

**LL-010 — Custom Raylib UI requires careful input state management**
Drawing custom text fields with keyboard input requires tracking editing state (which field is active), cursor position, and ensuring keyboard shortcuts (SPACE, R, etc.) don't fire while a text field is focused. The solution is to gate keyboard shortcut handling on `!ui.editing_a && !ui.editing_b`.

**LL-011 — Circuit rebuild vs. re-propagation must be distinguished**
When inputs change (A or B values), the circuit structure is the same — only wire values change. A full rebuild is wasteful. But when the NAND toggle flips, the entire circuit structure changes (different gates, different wires, different layout). The main loop must distinguish these two cases: `reset_propagation()` for input changes, `rebuild_app_state()` for structural changes.

### Lessons Identified

**LI-010 — UIState mutable reference pattern creates tight coupling**
`draw_input_panel()` takes a mutable `UIState&` and both reads and writes to it (updating editing state, cursor positions, slider values). This makes the function impure — it has side effects through the reference. A cleaner pattern would be to return a new UIState alongside the UIAction, but the current approach is pragmatic for a single-threaded Raylib app.

**LI-011 — Panel layout uses hardcoded Y-offsets**
The info panel starts at Y=330, which assumes the input panel is exactly 320px tall. If the input panel grows (e.g., adding more controls), the panels will overlap. A layout system that measures actual panel heights would be more robust.

---

## Phase 5: WASM Build and Polish

### Lessons Learned

**LL-012 — Raylib 5.0 requires explicit `-DPLATFORM=Web` for Emscripten builds**
Even though Raylib's `BuildOptions.cmake` detects `EMSCRIPTEN` and sets `PLATFORM=Web` internally, GLFW's CMake tries to find X11 *before* that detection runs — causing `Could NOT find X11 (missing: X11_X11_LIB)` on cross-compile. The fix is to pass `-DPLATFORM=Web` explicitly on the `emcmake cmake` command line so Raylib skips the X11 code path entirely.

**LL-013 — Global `add_compile_options()` bleeds into Emscripten third-party code**
Using `add_compile_options(-Wall -Wextra -Wpedantic)` globally applies these flags to *all* targets — including FetchContent'd Raylib and Catch2, generating hundreds of warnings from upstream code. For Emscripten builds we removed the global call and instead applied warnings per-target via `target_compile_options(... PRIVATE)`. For native builds, the global approach is acceptable since Raylib already compiles cleanly under GCC.

**LL-014 — Emscripten main loop requires function extraction, not just a wrapper**
`emscripten_set_main_loop_arg` takes a `void(*)(void*)` callback — the entire frame body must be extracted into a standalone function. Local variables from `main()` can't be captured, so all mutable state must be packed into a struct passed through the `void*`. The `FrameState` struct pattern keeps this clean and minimizes global state per the project guidelines.

**LL-015 — Catch2 and test targets should be conditional on `NOT EMSCRIPTEN`**
FetchContent'ing Catch2 under Emscripten is wasteful (adds ~30s configure time) and the test executable can't run in a WASM context anyway. Guarding both the FetchContent and `add_subdirectory(tests)` behind `if(NOT EMSCRIPTEN)` keeps the web build fast and focused.

### Lessons Identified

**LI-012 — Emscripten SDK version pinning matters for CI reproducibility**
The local build uses emsdk 5.0.0 (latest at time of development) while the GitHub Actions workflow uses `setup-emsdk@v14` with version 3.1.51 (a stable known-good version). These could produce subtly different WASM binaries. If issues arise, the CI version should be updated to match the local development version, or vice versa.

**LI-013 — ASYNCIFY adds overhead — consider removing if not needed**
The `-sASYNCIFY` flag is included for safety (it allows `emscripten_sleep()` and similar async patterns), but `emscripten_set_main_loop_arg` doesn't strictly require it. ASYNCIFY can increase code size by 10–20%. If binary size becomes a concern, testing without ASYNCIFY may be worthwhile.

**LI-014 — Canvas scaling via CSS vs. internal resolution**
The current approach keeps the canvas at 1280×720 internal resolution and scales via CSS `width/height`. This means Raylib's `GetMouseX/Y` returns coordinates in the internal resolution, which is correct for the UI. However, on high-DPI displays, text may appear slightly blurry. A future improvement could detect `devicePixelRatio` and render at native resolution.

---

## Phase 6: Responsive UI & Polish

### Lessons Learned

**LL-016 — `--preload-file` generates a separate `.data` file that deployment pipelines miss**
Emscripten's `--preload-file` flag produces an `index.data` file alongside `index.js`/`index.wasm`. If the deployment workflow only copies `.html`/`.js`/`.wasm`, the browser hangs on the loading screen waiting for a `.data` file that doesn't exist. **Resolution:** Switch to `--embed-file` which bakes asset bytes directly into the WASM binary — no extra files to deploy. Trade-off is slightly larger WASM size but eliminates a failure mode entirely.

**LL-017 — Drawing background after content paints over everything**
When switching from a fixed panel height to a content-measured height, the natural approach is: draw content, measure how tall it was, then draw background at that height. But Raylib has no z-ordering — the background rectangle covers all previously drawn text. **Resolution:** Either pre-compute height arithmetically (sum of row heights) before drawing anything, or use a two-pass approach. Pre-computation is simpler and avoids double-drawing.

**LL-018 — Capping panel pixel height without capping content metrics causes overflow**
Hard-capping a panel at e.g. 220px while its internal buttons/fonts/spacing scale up with the window causes content to overflow the panel boundary. The cap must be applied at the *metrics* level, not the *container* level. **Resolution:** Two-tier scale factor — panel metrics use `min(factor, 1.0)` so they never grow beyond baseline, while viewport elements (circuit, title, HUD) use the full uncapped factor.

**LL-019 — Raylib's `IsWindowResized()` doesn't fire for Emscripten canvas resolution changes**
When JavaScript resizes `canvas.width`/`canvas.height` in response to a browser resize event, Raylib's `IsWindowResized()` may not detect it (GLFW doesn't generate a resize callback for programmatic canvas dimension changes). **Resolution:** Track `last_w`/`last_h` in the frame loop and compare against `GetScreenWidth()`/`GetScreenHeight()` each frame — this catches both native resize events and Emscripten canvas resolution changes.

**LL-020 — Custom fonts loaded by Raylib don't include all Unicode code points**
Raylib's `LoadFontEx()` loads a specified set of glyph code points (default: 95 ASCII printable characters). UTF-8 symbols like `✓` (U+2713), `→` (U+2192), `·` (U+00B7) fall outside this range and render as `?` or a missing glyph box. **Resolution:** Either expand the glyph range in `LoadFontEx()` (increases memory) or replace with ASCII equivalents. For a small UI, ASCII is simpler and avoids cross-platform font inconsistencies.

**LL-021 — CSS-only canvas scaling doesn't give Raylib the real viewport size**
Scaling a fixed-resolution canvas via CSS `width`/`height` means `GetScreenWidth()`/`GetScreenHeight()` always returns the original size (e.g. 1280×720) regardless of browser window. The responsive UI system never activates because it sees constant dimensions. **Resolution:** Set the actual `canvas.width`/`canvas.height` attributes to the available viewport size so Raylib reads the true resolution. CSS `width: 100%; height: 100%` makes the canvas fill its container.

### Lessons Identified

**LI-015 — Responsive scaling creates a tension between readability and space efficiency**
Small fonts at low resolutions vs. wasted space at high resolutions. The two-tier factor (panel capped at 1.0, viewport uncapped) is a pragmatic solution, but more sophisticated approaches — like font-size breakpoints or panel collapse/accordion behavior — could improve the experience at extreme resolutions (< 600px or > 2560px wide).

**LI-016 — Font fallback chains are platform-specific**
The app_font module tries bundled TTF → system path → Raylib default. The system path (`/usr/share/fonts/truetype/hack/`) is Linux-specific. macOS and Windows users building natively would skip straight to the Raylib default font. If cross-platform native builds become important, platform-conditional font paths or always-bundling the TTF is necessary.

**LI-017 — Debounced resize can cause a brief visual mismatch**
The 80ms debounce on canvas resize means the user sees ~5 frames at the old resolution during a drag. For a simulator this is acceptable, but for a polished product, consider requestAnimationFrame-based throttling instead of setTimeout debouncing — it aligns resize with the render cycle more naturally.

**LI-018 — Pre-computed panel heights are fragile against content changes**
The info panel's height is computed by summing expected row heights arithmetically. If new rows are added (e.g., overflow indicator, additional status lines), the estimate must be manually updated. A measuring pass — where content is drawn to a throwaway buffer or clipping region and the actual Y cursor tracked — would be more maintainable, at the cost of slightly more code.
