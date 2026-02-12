/// @file ui_scale.hpp
/// @brief Global UI scaling state derived from the current window dimensions.
///
/// Provides a single source of truth for responsive layout values so that
/// panels, fonts, and spacing adapt to the window size without every
/// function needing extra parameters.

#pragma once

#include <algorithm>
#include <cmath>

namespace gateflow {

/// Recalculated once per frame from the current window size.
struct UIScale {
    float factor = 1.0f;      ///< Master scale: screen_h / 720
    float panel_w = 400.0f;   ///< Right-side panel width (responsive)
    float margin = 10.0f;     ///< Outer margin around panels

    // Derived font sizes (clamped integers)
    int font_normal = 16;
    int font_small = 14;
    int font_big = 21;
    int font_tiny = 12;

    // Derived spacing
    float row_height = 23.0f;
    float padding = 10.0f;
    float button_height = 34.65f;
    float field_height = 32.34f;
    float slider_height = 23.1f;
    float row_gap = 8.0f;

    // Circuit viewport scaling
    float circuit_padding = 40.0f; ///< Pixels around circuit area
    float max_ppu = 40.0f;        ///< Max pixels-per-unit for circuit
    int title_font = 24;          ///< Title font over circuit area
    int hud_font = 14;            ///< HUD font (bottom-left)
    int progress_font = 12;       ///< Progress bar label font
    float progress_h = 10.0f;     ///< Progress bar track height
};

/// Updates the global UI scale from the current screen dimensions.
/// Call once per frame, before drawing any UI panels.
void update_ui_scale(int screen_w, int screen_h);

/// Returns a const reference to the current UI scale values.
const UIScale& ui_scale();

} // namespace gateflow
