/// @file info_panel.cpp
/// @brief Implements the information panel with binary readouts and status text

#include "ui/info_panel.hpp"

#include <algorithm>
#include <cstdio>
#include <sstream>
#include <string>
#include <utility>

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

/// Draw a row of binary bits with resolved/pending coloring.
/// @param label  Row label (e.g. "A =")
/// @param value  The integer value to show in binary
/// @param bits   Number of bits
/// @param x, y   Position
/// @param scheduler  For checking which output bits are resolved (nullptr to show all resolved)
/// @param circuit    The circuit (for output wire queries, nullptr to show all resolved)
/// @param is_output  If true, color bits based on output wire resolution status
/// @param output_offset  Starting output wire index for bit resolution check
void draw_binary_row(const char* label, int value, int bits, float x, float y,
                     const PropagationScheduler* scheduler, const Circuit* circuit, bool is_output,
                     int output_offset) {
    DrawText(label, static_cast<int>(x), static_cast<int>(y), FONT_SIZE, LABEL_COLOR);

    float bit_x = x + 40.0f;
    // Draw bits MSB first (left to right = high bit to low bit)
    for (int i = bits - 1; i >= 0; i--) {
        bool bit_val = (value >> i) & 1;
        bool resolved = true;

        if (is_output && scheduler != nullptr && circuit != nullptr) {
            // Check if this output bit's wire is resolved
            int wire_idx = output_offset + i;
            if (wire_idx < static_cast<int>(circuit->output_wires().size())) {
                resolved = scheduler->is_wire_resolved(circuit->output_wires()[wire_idx]);
            }
        }

        Color bit_color;
        if (!resolved) {
            bit_color = BIT_PENDING;
        } else if (bit_val) {
            bit_color = BIT_RESOLVED_ONE;
        } else {
            bit_color = BIT_RESOLVED_ZERO;
        }

        char bit_char[2] = {bit_val ? '1' : '0', '\0'};
        if (!resolved) {
            bit_char[0] = '?';
        }
        DrawText(bit_char, static_cast<int>(bit_x), static_cast<int>(y), FONT_SIZE, bit_color);
        bit_x += 14.0f;
    }

    // Decimal value after the binary
    if (!is_output || (scheduler != nullptr && scheduler->is_complete())) {
        char dec_str[16];
        std::snprintf(dec_str, sizeof(dec_str), " = %d", value);
        DrawText(dec_str, static_cast<int>(bit_x + 4.0f), static_cast<int>(y), FONT_SIZE,
                 TEXT_COLOR);
    }
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

    // Binary A — inputs are always resolved
    draw_binary_row("A:", input_a, NUM_BITS, cx, cy, nullptr, nullptr, false, 0);
    cy += ROW_HEIGHT;

    // Binary B — inputs are always resolved
    draw_binary_row("B:", input_b, NUM_BITS, cx, cy, nullptr, nullptr, false, 0);
    cy += ROW_HEIGHT + 2.0f;

    // Separator line
    DrawLine(static_cast<int>(cx), static_cast<int>(cy),
             static_cast<int>(cx + panel_w - 2 * PADDING), static_cast<int>(cy), BORDER_COLOR);
    cy += 6.0f;

    // Binary Sum — bits resolve progressively
    draw_binary_row("S:", result, NUM_BITS + 1, cx, cy, &scheduler, &circuit, true, 0);
    cy += ROW_HEIGHT + 6.0f;

    // Decimal result (only shown when complete)
    if (scheduler.is_complete()) {
        char result_str[32];
        std::snprintf(result_str, sizeof(result_str), "%d + %d = %d", input_a, input_b, result);
        DrawText(result_str, static_cast<int>(cx), static_cast<int>(cy), FONT_SIZE_BIG,
                 RESULT_COLOR);
    }
    cy += ROW_HEIGHT + 4.0f;

    // Status text (wrapped to panel width)
    std::string status = propagation_status(scheduler);
    float text_w = panel_w - 2.0f * PADDING;
    (void)draw_wrapped_text(status, cx, cy, text_w, FONT_SIZE_SMALL, STATUS_COLOR);

    return panel_h;
}

float draw_explanation_panel(float panel_x, float panel_y, float panel_w) {
    static float scroll_target = 0.0f;
    static float scroll_current = 0.0f;

    float cx = panel_x + PADDING;
    float cy = panel_y + PADDING;
    float text_w = panel_w - 2.0f * PADDING;
    float content_top = cy + ROW_HEIGHT;
    float viewport_h = EXPLANATION_PANEL_HEIGHT - (PADDING * 2.0f + ROW_HEIGHT);

    DrawRectangleRec({panel_x, panel_y, panel_w, EXPLANATION_PANEL_HEIGHT}, BG_COLOR);
    DrawRectangleLinesEx({panel_x, panel_y, panel_w, EXPLANATION_PANEL_HEIGHT}, 1.0f,
                         BORDER_COLOR);

    DrawText("EXPLANATION", static_cast<int>(cx), static_cast<int>(cy), FONT_SIZE,
             EXPL_LABEL_COLOR);
    cy += ROW_HEIGHT;

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
