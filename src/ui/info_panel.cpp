/// @file info_panel.cpp
/// @brief Implements the information panel with binary readouts and status text

#include "ui/info_panel.hpp"

#include "simulation/gate.hpp"

#include <algorithm>
#include <cstdio>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace gateflow {

namespace {

constexpr float PADDING = 10.0f;
constexpr float ROW_HEIGHT = 23.0f;
constexpr int FONT_SIZE = 16;
constexpr int FONT_SIZE_SMALL = 14;
constexpr int FONT_SIZE_BIG = 21;
constexpr float EXPL_FONT_SIZE = 16.0f;
constexpr float EXPL_FONT_SPACING = 1.0f;
constexpr float EXPL_LINE_GAP = 3.0f;
constexpr float EXPL_PARAGRAPH_GAP = 4.0f;
constexpr float EXPL_SCROLL_SPEED = 22.0f;
constexpr float EXPL_SCROLL_SMOOTHING = 0.18f;
constexpr int NUM_BITS = 7;
constexpr float INFO_PANEL_HEIGHT = 231.0f;        // additional +10%
constexpr float EXPLANATION_PANEL_HEIGHT = 720.0f; // doubled as requested

const Color BG_COLOR = {35, 35, 42, 230};
const Color BORDER_COLOR = {70, 70, 85, 255};
const Color TEXT_COLOR = {220, 220, 230, 255};
const Color LABEL_COLOR = {160, 160, 180, 255};
const Color BIT_RESOLVED_ONE = {50, 220, 80, 255};    // Green for resolved 1
const Color BIT_RESOLVED_ZERO = {150, 150, 170, 255}; // Gray for resolved 0
const Color BIT_PENDING = {60, 60, 70, 255};          // Dim for unresolved
const Color RESULT_COLOR = {80, 220, 130, 255};
const Color STATUS_COLOR = {180, 180, 100, 255};
const Color EXPL_LABEL_COLOR = {200, 200, 220, 255};
const Color EXPL_TEXT_COLOR = {190, 190, 205, 255};
const Color CARRY_OK_COLOR = {90, 220, 120, 255};
const Color CARRY_PENDING_COLOR = {220, 200, 120, 255};

/// Draws wrapped text and returns consumed height.
float draw_wrapped_text(const std::string& text, float x, float y, float max_width, int font_size,
                        Color color, float line_gap = 2.0f) {
    std::istringstream iss(text);
    std::string word;
    std::string line;
    float cy = y;

    while (iss >> word) {
        std::string candidate = line.empty() ? word : (line + " " + word);
        if (MeasureText(candidate.c_str(), font_size) <= static_cast<int>(max_width)) {
            line = std::move(candidate);
        } else {
            if (!line.empty()) {
                DrawText(line.c_str(), static_cast<int>(x), static_cast<int>(cy), font_size, color);
                cy += static_cast<float>(font_size) + line_gap;
            }
            line = word;
        }
    }

    if (!line.empty()) {
        DrawText(line.c_str(), static_cast<int>(x), static_cast<int>(cy), font_size, color);
        cy += static_cast<float>(font_size);
    }

    return cy - y;
}

/// Draws wrapped text with DrawTextEx and returns consumed height.
float draw_wrapped_text_ex(const std::string& text, float x, float y, float max_width,
                           float font_size, float spacing, Color color,
                           float line_gap = EXPL_LINE_GAP) {
    Font font = GetFontDefault();
    std::istringstream iss(text);
    std::string word;
    std::string line;
    float cy = y;

    while (iss >> word) {
        std::string candidate = line.empty() ? word : (line + " " + word);
        if (MeasureTextEx(font, candidate.c_str(), font_size, spacing).x <= max_width) {
            line = std::move(candidate);
        } else {
            if (!line.empty()) {
                DrawTextEx(font, line.c_str(), {x, cy}, font_size, spacing, color);
                cy += font_size + line_gap;
            }
            line = word;
        }
    }

    if (!line.empty()) {
        DrawTextEx(font, line.c_str(), {x, cy}, font_size, spacing, color);
        cy += font_size;
    }

    return cy - y;
}

/// Measures wrapped text height without drawing.
float measure_wrapped_text_ex(const std::string& text, float max_width, float font_size,
                              float spacing, float line_gap = EXPL_LINE_GAP) {
    Font font = GetFontDefault();
    std::istringstream iss(text);
    std::string word;
    std::string line;
    float height = 0.0f;

    while (iss >> word) {
        std::string candidate = line.empty() ? word : (line + " " + word);
        if (MeasureTextEx(font, candidate.c_str(), font_size, spacing).x <= max_width) {
            line = std::move(candidate);
        } else {
            if (!line.empty()) {
                height += font_size + line_gap;
            }
            line = word;
        }
    }

    if (!line.empty()) {
        height += font_size;
    }

    return height;
}

std::vector<const Wire*> collect_carry_wires(const Circuit& circuit) {
    std::vector<const Wire*> carries;
    for (const auto& wire_ptr : circuit.wires()) {
        const Wire* wire = wire_ptr.get();
        const Gate* src = wire->get_source();
        if (src == nullptr) {
            continue;
        }

        if (src->get_type() == GateType::OR) {
            carries.push_back(wire);
            continue;
        }

        if (src->get_type() == GateType::AND) {
            bool has_xor_dest = false;
            bool has_and_dest = false;
            for (const Gate* dest : wire->get_destinations()) {
                if (dest->get_type() == GateType::XOR) {
                    has_xor_dest = true;
                } else if (dest->get_type() == GateType::AND) {
                    has_and_dest = true;
                }
            }
            if (has_xor_dest && has_and_dest) {
                carries.push_back(wire);
            }
        }
    }

    std::sort(carries.begin(), carries.end(), [](const Wire* a, const Wire* b) {
        const Gate* sa = a->get_source();
        const Gate* sb = b->get_source();
        if (sa == nullptr || sb == nullptr) {
            return a->get_id() < b->get_id();
        }
        return sa->get_id() < sb->get_id();
    });

    return carries;
}

std::string whats_happening_now(const PropagationScheduler& scheduler, int input_a, int input_b,
                                int result) {
    if (scheduler.current_depth() < 0.0f) {
        return "Enter two numbers (0-99) and press Run. Signals will enter each bit column and start addition at Bit 0.";
    }

    if (scheduler.is_complete()) {
        return "Complete: " + std::to_string(input_a) + " + " + std::to_string(input_b) + " = " +
               std::to_string(result) + ". All sum bits and carries are now stable.";
    }

    int depth = static_cast<int>(scheduler.current_depth());
    int approx_bit = std::min(6, std::max(0, depth / 3));
    if (depth <= 1) {
        return "Bit 0 is resolving: XOR computes the sum bit, AND computes the first carry.";
    }

    return "Carry is propagating into Bit " + std::to_string(approx_bit) +
           ". Ripple-carry adders wait for this chain, so larger adders take longer.";
}

/// Generate a human-readable status message based on current propagation depth
std::string propagation_status(const PropagationScheduler& scheduler) {
    if (scheduler.current_depth() < 0.0f) {
        return "Ready — press Run or Space to start";
    }
    if (scheduler.is_complete()) {
        return "Propagation complete";
    }

    int depth = static_cast<int>(scheduler.current_depth());
    int max_d = scheduler.max_depth();

    // The carry chain in a 7-bit RCA goes through depths roughly:
    // depth 0-1: bit 0 (half adder), depth 2-3: bit 1, etc.
    int approx_bit = depth / 3; // Rough mapping of depth to bit position
    if (approx_bit > 6) {
        approx_bit = 6;
    }

    char buf[128];
    if (depth <= 1) {
        std::snprintf(buf, sizeof(buf), "Processing bit 0 (least significant)... [%d/%d]", depth,
                      max_d);
    } else {
        std::snprintf(buf, sizeof(buf), "Carry propagating through bit %d... [%d/%d]", approx_bit,
                      depth, max_d);
    }
    return buf;
}

} // namespace

float draw_info_panel(const Circuit& circuit, const PropagationScheduler& scheduler, int input_a,
                      int input_b, int result, float panel_x, float panel_y, float panel_w) {
    float cx = panel_x + PADDING;
    float cy = panel_y + PADDING;

    // Panel background
    float panel_h = INFO_PANEL_HEIGHT;
    DrawRectangleRec({panel_x, panel_y, panel_w, panel_h}, BG_COLOR);
    DrawRectangleLinesEx({panel_x, panel_y, panel_w, panel_h}, 1.0f, BORDER_COLOR);

    // Title
    DrawText("RESULT", static_cast<int>(cx), static_cast<int>(cy), FONT_SIZE, TEXT_COLOR);
    cy += ROW_HEIGHT + 4.0f;

    // Weighted binary columns
    const int weights[NUM_BITS] = {64, 32, 16, 8, 4, 2, 1};
    float bit_x0 = cx + 56.0f;
    float bit_step = 28.0f;

    for (int i = 0; i < NUM_BITS; i++) {
        std::string w = std::to_string(weights[i]);
        DrawText(w.c_str(), static_cast<int>(bit_x0 + i * bit_step), static_cast<int>(cy),
                 FONT_SIZE_SMALL, LABEL_COLOR);
    }
    cy += ROW_HEIGHT - 2.0f;

    auto draw_bits_row = [&](const char* row_label, int value, bool output_row) {
        DrawText(row_label, static_cast<int>(cx), static_cast<int>(cy), FONT_SIZE, LABEL_COLOR);
        for (int i = 0; i < NUM_BITS; i++) {
            int bit_idx = NUM_BITS - 1 - i;
            bool bit_val = ((value >> bit_idx) & 1) != 0;
            bool resolved = true;
            if (output_row && bit_idx < static_cast<int>(circuit.output_wires().size())) {
                resolved = scheduler.is_wire_resolved(circuit.output_wires()[bit_idx]);
            }

            std::string glyph = resolved ? (bit_val ? "1" : "0") : "·";
            Color c = !resolved ? BIT_PENDING : (bit_val ? BIT_RESOLVED_ONE : BIT_RESOLVED_ZERO);
            DrawText(glyph.c_str(), static_cast<int>(bit_x0 + i * bit_step), static_cast<int>(cy),
                     FONT_SIZE, c);
        }

        if (!output_row || scheduler.is_complete()) {
            std::string dec = "= " + std::to_string(value);
            DrawText(dec.c_str(), static_cast<int>(bit_x0 + NUM_BITS * bit_step + 8),
                     static_cast<int>(cy), FONT_SIZE, TEXT_COLOR);
        }
        cy += ROW_HEIGHT;
    };

    draw_bits_row("A:", input_a, false);
    draw_bits_row("B:", input_b, false);

    // Separator line
    DrawLine(static_cast<int>(cx), static_cast<int>(cy),
             static_cast<int>(cx + panel_w - 2 * PADDING), static_cast<int>(cy), BORDER_COLOR);
    cy += 8.0f;

    draw_bits_row("S:", result, true);

    bool cout_resolved = scheduler.is_complete();
    bool cout_val = circuit.get_output(NUM_BITS);
    std::string cout_txt = std::string("Cout: ") + (cout_resolved ? (cout_val ? "1" : "0") : "·");
    DrawText(cout_txt.c_str(), static_cast<int>(cx), static_cast<int>(cy), FONT_SIZE,
             cout_resolved ? (cout_val ? BIT_RESOLVED_ONE : BIT_RESOLVED_ZERO) : BIT_PENDING);
    cy += ROW_HEIGHT;

    std::vector<const Wire*> carries = collect_carry_wires(circuit);
    if (!carries.empty()) {
        DrawText("Carry:", static_cast<int>(cx), static_cast<int>(cy), FONT_SIZE_SMALL,
                 LABEL_COLOR);
        float ccx = cx + 60.0f;
        for (size_t i = 0; i < carries.size(); i++) {
            bool resolved = scheduler.is_wire_resolved(carries[i]);
            std::string token = "C" + std::to_string(i) + (resolved ? " ✓" : " →");
            DrawText(token.c_str(), static_cast<int>(ccx), static_cast<int>(cy), FONT_SIZE_SMALL,
                     resolved ? CARRY_OK_COLOR : CARRY_PENDING_COLOR);
            ccx += static_cast<float>(MeasureText(token.c_str(), FONT_SIZE_SMALL)) + 8.0f;
            if (ccx > panel_x + panel_w - 80.0f) {
                ccx = cx + 60.0f;
                cy += ROW_HEIGHT - 4.0f;
            }
        }
        cy += ROW_HEIGHT - 2.0f;
    }

    // Decimal result (only shown when complete)
    if (scheduler.is_complete()) {
        char result_str[32];
        std::snprintf(result_str, sizeof(result_str), "%d + %d = %d", input_a, input_b, result);
        DrawText(result_str, static_cast<int>(cx), static_cast<int>(cy), FONT_SIZE_BIG,
                 RESULT_COLOR);
    }
    cy += ROW_HEIGHT + 2.0f;

    // Status text (wrapped to panel width)
    std::string status = propagation_status(scheduler);
    float text_w = panel_w - 2.0f * PADDING;
    (void)draw_wrapped_text(status, cx, cy, text_w, FONT_SIZE_SMALL, STATUS_COLOR);

    return panel_h;
}

float draw_explanation_panel(float panel_x, float panel_y, float panel_w,
                             const PropagationScheduler& scheduler, int input_a, int input_b,
                             int result) {
    static float scroll_target = 0.0f;
    static float scroll_current = 0.0f;
    static bool show_reference = true;

    float cx = panel_x + PADDING;
    float cy = panel_y + PADDING;
    float text_w = panel_w - 2.0f * PADDING;
    float viewport_h = EXPLANATION_PANEL_HEIGHT - (PADDING * 2.0f + ROW_HEIGHT + 110.0f);

    DrawRectangleRec({panel_x, panel_y, panel_w, EXPLANATION_PANEL_HEIGHT}, BG_COLOR);
    DrawRectangleLinesEx({panel_x, panel_y, panel_w, EXPLANATION_PANEL_HEIGHT}, 1.0f,
                         BORDER_COLOR);

    DrawText("EXPLANATION", static_cast<int>(cx), static_cast<int>(cy), FONT_SIZE,
             EXPL_LABEL_COLOR);
    cy += ROW_HEIGHT;

    // Section A: dynamic context
    DrawText("What's happening now", static_cast<int>(cx), static_cast<int>(cy), FONT_SIZE,
             {220, 220, 130, 255});
    cy += ROW_HEIGHT - 4.0f;
    cy += draw_wrapped_text_ex(whats_happening_now(scheduler, input_a, input_b, result), cx, cy,
                               text_w, EXPL_FONT_SIZE + 1.0f, EXPL_FONT_SPACING,
                               {225, 225, 205, 255}, EXPL_LINE_GAP);
    cy += 8.0f;

    Rectangle toggle_rect = {cx, cy, text_w, 22.0f};
    if (CheckCollisionPointRec(GetMousePosition(), toggle_rect) &&
        IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        show_reference = !show_reference;
    }
    std::string toggle = std::string(show_reference ? "▼ " : "▶ ") + "How it works";
    DrawText(toggle.c_str(), static_cast<int>(cx), static_cast<int>(cy), FONT_SIZE,
             {180, 205, 240, 255});
    cy += ROW_HEIGHT;

    if (!show_reference) {
        return EXPLANATION_PANEL_HEIGHT;
    }

    float content_top = cy;

    const std::string lines[] = {
        "What you are seeing: an adder built from logic gates. Signals travel from inputs to outputs over time.",
        "A0..A6 are bits of the first number and B0..B6 are bits of the second number (0 = low, 1 = high).",
        "Bit 0 is the least-significant bit (LSB). It is the first place where addition starts.",
        "XOR means 'exclusive OR': it is 1 when exactly one input is 1. This gives each sum bit.",
        "AND is 1 only when both inputs are 1. This creates a carry when a bit position overflows.",
        "OR combines carry paths. NAND means NOT(AND) and can be used to build all other gates.",
        "Cout means 'carry out': the extra bit produced when the top bit overflows.",
        "Animation guide: gray/dim = not resolved yet, green = resolved 1, gray steady = resolved 0.",
        "As time advances, carry moves across bits. That carry chain is why some additions take longer.",
        "S0..S6 are sum output bits. When propagation completes, the final decimal answer is stable.",
    };

    float content_h = 0.0f;
    for (const auto& line : lines) {
        content_h += measure_wrapped_text_ex(line, text_w, EXPL_FONT_SIZE, EXPL_FONT_SPACING,
                                             EXPL_LINE_GAP);
        content_h += EXPL_PARAGRAPH_GAP;
    }

    float max_scroll = std::max(0.0f, content_h - viewport_h);

    Rectangle panel_rect = {panel_x, panel_y, panel_w, EXPLANATION_PANEL_HEIGHT};
    Vector2 mouse = GetMousePosition();
    bool hovered = CheckCollisionPointRec(mouse, panel_rect);

    if (hovered) {
        float wheel = GetMouseWheelMove();
        if (wheel != 0.0f) {
            scroll_target -= wheel * EXPL_SCROLL_SPEED;
        }
    }

    scroll_target = std::clamp(scroll_target, 0.0f, max_scroll);
    scroll_current += (scroll_target - scroll_current) * EXPL_SCROLL_SMOOTHING;
    scroll_current = std::clamp(scroll_current, 0.0f, max_scroll);

    BeginScissorMode(static_cast<int>(panel_x + PADDING), static_cast<int>(content_top),
                     static_cast<int>(text_w), static_cast<int>(viewport_h));

    float draw_y = content_top - scroll_current;
    for (const auto& line : lines) {
        draw_y += draw_wrapped_text_ex(line, cx, draw_y, text_w, EXPL_FONT_SIZE, EXPL_FONT_SPACING,
                                   EXPL_TEXT_COLOR, EXPL_LINE_GAP);
        draw_y += EXPL_PARAGRAPH_GAP;
    }

    EndScissorMode();

    // Subtle scrollbar indicator when content exceeds viewport.
    if (max_scroll > 0.0f) {
        float track_x = panel_x + panel_w - 6.0f;
        float track_y = content_top;
        float track_h = viewport_h;
        DrawRectangle(static_cast<int>(track_x), static_cast<int>(track_y), 2,
                      static_cast<int>(track_h), {80, 80, 95, 180});

        float thumb_h = std::max(20.0f, track_h * (viewport_h / content_h));
        float thumb_y = track_y + (track_h - thumb_h) *
                                      (max_scroll > 0.0f ? (scroll_current / max_scroll) : 0.0f);
        DrawRectangle(static_cast<int>(track_x), static_cast<int>(thumb_y), 2,
                      static_cast<int>(thumb_h), {150, 150, 170, 210});
    }

    return EXPLANATION_PANEL_HEIGHT;
}

} // namespace gateflow
