/// @file info_panel.hpp
/// @brief Information panel showing binary/decimal readouts and propagation status.

#pragma once

#include "simulation/circuit.hpp"
#include "timing/propagation_scheduler.hpp"

#include <raylib.h>

namespace gateflow {

/// Draws the information panel showing:
/// - Binary representation of A, B, and the result (bits highlight as they resolve)
/// - Decimal result (appears when propagation completes)
/// - Status text describing the current propagation phase
/// @param circuit    The circuit being visualized
/// @param scheduler  The propagation scheduler (for depth/resolution queries)
/// @param input_a    Decimal value of input A
/// @param input_b    Decimal value of input B
/// @param result     Decimal result value
/// @param panel_x    Left edge of panel in screen coords
/// @param panel_y    Top edge of panel in screen coords
/// @param panel_w    Width of the panel
/// @return Rendered panel height, for dynamic stacking.
float draw_info_panel(const Circuit& circuit, const PropagationScheduler& scheduler, int input_a,
                      int input_b, int result, float panel_x, float panel_y, float panel_w);

/// Draws a static explanation panel for novice users.
/// @param panel_x Left edge of panel in screen coords
/// @param panel_y Top edge of panel in screen coords
/// @param panel_w Width of the panel
/// @return Rendered panel height.
float draw_explanation_panel(float panel_x, float panel_y, float panel_w);

} // namespace gateflow
