# Gateflow — Enhancement Specification v3

Based on review of the current implementation after v2 enhancements were applied.

**Critical constraint:** The project now has a responsive grid layout system that adapts to window size. All changes in this spec must work within and respect that existing grid system. Do not replace or override the responsive layout — extend it. When modifying panel sizes or adding new elements, ensure the grid recalculates correctly at different window sizes. Test at both the default window size and at least one significantly smaller size.

---

## 1. Collapsible "How It Works" Explanation Panel

**Priority: High**

The explanation panel currently shows all content at once in a dense block. Restructure it into a collapsible accordion-style system with three distinct sections.

### Section structure

**"What's happening now"** — remains always visible at the top of the explanation area. This is the dynamic, state-aware message that updates during propagation (e.g., "Carry propagating through bit 3", "Complete: 75 + 37 = 112"). This is already implemented and working well. Keep it prominent — it should be the first thing the eye hits below the result display.

**"Logic Gates" (collapsible, default collapsed):**
- Brief intro: "Logic gates are the fundamental building blocks of digital circuits. Each gate takes one or more binary inputs (0 or 1) and produces a single binary output based on a fixed rule."
- Then a short entry for each gate type used in the project:
  - **XOR (Exclusive OR):** Output is 1 when exactly one input is 1. Used to compute sum bits — it answers "are these two bits different?" Truth table: 0,0→0 | 0,1→1 | 1,0→1 | 1,1→0
  - **AND:** Output is 1 only when both inputs are 1. Used to detect carries — if both bits are 1, the addition overflows into the next bit. Truth table: 0,0→0 | 0,1→0 | 1,0→0 | 1,1→1
  - **OR:** Output is 1 when at least one input is 1. Used to combine carry paths — a carry is produced if either carry source is active. Truth table: 0,0→0 | 0,1→1 | 1,0→1 | 1,1→1
  - **NAND (NOT AND):** The inverse of AND — output is 0 only when both inputs are 1. Uniquely, every other gate can be built from NAND gates alone. This is called "functional completeness." Truth table: 0,0→1 | 0,1→1 | 1,0→1 | 1,1→0
- Format each gate entry compactly: gate name in the accent color for that gate type (teal for XOR, orange for AND, yellow for OR), one-line description, a small inline truth table
- The truth tables here are static reference — the interactive hover tooltips on the actual gates show the live state

**"Ripple-Carry Adder" (collapsible, default collapsed):**
- Start with the concept: "A ripple-carry adder adds two binary numbers the same way you add decimal numbers by hand — one column at a time, right to left, carrying overflow to the next column."
- Explain the full adder unit: "Each column is handled by a full adder, a small circuit of 5 gates (2 XOR, 2 AND, 1 OR) that takes two input bits plus a carry-in, and produces a sum bit plus a carry-out."
- Explain the carry chain: "The carry-out from each full adder feeds into the carry-in of the next. This chain is shown in amber — it's the critical path that determines how fast the adder operates. The carry must 'ripple' through every bit position, which is why this design is called a ripple-carry adder."
- Explain why it matters: "This propagation delay is why real CPUs use more complex adder designs like carry-lookahead adders. But the ripple-carry adder makes the process visible — you can see the carry wave move across the circuit."
- Briefly explain overflow: "If the final carry-out (Cout) is 1, the result exceeds 7 bits. This is overflow — the sum needs an 8th bit. Try 99 + 99 to see this in action."

**"Reading the Animation" (collapsible, default collapsed):**
- Color legend:
  - Green fill = gate output is 1 (active)
  - Gray fill = gate output is 0 (resolved inactive)
  - Dim/translucent = gate not yet resolved (signal hasn't reached it)
  - Amber wire = carry chain connecting full adders
  - Green wire = signal carrying 1
  - Dark wire = signal carrying 0
- Note about gate accents: "Each gate type has a colored accent border: teal for XOR, orange for AND, yellow for OR"
- Tip: "Hover over any gate to see its truth table with the current inputs highlighted"
- Tip: "Use the speed slider to slow down propagation and watch each gate resolve individually"

### Collapsible implementation details

- Each section header is clickable and shows a toggle indicator (▶ collapsed, ▼ expanded)
- Clicking toggles the visibility of that section's content
- Store the collapsed/expanded state so it persists across propagation runs within the same session
- When collapsed, only the section header is visible — the panel reclaims that vertical space for other sections
- Ensure the collapsible behavior works within the responsive grid — expanding a section should scroll the panel content if it overflows, not push other UI elements out of the grid
- The panel should have vertical scrolling if content exceeds available height (this may already exist)

### Text formatting

- Use a readable font size — the current explanation text is quite small. If a TTF font is loaded, use it here at a size that's comfortable for reading (at least 13-14px equivalent)
- Line spacing should be generous enough for readability (1.3-1.5x line height)
- Keep line width reasonable — if the panel is narrow, prefer wrapping over horizontal overflow
- Section content should have a small left indent or left border to visually group it under its header

---

## 2. Visual Propagation Progress Bar

**Priority: High**

Replace the text-based "Propagation depth 13/12" with a visual progress bar.

Implementation:
- Thin horizontal bar (6-8px tall) positioned at the top of the circuit area, spanning the full width of the circuit (not the info panel)
- Background: dark track color matching the panel backgrounds
- Fill: gradient or solid color that advances left-to-right as `current_depth / max_depth` increases
- Fill color: match the carry chain amber, or use a green that matches active gates
- When propagation completes, the bar should be fully filled and could briefly flash or pulse once
- Remove the redundant "Depth: 13 / 12" text from the bottom-left corner
- Remove the text-based "Propagation depth 13/12" from the top-left corner — the bar replaces both
- Fix the off-by-one: current_depth should not exceed max_depth (currently shows 13/12)

Integration with responsive grid:
- The progress bar should be a fixed-height element above the circuit area within the grid
- It should resize horizontally with the circuit area, not with the full window width
- It should not push the circuit downward significantly — keep it thin

---

## 3. Consistent Gate Accent Colors

**Priority: High**

Currently, gate accents appear inconsistent — some AND gates show orange accents while others don't, and OR gates appear to have no accent. Every instance of each gate type must have the same accent treatment regardless of position in the circuit.

Rules:
- XOR gates: teal/cyan accent (left border or top border, 3-4px wide)
- AND gates: orange accent (same style)
- OR gates: yellow accent (same style)
- NAND gates (when NAND view is on): a distinct color, perhaps magenta or red
- The accent is always visible regardless of gate state (active/inactive/pending)
- The accent does not change color with state — it identifies type, while the fill identifies state
- Verify this on all gates across the full circuit — both the upper gate pair (half adder 1) and lower gate pair (half adder 2 + OR) within each full adder

---

## 4. Unified Status Location

**Priority: Medium**

"PLAYING" appears bottom-left, "PROPAGATION COMPLETE" appears top-right. These should be in the same location.

Implementation:
- Place the status indicator in a single consistent location — top-right, just above or to the left of the "INPUTS" header, where "PROPAGATION COMPLETE" currently appears
- States: "READY" (before first run), "PLAYING" (during propagation), "PAUSED", "PROPAGATION COMPLETE"
- Use color coding: green for PLAYING, amber for PAUSED, cyan for COMPLETE, gray for READY
- Remove the bottom-left status text entirely

---

## 5. Carry Status Visualization

**Priority: Medium**

The carry chain status "C0 ok C1 ok C2 ok ..." is functional but text-heavy.

Replace with a compact visual indicator:
- A row of 7 small circles (or squares) labeled C0 through C6
- Unresolved: dim/empty circle
- Resolved with carry=0: gray filled circle
- Resolved with carry=1: amber filled circle (matching the carry chain wire color)
- As propagation advances, circles fill in from right (C0) to left (C6), mirroring the carry wave in the circuit
- Keep the "Cout: 0/1" label at the end
- This should fit on a single line in the result section

---

## 6. Binary Display Active Bit Highlighting

**Priority: Medium**

In the result binary display, "1" and "0" bits are currently the same color. Differentiate them:

- Bits with value "1": bright green text (matching active gate fill color)
- Bits with value "0": dim gray text
- This creates a visual connection between the binary readout and the circuit — the same "green = 1" mapping applies in both places
- Apply to all three rows (A, B, S)
- During propagation, unresolved S bits should remain dim/neutral (not green or gray) until they resolve

---

## 7. Better Font

**Priority: Medium**

The current rendering uses what appears to be raylib's default bitmap font, which is rough at small sizes — especially visible in the explanation text and carry status line.

Implementation:
- Load a TTF font at startup. Recommended options (all freely licensed):
  - **JetBrains Mono** — designed for code/technical content, excellent readability at small sizes, monospaced
  - **Inter** — clean proportional font, great for UI text
  - **IBM Plex Mono** — technical aesthetic, good at small sizes
- Use two font sizes: a "body" size for explanation text and panel content (14-16px equivalent), and a "label" size for gate labels and pin labels (12-14px)
- The header "Gateflow — Logic Gate Simulator" and the equation "75 + 37 = 112" can use a larger size (20-24px)
- Load the font in the initialization phase and pass it to all rendering functions — avoid loading per-frame
- Raylib's `LoadFontEx()` loads TTF with specified sizes. Load at the sizes you need.
- Bundle the font file in the `assets/` directory and ensure it's included in the WASM build (Emscripten `--preload-file`)

---

## 8. Bit 7 Overflow Visual Cleanup

**Priority: Low-Medium**

The "Bit 7 overflow" section in the bottom-right of the circuit is cramped and the label is partially obscured.

Improvements:
- Give the Cout output its own small group panel (matching the style of the Bit 0-6 panels) labeled "Overflow"
- Position it cleanly to the right of or below the Bit 0 group with adequate spacing
- The Cout label and value ("Cout: 0" / "Cout: 1") should be the same size as the other output labels (S0: 1, S1: 0, etc.)
- When Cout = 1, highlight it in amber or red to draw attention to the overflow condition
- Verify this doesn't collide with the info panel at various window sizes — the responsive grid should handle the spacing

---

## 9. Wire Routing Spacing

**Priority: Low**

Internal wires within full adders (especially Bit 4 and Bit 5 columns) run very close to gate borders and to each other.

Improvements:
- Increase the vertical gap between the upper gate pair (first XOR + first AND) and the lower gate pair (second XOR + second AND + OR) within each full adder
- This gives internal wires more routing room
- The amount of increase should be proportional to the existing spacing — roughly 20-30% more vertical gap
- Ensure this increase works with the responsive grid scaling — the gap should scale proportionally, not be a fixed pixel value

---

## 10. Input Pin Label Size

**Priority: Low**

Input pin labels (A6:1, B6:0, etc.) are small relative to gate labels. Since they're the entry point for understanding the visualization, increase their font size to match or approach the gate label size.

Similarly, output pin labels (S0: 1, S1: 0, etc.) should be slightly larger. They are the "answer" and deserve visual weight.

---

## Implementation Notes for Claude Code

### Responsive grid preservation

Every change in this spec must be verified against the existing responsive grid:
- After implementing each enhancement, resize the window to at least 3 sizes (default, 75% width, 50% width) and verify nothing overlaps, overflows, or disappears
- The collapsible explanation panel (Enhancement 1) is the highest risk for grid interaction — ensure expanded sections trigger scrolling within the panel rather than expanding the panel's grid allocation
- The progress bar (Enhancement 2) adds a new element to the grid — allocate it a fixed small height that doesn't compress the circuit area
- Font changes (Enhancement 7) may affect text bounding boxes throughout the UI — verify all text fits within its allocated space at the new font/sizes

### Implementation order

Work through these enhancements in the numbered priority order. Complete and verify each one before moving to the next. Each enhancement should be a separate commit (or small group of commits).

### Testing checklist per enhancement

- [ ] Compiles with zero warnings
- [ ] Renders correctly at default window size
- [ ] Renders correctly at reduced window size (responsive grid check)
- [ ] Does not break existing functionality (propagation, animation, hover tooltips, presets)
- [ ] Interacts correctly with NAND view toggle (if applicable)
- [ ] Propagation animation still looks correct (timing, coloring, state transitions)
