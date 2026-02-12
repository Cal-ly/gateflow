/// @file ui_scale.cpp
/// @brief Computes per-frame responsive UI metrics from the window size.

#include "ui/ui_scale.hpp"

namespace gateflow {

namespace {

UIScale g_scale;

int scaled_font(float base, float factor) {
    int v = static_cast<int>(std::round(base * factor));
    return std::clamp(v, static_cast<int>(base * 0.65f), static_cast<int>(base * 1.6f));
}

} // namespace

void update_ui_scale(int screen_w, int screen_h) {
    constexpr float BASELINE_H = 720.0f;
    constexpr float BASELINE_W = 1280.0f;

    float sw = static_cast<float>(screen_w);
    float sh = static_cast<float>(screen_h);

    // Master factor: blend of height-based and width-based scaling (height-dominant)
    float hf = sh / BASELINE_H;
    float wf = sw / BASELINE_W;
    g_scale.factor = std::clamp(hf * 0.7f + wf * 0.3f, 0.6f, 1.8f);

    // Panel width: 30% of screen, clamped
    g_scale.panel_w = std::clamp(sw * 0.30f, 280.0f, 500.0f);
    g_scale.margin = std::clamp(10.0f * g_scale.factor, 6.0f, 16.0f);

    // Panel factor: scales down on small screens but never upscales beyond
    // 720p baseline, so input/result panels stay compact on large displays.
    float f = g_scale.factor;
    float pf = std::min(f, 1.0f);

    // Font sizes (panel-capped)
    g_scale.font_normal = scaled_font(16.0f, pf);
    g_scale.font_small  = scaled_font(14.0f, pf);
    g_scale.font_big    = scaled_font(21.0f, pf);
    g_scale.font_tiny   = scaled_font(12.0f, pf);

    // Spacing / layout (panel-capped)
    g_scale.row_height     = std::round(23.0f * pf);
    g_scale.padding        = std::round(10.0f * pf);
    g_scale.button_height  = std::round(34.65f * pf);
    g_scale.field_height   = std::round(32.34f * pf);
    g_scale.slider_height  = std::round(23.1f * pf);
    g_scale.row_gap        = std::round(8.0f * pf);

    // Circuit viewport (uses full factor — scales up on large screens)
    g_scale.circuit_padding = std::clamp(40.0f * f, 20.0f, 70.0f);
    g_scale.max_ppu         = std::clamp(40.0f * f, 20.0f, 80.0f);
    g_scale.title_font      = scaled_font(24.0f, f);
    g_scale.hud_font        = scaled_font(14.0f, f);
    g_scale.progress_font   = scaled_font(12.0f, f);
    g_scale.progress_h      = std::clamp(10.0f * f, 6.0f, 18.0f);
}

const UIScale& ui_scale() {
    return g_scale;
}

} // namespace gateflow
