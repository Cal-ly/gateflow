/// @file main.cpp
/// @brief Gateflow entry point — interactive logic-gate visualizer
///
/// Builds a 7-bit ripple-carry adder with user-configurable inputs (0–99),
/// animated signal propagation, and optional NAND decomposition view.
/// Supports both native desktop and Emscripten/WASM builds.

#include "rendering/animation_state.hpp"
#include "rendering/app_font.hpp"
#include "rendering/gate_renderer.hpp"
#include "rendering/layout_engine.hpp"
#include "rendering/wire_renderer.hpp"
#include "simulation/circuit.hpp"
#include "simulation/circuit_builder.hpp"
#include "simulation/nand_decompose.hpp"
#include "timing/propagation_scheduler.hpp"
#include "ui/info_panel.hpp"
#include "ui/input_panel.hpp"
#include "ui/ui_scale.hpp"

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

constexpr int ADDER_BITS = 7;

/// Holds the entire simulation + rendering state that gets rebuilt on input/NAND changes.
struct AppState {
    std::unique_ptr<gateflow::Circuit> circuit;
    gateflow::Layout layout;
    std::unique_ptr<gateflow::PropagationScheduler> scheduler;
    std::unique_ptr<gateflow::AnimationState> anim;
    int result = 0;
    float scale = 40.0f;  // Will be recomputed by refit_circuit
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
    const auto& sc = gateflow::ui_scale();
    float screen_w = static_cast<float>(GetScreenWidth());
    float screen_h = static_cast<float>(GetScreenHeight());

    float available_w = screen_w - sc.panel_w - 2.0f * sc.margin - 2.0f * sc.circuit_padding;
    float available_h = screen_h - 2.0f * sc.circuit_padding - 40.0f; // 40px for title bar

    float bbox_w = app.layout.bounding_box.w;
    float bbox_h = app.layout.bounding_box.h;

    if (bbox_w <= 0.0f || bbox_h <= 0.0f) {
        app.scale = sc.max_ppu;
        app.offset = {0, 0};
        return;
    }

    // Pick the largest scale that fits both dimensions, capped at MAX
    float scale_w = available_w / bbox_w;
    float scale_h = available_h / bbox_h;
    app.scale = std::min({scale_w, scale_h, sc.max_ppu});
    if (app.scale < 4.0f) app.scale = 4.0f; // Floor to keep things visible

    float circuit_w = bbox_w * app.scale;
    float circuit_h = bbox_h * app.scale;
    float area_w = screen_w - sc.panel_w - sc.margin;

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

    int screen_w = GetScreenWidth();
    int screen_h = GetScreenHeight();

    // --- Detect any size change (native resize OR Emscripten canvas resize) ---
    static int last_w = 0, last_h = 0;
    if (screen_w != last_w || screen_h != last_h) {
        last_w = screen_w;
        last_h = screen_h;
        gateflow::update_ui_scale(screen_w, screen_h);
        refit_circuit(app);
    }

    // --- Recompute responsive UI metrics each frame ---
    gateflow::update_ui_scale(screen_w, screen_h);
    const auto& sc = gateflow::ui_scale();
    float panel_w = sc.panel_w;
    float ui_margin = sc.margin;

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
    gateflow::draw_adder_groups(*app.circuit, app.layout, app.scale, app.offset);
    gateflow::draw_wires(*app.circuit, app.layout, *app.anim, app.scale, app.offset);
    gateflow::draw_gates(*app.circuit, app.layout, *app.anim, app.scale, app.offset);
    gateflow::draw_io_labels(*app.circuit, app.layout, app.scale, app.offset);

    // Draw title
    std::string title = std::to_string(ui.input_a) + " + " + std::to_string(ui.input_b) + " = " +
                        std::to_string(app.result);
    int title_font = sc.title_font;
    int title_width = gateflow::MeasureAppText(title.c_str(), title_font);
    float circuit_area_w = static_cast<float>(screen_w) - panel_w - ui_margin;
    gateflow::DrawAppText(title.c_str(),
             static_cast<int>((circuit_area_w - static_cast<float>(title_width)) / 2.0f), 12, title_font,
             {240, 240, 240, 255});

    // Global propagation progress bar
    float progress = 0.0f;
    if (app.scheduler->max_depth() > 0) {
        progress = std::clamp(app.scheduler->current_depth() /
                                  static_cast<float>(app.scheduler->max_depth()),
                              0.0f, 1.0f);
    }
    Rectangle progress_track = {12.0f, 44.0f, circuit_area_w - 24.0f, sc.progress_h};
    DrawRectangleRounded(progress_track, 0.35f, 4, {45, 45, 55, 255});
    Rectangle progress_fill = progress_track;
    progress_fill.width *= progress;
    Color bar_color = app.scheduler->is_complete() ? Color{80, 220, 100, 230}
                                                   : Color{245, 190, 70, 220};
    DrawRectangleRounded(progress_fill, 0.35f, 4, bar_color);

    // --- Right-side UI panels ---
    float panel_x = static_cast<float>(screen_w) - panel_w - ui_margin;

    // Input panel (top right)
    gateflow::InputPanelResult input_panel =
        gateflow::draw_input_panel(ui, panel_x, ui_margin, panel_w);
    gateflow::UIAction action = input_panel.action;

    // Info panel (below input panel)
    float info_panel_y = ui_margin + input_panel.panel_height + 10.0f;
    float info_panel_h =
        gateflow::draw_info_panel(*app.circuit, *app.scheduler, ui.input_a, ui.input_b,
                                  app.result, panel_x, info_panel_y, panel_w);

    // Explanation panel (fills remaining vertical space)
    float expl_y = info_panel_y + info_panel_h + 10.0f;
    float expl_available_h = static_cast<float>(screen_h) - expl_y - ui_margin;
    gateflow::draw_explanation_panel(panel_x, expl_y, panel_w, *app.scheduler, ui.input_a,
                                     ui.input_b, app.result, expl_available_h);

    // --- Unified status indicator (top-right of circuit area) ---
    {
        const char* status_str = nullptr;
        Color status_color = {140, 140, 160, 255};

        if (app.scheduler->is_complete()) {
            status_str = "COMPLETE";
            status_color = {80, 220, 200, 255};
        } else if (app.scheduler->mode() == gateflow::PlaybackMode::PAUSED) {
            status_str = "PAUSED";
            status_color = {255, 200, 80, 255};
        } else if (app.scheduler->mode() == gateflow::PlaybackMode::REALTIME) {
            status_str = "PLAYING";
            status_color = {80, 220, 100, 255};
        } else {
            status_str = "READY";
        }

        int sw_text = gateflow::MeasureAppText(status_str, sc.hud_font);
        float status_x = circuit_area_w - static_cast<float>(sw_text) - 14.0f;
        gateflow::DrawAppText(status_str, static_cast<int>(status_x), 16, sc.hud_font, status_color);

        if (ui.show_nand) {
            const char* nand_str = "NAND";
            int nand_w = gateflow::MeasureAppText(nand_str, sc.hud_font - 1);
            gateflow::DrawAppText(nand_str, static_cast<int>(status_x) - nand_w - 12, 16,
                                  sc.hud_font - 1, {255, 160, 60, 255});
        }
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

    // --- Load custom font (must be after InitWindow) ---
    gateflow::init_app_font();

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

    gateflow::cleanup_app_font();
    CloseWindow();
    return 0;
}
