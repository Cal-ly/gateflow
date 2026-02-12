#pragma once

/// @file layout_engine.hpp
/// @brief Computes screen positions for circuit elements in logical units

#include "simulation/circuit.hpp"

#include <map>
#include <vector>

namespace gateflow {

/// A 2D point in logical coordinates
struct Vec2 {
    float x = 0.0f;
    float y = 0.0f;
};

/// An axis-aligned rectangle in logical coordinates
struct Rect {
    float x = 0.0f;
    float y = 0.0f;
    float w = 0.0f;
    float h = 0.0f;
};

/// A single routed branch path for one wire destination.
/// Includes precomputed metrics for efficient animation rendering.
struct WirePath {
    std::vector<Vec2> points;
    std::vector<float> cumulative_lengths; ///< cumulative_lengths[0] = 0
    float total_length = 0.0f;
};

/// Holds the computed positions of every visual element in a circuit.
/// All coordinates are in logical units — the renderer scales to screen pixels.
struct Layout {
    std::map<const Gate*, Rect> gate_positions;
    std::map<const Wire*, std::vector<WirePath>> wire_paths;
    std::vector<Vec2> input_positions;
    std::vector<Vec2> output_positions;
    Rect bounding_box;
};

/// Computes a deterministic layout for a circuit.
///
/// For a ripple-carry adder, gates are grouped by full-adder columns arranged
/// right-to-left (bit 0 = rightmost). Within each column, gates are arranged
/// top-to-bottom. Inputs enter from the top, outputs exit at the bottom,
/// and the carry chain flows left across the top.
///
/// @param circuit The finalized circuit to lay out
/// @return Layout with all positions in logical units
[[nodiscard]] Layout compute_layout(const Circuit& circuit);

} // namespace gateflow
