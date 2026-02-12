/// @file main.cpp
/// @brief Gateflow entry point — interactive logic-gate visualizer
///
/// Builds a 7-bit ripple-carry adder with user-configurable inputs (0–99),
/// animated signal propagation, and optional NAND decomposition view.
/// Supports both native desktop and Emscripten/WASM builds.

#include "rendering/animation_state.hpp"
#include "rendering/gate_renderer.hpp"
#include "rendering/layout_engine.hpp"
#include "rendering/wire_renderer.hpp"
#include "simulation/circuit.hpp"
#include "simulation/circuit_builder.hpp"
#include "simulation/nand_decompose.hpp"
#include "timing/propagation_scheduler.hpp"
#include "ui/info_panel.hpp"
#include "ui/input_panel.hpp"

#include <raylib.h>

#ifdef __EMSCRIPTEN__
#include <emscripten/emscripten.h>
#endif

#include <algorithm>
#include <memory>
#include <string>

namespace {

constexpr int INITIAL_WIDTH = 1280;
constexpr int INITIAL_HEIGHT = 720;
constexpr int MIN_WIDTH = 900;
constexpr int MIN_HEIGHT = 500;
constexpr int TARGET_FPS = 60;
constexpr float MAX_PIXELS_PER_UNIT = 40.0f; // Upper bound for scale
constexpr float UI_PANEL_WIDTH = 220.0f;
constexpr float UI_MARGIN = 10.0f;
constexpr float CIRCUIT_PADDING = 40.0f; // Pixels of padding around the circuit

constexpr int ADDER_BITS = 7;

/// Holds the entire simulation + rendering state that gets rebuilt on input/NAND changes.
struct AppState {
    std::unique_ptr<gateflow::Circuit> circuit;
    gateflow::Layout layout;
    std::unique_ptr<gateflow::PropagationScheduler> scheduler;
    std::unique_ptr<gateflow::AnimationState> anim;
    int result = 0;
    float scale = MAX_PIXELS_PER_UNIT;
    Vector2 offset = {0, 0};
};

/// Sets the bits of value A and B on a 7-bit ripple-carry adder circuit.
void set_adder_inputs(gateflow::Circuit& circuit, int a, int b) {
    for (int i = 0; i < ADDER_BITS; i++) {
        circuit.set_input(i, (a >> i) & 1);
        circuit.set_input(ADDER_BITS + i, (b >> i) & 1);
    }
}

/// Reads the sum result from a 7-bit ripple-carry adder.
int read_adder_output(const gateflow::Circuit& circuit) {
    int result = 0;
    for (int i = 0; i < ADDER_BITS; i++) {
        if (circuit.get_output(i)) {
            result |= (1 << i);
        }
    }
    if (circuit.get_output(ADDER_BITS)) {
        result |= (1 << ADDER_BITS);
    }
    return result;
}

// Forward declaration — defined after rebuild_app_state
void refit_circuit(AppState& app);

/// Builds (or rebuilds) the complete app state from the current UI inputs.
void rebuild_app_state(AppState& app, const gateflow::UIState& ui) {
    // 1. Build the circuit
    app.circuit = gateflow::build_ripple_carry_adder(ADDER_BITS);

    // 2. Apply NAND decomposition if toggled
    if (ui.show_nand) {
        gateflow::decompose_to_nand(*app.circuit);
    }

    // 3. Set inputs and propagate
    set_adder_inputs(*app.circuit, ui.input_a, ui.input_b);
    (void)app.circuit->propagate();
    app.result = read_adder_output(*app.circuit);

    // 4. Compute layout
    app.layout = gateflow::compute_layout(*app.circuit);

    // 5. Create scheduler and animation
    app.scheduler = std::make_unique<gateflow::PropagationScheduler>(app.circuit.get());
    app.scheduler->set_speed(ui.speed);
    app.anim = std::make_unique<gateflow::AnimationState>(app.circuit.get());

    // 6. Fit circuit into available area and center it
    refit_circuit(app);
}

/// Recomputes scale and offset to fit the circuit in the current window.
/// Called on rebuild and on window resize.
void refit_circuit(AppState& app) {
    float screen_w = static_cast<float>(GetScreenWidth());
    float screen_h = static_cast<float>(GetScreenHeight());

    float available_w = screen_w - UI_PANEL_WIDTH - 2.0f * UI_MARGIN - 2.0f * CIRCUIT_PADDING;
    float available_h = screen_h - 2.0f * CIRCUIT_PADDING - 40.0f; // 40px for title bar

    float bbox_w = app.layout.bounding_box.w;
    float bbox_h = app.layout.bounding_box.h;

    if (bbox_w <= 0.0f || bbox_h <= 0.0f) {
        app.scale = MAX_PIXELS_PER_UNIT;
        app.offset = {0, 0};
        return;
    }

    // Pick the largest scale that fits both dimensions, capped at MAX
    float scale_w = available_w / bbox_w;
    float scale_h = available_h / bbox_h;
    app.scale = std::min({scale_w, scale_h, MAX_PIXELS_PER_UNIT});
    if (app.scale < 4.0f) app.scale = 4.0f; // Floor to keep things visible

    float circuit_w = bbox_w * app.scale;
    float circuit_h = bbox_h * app.scale;
    float area_w = screen_w - UI_PANEL_WIDTH - UI_MARGIN;

    app.offset = {
        (area_w - circuit_w) / 2.0f - app.layout.bounding_box.x * app.scale,
        (screen_h - circuit_h) / 2.0f - app.layout.bounding_box.y * app.scale + 20.0f};
}

/// Resets propagation without rebuilding the circuit (for input value changes only).
void reset_propagation(AppState& app, const gateflow::UIState& ui) {
    set_adder_inputs(*app.circuit, ui.input_a, ui.input_b);
    (void)app.circuit->propagate();
    app.result = read_adder_output(*app.circuit);

    app.scheduler->reset();
    app.anim->reset();
    app.scheduler->set_speed(ui.speed);
}

/// Returns a mode label string
const char* mode_label(gateflow::PlaybackMode mode) {
    switch (mode) {
    case gateflow::PlaybackMode::REALTIME:
        return "PLAYING";
    case gateflow::PlaybackMode::PAUSED:
        return "PAUSED";
    case gateflow::PlaybackMode::STEP:
        return "STEP";
    }
    return "???";
}

/// All mutable state needed by the frame loop, bundled so it can be passed
/// through Emscripten's void* callback.
struct FrameState {
    gateflow::UIState ui;
    AppState app;
};

/// One frame of the application — called each tick by the native loop or by
/// emscripten_set_main_loop_arg.
void frame_tick(FrameState& state) {
    auto& ui = state.ui;
    auto& app = state.app;
    float dt = GetFrameTime();

    // --- Detect window resize and refit circuit ---
    if (IsWindowResized()) {
        refit_circuit(app);
    }

    int screen_w = GetScreenWidth();
    int screen_h = GetScreenHeight();

    // --- Handle keyboard shortcuts (only when not editing a text field) ---
    if (!ui.editing_a && !ui.editing_b) {
        if (IsKeyPressed(KEY_SPACE)) {
            app.scheduler->toggle_pause();
            ui.is_running = (app.scheduler->mode() == gateflow::PlaybackMode::REALTIME);
        }
        if (IsKeyPressed(KEY_RIGHT)) {
            app.scheduler->step();
        }
        if (IsKeyPressed(KEY_R)) {
            reset_propagation(app, ui);
            app.scheduler->set_mode(gateflow::PlaybackMode::REALTIME);
            ui.is_running = true;
        }
    }

    // --- Update simulation ---
    app.scheduler->tick(dt);
    app.anim->update(dt, *app.scheduler);

    // --- Draw ---
    BeginDrawing();
    ClearBackground({25, 25, 30, 255});

    // Draw circuit in the main area (left of the UI panels)
    gateflow::draw_wires(*app.circuit, app.layout, *app.anim, app.scale, app.offset);
    gateflow::draw_gates(*app.circuit, app.layout, *app.anim, app.scale, app.offset);
    gateflow::draw_io_labels(*app.circuit, app.layout, app.scale, app.offset);

    // Draw title
    std::string title = std::to_string(ui.input_a) + " + " + std::to_string(ui.input_b) + " = " +
                        std::to_string(app.result);
    int title_width = MeasureText(title.c_str(), 24);
    float circuit_area_w = static_cast<float>(screen_w) - UI_PANEL_WIDTH - UI_MARGIN;
    DrawText(title.c_str(),
             static_cast<int>((circuit_area_w - static_cast<float>(title_width)) / 2.0f), 12, 24,
             {240, 240, 240, 255});

    // --- Right-side UI panels ---
    float panel_x = static_cast<float>(screen_w) - UI_PANEL_WIDTH - UI_MARGIN;

    // Input panel (top right)
    gateflow::UIAction action = gateflow::draw_input_panel(ui, panel_x, UI_MARGIN, UI_PANEL_WIDTH);

    // Info panel (below input panel)
    gateflow::draw_info_panel(*app.circuit, *app.scheduler, ui.input_a, ui.input_b, app.result,
                              panel_x, 330.0f, UI_PANEL_WIDTH);

    // --- HUD: mode, depth, NAND indicator ---
    const char* mode_str = mode_label(app.scheduler->mode());
    DrawText(mode_str, 10, screen_h - 50, 14,
             app.scheduler->mode() == gateflow::PlaybackMode::PAUSED ? Color{255, 200, 80, 255}
                                                                     : Color{80, 220, 100, 255});

    std::string depth_str =
        "Depth: " + std::to_string(static_cast<int>(app.scheduler->current_depth())) + " / " +
        std::to_string(app.scheduler->max_depth());
    DrawText(depth_str.c_str(), 10, screen_h - 32, 13, {140, 140, 140, 255});

    if (ui.show_nand) {
        DrawText("NAND VIEW", 10, screen_h - 68, 13, {255, 160, 60, 255});
    }

    if (app.scheduler->is_complete()) {
        DrawText("PROPAGATION COMPLETE",
                 static_cast<int>(circuit_area_w) - MeasureText("PROPAGATION COMPLETE", 16) - 10,
                 16, 16, {80, 220, 100, 255});
    }

    EndDrawing();

    // --- Process UI actions (take effect next frame) ---
    if (action.nand_toggled) {
        rebuild_app_state(app, ui);
        app.scheduler->set_mode(gateflow::PlaybackMode::REALTIME);
        ui.is_running = true;
    } else if (action.inputs_changed || action.run_pressed) {
        reset_propagation(app, ui);
        app.scheduler->set_mode(gateflow::PlaybackMode::REALTIME);
        ui.is_running = true;
    }

    if (action.pause_pressed) {
        app.scheduler->toggle_pause();
        ui.is_running = (app.scheduler->mode() == gateflow::PlaybackMode::REALTIME);
    }
    if (action.step_pressed) {
        app.scheduler->step();
        ui.is_running = false;
    }
    if (action.reset_pressed) {
        reset_propagation(app, ui);
        app.scheduler->set_mode(gateflow::PlaybackMode::REALTIME);
        ui.is_running = true;
    }
    if (action.speed_changed) {
        app.scheduler->set_speed(ui.speed);
    }
}

#ifdef __EMSCRIPTEN__
/// Emscripten main loop callback — unwraps the void* to FrameState.
void emscripten_frame(void* arg) {
    auto* state = static_cast<FrameState*>(arg);
    frame_tick(*state);
}
#endif

} // namespace

int main() {
    // --- Initialize Raylib window ---
    SetConfigFlags(FLAG_WINDOW_RESIZABLE);
    InitWindow(INITIAL_WIDTH, INITIAL_HEIGHT, "Gateflow — Logic Gate Simulator");
    SetWindowMinSize(MIN_WIDTH, MIN_HEIGHT);
    SetTargetFPS(TARGET_FPS);

    // --- Create all mutable state ---
    FrameState state;
    rebuild_app_state(state.app, state.ui);

#ifdef __EMSCRIPTEN__
    // Emscripten takes ownership of the main loop — we pass state via void*.
    // 0 = use requestAnimationFrame (browser-native vsync), 1 = simulate infinite loop.
    emscripten_set_main_loop_arg(emscripten_frame, &state, 0, 1);
#else
    while (!WindowShouldClose()) {
        frame_tick(state);
    }
#endif

    CloseWindow();
    return 0;
}
