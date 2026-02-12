/// @file wire_renderer.hpp
/// @brief Draws wires as polylines with right-angle routing and active/inactive coloring

#pragma once

#include "rendering/animation_state.hpp"
#include "rendering/layout_engine.hpp"
#include "simulation/circuit.hpp"

#include <raylib.h>

namespace gateflow {

/// Draws all wires with animation state (signal travel, resolved/unresolved).
/// Wires carrying 0 are thin dark gray; wires carrying 1 are thick bright green.
/// Unresolved wires are dimmed. Signal pulses travel along wires as they resolve.
/// @param circuit The circuit whose wires are drawn
/// @param layout  Precomputed wire paths
/// @param anim    Current animation state for signal travel
/// @param scale   Pixels per logical unit
/// @param offset  Screen-space offset (for camera/viewport)
void draw_wires(const Circuit& circuit, const Layout& layout, const AnimationState& anim,
                float scale, Vector2 offset);

} // namespace gateflow
