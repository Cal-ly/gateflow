/// @file info_panel.cpp
/// @brief Implements the information panel with binary readouts and status text

#include "ui/info_panel.hpp"

#include <cstdio>
#include <string>

namespace gateflow {

namespace {

constexpr float PADDING = 10.0f;
constexpr float ROW_HEIGHT = 22.0f;
constexpr int FONT_SIZE = 15;
constexpr int FONT_SIZE_SMALL = 13;
constexpr int FONT_SIZE_BIG = 20;
constexpr int NUM_BITS = 7;

const Color BG_COLOR = {35, 35, 42, 230};
const Color BORDER_COLOR = {70, 70, 85, 255};
const Color TEXT_COLOR = {220, 220, 230, 255};
const Color LABEL_COLOR = {160, 160, 180, 255};
const Color BIT_RESOLVED_ONE = {50, 220, 80, 255};    // Green for resolved 1
const Color BIT_RESOLVED_ZERO = {150, 150, 170, 255}; // Gray for resolved 0
const Color BIT_PENDING = {60, 60, 70, 255};          // Dim for unresolved
const Color RESULT_COLOR = {80, 220, 130, 255};
const Color STATUS_COLOR = {180, 180, 100, 255};

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

void draw_info_panel(const Circuit& circuit, const PropagationScheduler& scheduler, int input_a,
                     int input_b, int result, float panel_x, float panel_y, float panel_w) {
    float cx = panel_x + PADDING;
    float cy = panel_y + PADDING;

    // Panel background
    float panel_h = 200.0f;
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

    // Status text
    std::string status = propagation_status(scheduler);
    DrawText(status.c_str(), static_cast<int>(cx), static_cast<int>(cy), FONT_SIZE_SMALL,
             STATUS_COLOR);
}

} // namespace gateflow
