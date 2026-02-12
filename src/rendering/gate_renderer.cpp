/// @file gate_renderer.cpp
/// @brief Draws gates as rounded rectangles with type labels and animated state coloring

#include "rendering/gate_renderer.hpp"

#include "rendering/app_font.hpp"
#include "simulation/gate.hpp"
#include "simulation/wire.hpp"

#include <algorithm>
#include <cmath>
#include <map>
#include <string>
#include <vector>

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
const Color GROUP_BG_COLOR = {38, 38, 55, 170};           // subtle column grouping background
const Color GROUP_BORDER_COLOR = {58, 58, 78, 220};

const Color ACCENT_XOR = {70, 210, 220, 255};
const Color ACCENT_AND = {255, 165, 70, 255};
const Color ACCENT_OR = {240, 220, 90, 255};
const Color ACCENT_NAND = {170, 120, 255, 255};
const Color ACCENT_OTHER = {140, 140, 170, 255};
const Color TOOLTIP_BG = {24, 24, 32, 245};
const Color TOOLTIP_BORDER = {95, 95, 120, 255};
const Color TOOLTIP_TITLE = {226, 226, 236, 255};
const Color TOOLTIP_BODY = {190, 208, 228, 255};
const Color TOOLTIP_ROW = {170, 170, 185, 255};
const Color TOOLTIP_ROW_ACTIVE = {255, 225, 145, 255};

constexpr float CORNER_ROUNDNESS = 0.3f; // Raylib roundness parameter (0.0–1.0)
constexpr int CORNER_SEGMENTS = 4;
constexpr float OUTLINE_THICKNESS = 2.0f;
constexpr int FONT_SIZE_GATE = 19;
constexpr int FONT_SIZE_IO = 19;
constexpr float IO_DOT_RADIUS = 4.0f;
constexpr float GROUP_MARGIN = 0.6f;

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

Color gate_type_accent(GateType type) {
    switch (type) {
    case GateType::XOR:
        return ACCENT_XOR;
    case GateType::AND:
        return ACCENT_AND;
    case GateType::OR:
        return ACCENT_OR;
    case GateType::NAND:
        return ACCENT_NAND;
    case GateType::NOT:
    case GateType::BUFFER:
        return ACCENT_OTHER;
    }
    return ACCENT_OTHER;
}

int rounded_x_bucket(float x) {
    return static_cast<int>(std::round(x * 100.0f));
}

} // namespace

void draw_adder_groups(const Circuit& circuit, const Layout& layout, float scale, Vector2 offset) {
    if (circuit.num_inputs() % 2 != 0 || circuit.num_outputs() != circuit.num_inputs() / 2 + 1) {
        return;
    }

    std::map<int, Rect> column_bounds;
    for (const auto& [gate, rect] : layout.gate_positions) {
        (void)gate;
        int bucket = rounded_x_bucket(rect.x);
        auto it = column_bounds.find(bucket);
        if (it == column_bounds.end()) {
            column_bounds[bucket] = rect;
        } else {
            Rect& b = it->second;
            float min_x = std::min(b.x, rect.x);
            float min_y = std::min(b.y, rect.y);
            float max_x = std::max(b.x + b.w, rect.x + rect.w);
            float max_y = std::max(b.y + b.h, rect.y + rect.h);
            b = {min_x, min_y, max_x - min_x, max_y - min_y};
        }
    }

    if (column_bounds.empty()) {
        return;
    }

    // Sort columns right-to-left for bit indexing (Bit 0 at rightmost).
    std::vector<Rect> columns;
    columns.reserve(column_bounds.size());
    for (const auto& [bucket, rect] : column_bounds) {
        (void)bucket;
        columns.push_back(rect);
    }
    std::sort(columns.begin(), columns.end(), [](const Rect& a, const Rect& b) { return a.x > b.x; });

    for (size_t bit = 0; bit < columns.size(); bit++) {
        Rect r = columns[bit];
        r.x -= GROUP_MARGIN;
        r.y -= (GROUP_MARGIN + 1.0f);
        r.w += GROUP_MARGIN * 2.0f;
        r.h += GROUP_MARGIN * 2.0f + 1.8f;

        Rectangle sr = to_screen(r, scale, offset);
        DrawRectangleRounded(sr, 0.15f, 4, GROUP_BG_COLOR);
        DrawRectangleRoundedLines(sr, 0.15f, 4, 1.0f, GROUP_BORDER_COLOR);

        std::string label = "Bit " + std::to_string(bit);
        DrawAppText(label.c_str(), static_cast<int>(sr.x + 6), static_cast<int>(sr.y + 4), 14,
                 {180, 180, 200, 240});
    }

    // Overflow (Cout) group near the final output pin.
    if (!layout.output_positions.empty()) {
        Vec2 cout_pos = layout.output_positions.back();
        Rect o = {cout_pos.x - 1.7f, cout_pos.y - 1.8f, 3.4f, 2.6f};
        Rectangle so = to_screen(o, scale, offset);

        // Highlight amber when Cout = 1 (overflow), otherwise neutral.
        int num_outputs = static_cast<int>(circuit.output_wires().size());
        bool cout_active = (num_outputs > 0) && circuit.output_wires().back()->get_value();

        Color bg = cout_active ? Color{65, 55, 30, 190} : Color{45, 45, 60, 170};
        Color border = cout_active ? Color{245, 190, 70, 240} : Color{80, 80, 110, 220};
        Color label_col = cout_active ? Color{245, 200, 100, 255} : Color{210, 190, 130, 255};

        DrawRectangleRounded(so, 0.2f, 4, bg);
        DrawRectangleRoundedLines(so, 0.2f, 4, 1.0f, border);
        DrawAppText("Cout", static_cast<int>(so.x + 4), static_cast<int>(so.y + 3), 14,
                 label_col);
        if (cout_active) {
            DrawAppText("overflow", static_cast<int>(so.x + 4), static_cast<int>(so.y + 18), 10,
                     {255, 180, 80, 255});
        }
    }
}

void draw_gates(const Circuit& circuit, const Layout& layout, const AnimationState& anim,
                float scale, Vector2 offset) {
    const Gate* hovered_gate = nullptr;
    Rectangle hovered_rect = {0, 0, 0, 0};

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

        // Draw gate-type accent stripe for quick visual differentiation.
        Color accent = with_alpha(gate_type_accent(gate->get_type()), std::max(alpha, 0.45f));
        DrawRectangle(static_cast<int>(screen_rect.x), static_cast<int>(screen_rect.y), 4,
                  static_cast<int>(screen_rect.height), accent);

        // Draw gate type label centered
        auto label_sv = gate_type_name(gate->get_type());
        const char* label = label_sv.data();
        int text_width = MeasureAppText(label, FONT_SIZE_GATE);
        float text_x = screen_rect.x + (screen_rect.width - static_cast<float>(text_width)) / 2.0f;
        float text_y =
            screen_rect.y + (screen_rect.height - static_cast<float>(FONT_SIZE_GATE)) / 2.0f;

        Color label_color = with_alpha(LABEL_COLOR, std::max(alpha, 0.2f));
        DrawAppText(label, static_cast<int>(text_x), static_cast<int>(text_y), FONT_SIZE_GATE,
                 label_color);

        if (CheckCollisionPointRec(GetMousePosition(), screen_rect)) {
            hovered_gate = gate;
            hovered_rect = screen_rect;
        }
    }

    if (hovered_gate != nullptr) {
        std::vector<bool> in_vals;
        in_vals.reserve(hovered_gate->get_inputs().size());
        for (const Wire* w : hovered_gate->get_inputs()) {
            in_vals.push_back(w->get_value());
        }
        bool out_val = false;
        if (const Wire* out = hovered_gate->get_output(); out != nullptr) {
            out_val = out->get_value();
        }

        std::vector<std::pair<std::string, bool>> rows;
        auto add_row = [&](std::string row, bool highlight) {
            rows.emplace_back(std::move(row), highlight);
        };

        bool a = in_vals.size() >= 1 ? in_vals[0] : false;
        bool b = in_vals.size() >= 2 ? in_vals[1] : false;
        switch (hovered_gate->get_type()) {
        case GateType::XOR:
            add_row("0,0 -> 0", !a && !b);
            add_row("0,1 -> 1", !a && b);
            add_row("1,0 -> 1", a && !b);
            add_row("1,1 -> 0", a && b);
            break;
        case GateType::AND:
            add_row("0,0 -> 0", !a && !b);
            add_row("0,1 -> 0", !a && b);
            add_row("1,0 -> 0", a && !b);
            add_row("1,1 -> 1", a && b);
            break;
        case GateType::OR:
            add_row("0,0 -> 0", !a && !b);
            add_row("0,1 -> 1", !a && b);
            add_row("1,0 -> 1", a && !b);
            add_row("1,1 -> 1", a && b);
            break;
        case GateType::NAND:
            add_row("0,0 -> 1", !a && !b);
            add_row("0,1 -> 1", !a && b);
            add_row("1,0 -> 1", a && !b);
            add_row("1,1 -> 0", a && b);
            break;
        case GateType::NOT:
            add_row("0 -> 1", !a);
            add_row("1 -> 0", a);
            break;
        case GateType::BUFFER:
            add_row("0 -> 0", !a);
            add_row("1 -> 1", a);
            break;
        }

        const float tip_width = 294.0f;
        const float tip_height = 62.0f + static_cast<float>(rows.size()) * 16.0f;
        Rectangle tip = {hovered_rect.x + hovered_rect.width + 10.0f, hovered_rect.y - 6.0f,
                         tip_width, tip_height};

        if (tip.x + tip.width > static_cast<float>(GetScreenWidth()) - 6.0f) {
            tip.x = hovered_rect.x - tip.width - 10.0f;
        }
        tip.x = std::clamp(tip.x, 6.0f, static_cast<float>(GetScreenWidth()) - tip.width - 6.0f);
        tip.y = std::clamp(tip.y, 6.0f,
                           static_cast<float>(GetScreenHeight()) - tip.height - 6.0f);

        Rectangle shadow = {tip.x + 2.0f, tip.y + 3.0f, tip.width, tip.height};
        DrawRectangleRounded(shadow, 0.16f, 4, {0, 0, 0, 100});
        DrawRectangleRounded(tip, 0.16f, 4, TOOLTIP_BG);
        DrawRectangleRoundedLines(tip, 0.16f, 4, 1.0f, TOOLTIP_BORDER);

        Color accent = gate_type_accent(hovered_gate->get_type());
        DrawRectangle(static_cast<int>(tip.x), static_cast<int>(tip.y), static_cast<int>(tip.width),
                      18, with_alpha(accent, 0.20f));
        DrawRectangle(static_cast<int>(tip.x), static_cast<int>(tip.y), static_cast<int>(tip.width),
                      3, with_alpha(accent, 0.85f));

        std::string title = std::string(gate_type_name(hovered_gate->get_type())) + " gate";
        DrawAppText(title.c_str(), static_cast<int>(tip.x + 10), static_cast<int>(tip.y + 8), 14,
                 TOOLTIP_TITLE);

        std::string io = "in: ";
        for (size_t i = 0; i < in_vals.size(); i++) {
            io += (in_vals[i] ? '1' : '0');
            if (i + 1 < in_vals.size()) {
                io += ", ";
            }
        }
        io += "   out: ";
        io += out_val ? '1' : '0';
        DrawAppText(io.c_str(), static_cast<int>(tip.x + 10), static_cast<int>(tip.y + 29), 13,
                 TOOLTIP_BODY);

        DrawLine(static_cast<int>(tip.x + 8), static_cast<int>(tip.y + 46),
                 static_cast<int>(tip.x + tip.width - 8), static_cast<int>(tip.y + 46),
                 with_alpha(accent, 0.35f));

        int y = static_cast<int>(tip.y + 50);
        for (const auto& [text, highlight] : rows) {
            if (highlight) {
                DrawRectangle(static_cast<int>(tip.x + 8), y - 1, static_cast<int>(tip.width - 16),
                              14, with_alpha(accent, 0.28f));
            }
            DrawAppText(text.c_str(), static_cast<int>(tip.x + 12), y, 12,
                     highlight ? TOOLTIP_ROW_ACTIVE : TOOLTIP_ROW);
            y += 16;
        }
    }
}

void draw_io_labels(const Circuit& circuit, const Layout& layout, float scale, Vector2 offset) {
    int num_inputs = static_cast<int>(circuit.input_wires().size());
    int bits = num_inputs / 2;

    // Slightly smaller font for input labels to avoid overlap, but readable.
    constexpr int INPUT_FONT = 17;

    // Draw input dots and labels.
    // A[i] and B[i] share a column and are close together, so we right-align
    // A labels (text extends left of the dot) and left-align B labels (text
    // extends right of the dot) to prevent overlap.
    for (size_t i = 0; i < layout.input_positions.size(); i++) {
        Vector2 pos = to_screen(layout.input_positions[i], scale, offset);
        DrawCircleV(pos, IO_DOT_RADIUS, IO_DOT_COLOR);

        std::string label;
        bool is_a = static_cast<int>(i) < bits;
        if (is_a) {
            bool bit = circuit.input_wires()[i]->get_value();
            label = "A" + std::to_string(i) + ":" + (bit ? "1" : "0");
        } else {
            size_t bi = i - static_cast<size_t>(bits);
            bool bit = circuit.input_wires()[i]->get_value();
            label = "B" + std::to_string(bi) + ":" + (bit ? "1" : "0");
        }

        int text_width = MeasureAppText(label.c_str(), INPUT_FONT);
        int label_y = static_cast<int>(pos.y) - INPUT_FONT - 4;

        if (is_a) {
            // Right-align: text ends at the dot
            DrawAppText(label.c_str(), static_cast<int>(pos.x) - text_width - 2,
                        label_y, INPUT_FONT, INPUT_LABEL_COLOR);
        } else {
            // Left-align: text starts at the dot
            DrawAppText(label.c_str(), static_cast<int>(pos.x) + 2,
                        label_y, INPUT_FONT, INPUT_LABEL_COLOR);
        }
    }

    // Draw output dots and labels
    int num_outputs = static_cast<int>(circuit.output_wires().size());
    for (size_t i = 0; i < layout.output_positions.size(); i++) {
        Vector2 pos = to_screen(layout.output_positions[i], scale, offset);
        DrawCircleV(pos, IO_DOT_RADIUS, IO_DOT_COLOR);

        // Label: S0, S1, ... or Cout
        std::string label;
        if (static_cast<int>(i) < num_outputs - 1) {
            bool bit = circuit.output_wires()[i]->get_value();
            label = "S" + std::to_string(i) + ": " + (bit ? "1" : "0");
        } else {
            bool bit = circuit.output_wires()[i]->get_value();
            label = std::string("Cout: ") + (bit ? "1" : "0");
        }
        DrawAppText(label.c_str(),
                 static_cast<int>(pos.x) - MeasureAppText(label.c_str(), FONT_SIZE_IO) / 2,
                 static_cast<int>(pos.y) + 6, FONT_SIZE_IO, OUTPUT_LABEL_COLOR);
    }
}

} // namespace gateflow
