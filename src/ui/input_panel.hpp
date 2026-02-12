/// @file input_panel.hpp
/// @brief UI panel for circuit input controls — number inputs, buttons, slider, toggle.
///
/// All UI is drawn using Raylib primitives (no raygui/ImGui). The panel
/// reports actions back to the caller so the main loop can rebuild/reset
/// the circuit accordingly.

#pragma once

#include <raylib.h>

namespace gateflow {

/// Actions the input panel can request from the main loop
struct UIAction {
    bool inputs_changed = false; ///< User changed A or B — re-propagate
    bool run_pressed = false;    ///< User clicked Run — reset & play
    bool pause_pressed = false;  ///< Toggle pause
    bool step_pressed = false;   ///< Step one depth
    bool reset_pressed = false;  ///< Reset propagation to start
    bool nand_toggled = false;   ///< Toggle logical / NAND view
    bool speed_changed = false;  ///< Speed slider was dragged
};

/// Result of drawing the input panel.
struct InputPanelResult {
    UIAction action;
    float panel_height = 0.0f;
};

/// Persistent UI state — kept across frames
struct UIState {
    int input_a = 42;
    int input_b = 37;
    float speed = 3.0f;
    bool is_running = true;
    bool show_nand = false;

    // Internal editing state
    bool editing_a = false;
    bool editing_b = false;
    char buf_a[4] = "42";
    char buf_b[4] = "37";
    int cursor_a = 2;
    int cursor_b = 2;

    // Slider drag state
    bool dragging_speed = false;
};

/// Draws the input panel and handles mouse/keyboard interaction.
/// @param state  Mutable UI state (persists across frames)
/// @param panel_x Left edge of the panel in screen coordinates
/// @param panel_y Top edge of the panel in screen coordinates
/// @param panel_w Width of the panel
/// @return Actions and rendered panel height for dynamic panel stacking
InputPanelResult draw_input_panel(UIState& state, float panel_x, float panel_y, float panel_w);

} // namespace gateflow
