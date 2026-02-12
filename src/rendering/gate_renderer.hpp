/// @file gate_renderer.hpp
/// @brief Draws gates as rounded rectangles with type labels and active/inactive coloring

#pragma once

#include "rendering/animation_state.hpp"
#include "rendering/layout_engine.hpp"
#include "simulation/circuit.hpp"

#include <raylib.h>

namespace gateflow {

/// Draws all gates with animation state (pending/resolved/active coloring).
/// @param circuit The circuit whose gates are drawn
/// @param layout  Precomputed positions for gates and wires
/// @param anim    Current animation state for fade-in and pulsing
/// @param scale   Pixels per logical unit
/// @param offset  Screen-space offset (for camera/viewport)
void draw_gates(const Circuit& circuit, const Layout& layout, const AnimationState& anim,
                float scale, Vector2 offset);

/// Draws input/output labels and connection points.
/// @param circuit The circuit
/// @param layout  Precomputed positions
/// @param scale   Pixels per logical unit
/// @param offset  Screen-space offset
void draw_io_labels(const Circuit& circuit, const Layout& layout, float scale, Vector2 offset);

} // namespace gateflow
