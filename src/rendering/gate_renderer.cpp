/// @file gate_renderer.cpp
/// @brief Draws gates as rounded rectangles with type labels and animated state coloring

#include "rendering/gate_renderer.hpp"

#include "simulation/gate.hpp"
#include "simulation/wire.hpp"

#include <algorithm>
#include <cmath>
#include <string>

namespace gateflow {

namespace {

// --- Color palette ---
const Color GATE_INACTIVE_FILL = {80, 80, 80, 255};       // Dark gray
const Color GATE_INACTIVE_OUTLINE = {120, 120, 120, 255}; // Medium gray
const Color GATE_ACTIVE_FILL = {30, 180, 60, 255};        // Bright green
const Color GATE_ACTIVE_OUTLINE = {50, 220, 80, 255};     // Highlighted green
const Color GATE_PENDING_FILL = {50, 50, 55, 255};        // Very dim gray
const Color GATE_PENDING_OUTLINE = {80, 80, 90, 255};     // Dim outline
const Color LABEL_COLOR = {240, 240, 240, 255};           // Near-white
const Color IO_DOT_COLOR = {200, 200, 50, 255};           // Yellow for I/O dots
const Color INPUT_LABEL_COLOR = {180, 180, 255, 255};     // Light blue for input labels
const Color OUTPUT_LABEL_COLOR = {255, 180, 180, 255};    // Light red for output labels

constexpr float CORNER_ROUNDNESS = 0.3f; // Raylib roundness parameter (0.0–1.0)
constexpr int CORNER_SEGMENTS = 4;
constexpr float OUTLINE_THICKNESS = 2.0f;
constexpr int FONT_SIZE_GATE = 14;
constexpr int FONT_SIZE_IO = 12;
constexpr float IO_DOT_RADIUS = 4.0f;

/// Converts a logical-unit rect to screen-space
Rectangle to_screen(const Rect& r, float scale, Vector2 offset) {
    return {r.x * scale + offset.x, r.y * scale + offset.y, r.w * scale, r.h * scale};
}

/// Converts a logical-unit vec2 to screen-space
Vector2 to_screen(Vec2 v, float scale, Vector2 offset) {
    return {v.x * scale + offset.x, v.y * scale + offset.y};
}

/// Linearly interpolate between two colors
Color lerp_color(Color a, Color b, float t) {
    t = std::clamp(t, 0.0f, 1.0f);
    return {static_cast<unsigned char>(a.r + static_cast<int>((b.r - a.r) * t)),
            static_cast<unsigned char>(a.g + static_cast<int>((b.g - a.g) * t)),
            static_cast<unsigned char>(a.b + static_cast<int>((b.b - a.b) * t)),
            static_cast<unsigned char>(a.a + static_cast<int>((b.a - a.a) * t))};
}

/// Apply alpha modulation to a color
Color with_alpha(Color c, float alpha) {
    auto a = static_cast<unsigned char>(static_cast<float>(c.a) * std::clamp(alpha, 0.0f, 1.0f));
    return {c.r, c.g, c.b, a};
}

} // namespace

void draw_gates(const Circuit& circuit, const Layout& layout, const AnimationState& anim,
                float scale, Vector2 offset) {
    for (auto& gate_ptr : circuit.gates()) {
        const Gate* gate = gate_ptr.get();
        auto it = layout.gate_positions.find(gate);
        if (it == layout.gate_positions.end()) {
            continue;
        }

        Rectangle screen_rect = to_screen(it->second, scale, offset);
        const GateAnim& ga = anim.gate_anim(gate);

        // Determine the gate's output value in O(1)
        bool active = false;
        if (const Wire* out = gate->get_output(); out != nullptr) {
            active = out->get_value();
        }

        Color fill;
        Color outline;
        float alpha = ga.alpha;

        if (!ga.resolved) {
            // Pending: dim, pulsing
            fill = GATE_PENDING_FILL;
            outline = GATE_PENDING_OUTLINE;
        } else if (active) {
            // Resolved + active: fade from pending to green
            fill = lerp_color(GATE_PENDING_FILL, GATE_ACTIVE_FILL, alpha);
            outline = lerp_color(GATE_PENDING_OUTLINE, GATE_ACTIVE_OUTLINE, alpha);
        } else {
            // Resolved + inactive: fade from pending to gray
            fill = lerp_color(GATE_PENDING_FILL, GATE_INACTIVE_FILL, alpha);
            outline = lerp_color(GATE_PENDING_OUTLINE, GATE_INACTIVE_OUTLINE, alpha);
        }

        // Apply overall alpha for pending pulse effect
        fill = with_alpha(fill, std::max(alpha, 0.15f));
        outline = with_alpha(outline, std::max(alpha, 0.25f));

        // Draw filled rounded rectangle
        DrawRectangleRounded(screen_rect, CORNER_ROUNDNESS, CORNER_SEGMENTS, fill);
        // Draw outline
        DrawRectangleRoundedLines(screen_rect, CORNER_ROUNDNESS, CORNER_SEGMENTS, OUTLINE_THICKNESS,
                                  outline);

        // Draw gate type label centered
        auto label_sv = gate_type_name(gate->get_type());
        const char* label = label_sv.data();
        int text_width = MeasureText(label, FONT_SIZE_GATE);
        float text_x = screen_rect.x + (screen_rect.width - static_cast<float>(text_width)) / 2.0f;
        float text_y =
            screen_rect.y + (screen_rect.height - static_cast<float>(FONT_SIZE_GATE)) / 2.0f;

        Color label_color = with_alpha(LABEL_COLOR, std::max(alpha, 0.2f));
        DrawText(label, static_cast<int>(text_x), static_cast<int>(text_y), FONT_SIZE_GATE,
                 label_color);
    }
}

void draw_io_labels(const Circuit& circuit, const Layout& layout, float scale, Vector2 offset) {
    int num_inputs = static_cast<int>(circuit.input_wires().size());
    int bits = num_inputs / 2;

    // Draw input dots and labels
    for (size_t i = 0; i < layout.input_positions.size(); i++) {
        Vector2 pos = to_screen(layout.input_positions[i], scale, offset);
        DrawCircleV(pos, IO_DOT_RADIUS, IO_DOT_COLOR);

        // Label: A0, A1, ... or B0, B1, ...
        std::string label;
        if (static_cast<int>(i) < bits) {
            label = "A" + std::to_string(i);
        } else {
            label = "B" + std::to_string(static_cast<int>(i) - bits);
        }
        int text_width = MeasureText(label.c_str(), FONT_SIZE_IO);
        DrawText(label.c_str(), static_cast<int>(pos.x) - text_width / 2,
                 static_cast<int>(pos.y) - FONT_SIZE_IO - 4, FONT_SIZE_IO, INPUT_LABEL_COLOR);
    }

    // Draw output dots and labels
    int num_outputs = static_cast<int>(circuit.output_wires().size());
    for (size_t i = 0; i < layout.output_positions.size(); i++) {
        Vector2 pos = to_screen(layout.output_positions[i], scale, offset);
        DrawCircleV(pos, IO_DOT_RADIUS, IO_DOT_COLOR);

        // Label: S0, S1, ... or Cout
        std::string label;
        if (static_cast<int>(i) < num_outputs - 1) {
            label = "S" + std::to_string(i);
        } else {
            label = "Cout";
        }
        DrawText(label.c_str(),
                 static_cast<int>(pos.x) - MeasureText(label.c_str(), FONT_SIZE_IO) / 2,
                 static_cast<int>(pos.y) + 6, FONT_SIZE_IO, OUTPUT_LABEL_COLOR);
    }
}

} // namespace gateflow
