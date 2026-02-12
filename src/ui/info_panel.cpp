/// @file info_panel.cpp
/// @brief Implements the information panel with binary readouts and status text

#include "ui/info_panel.hpp"

#include "rendering/app_font.hpp"
#include "simulation/gate.hpp"
#include "ui/ui_scale.hpp"

#include <algorithm>
#include <cstdio>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace gateflow {

namespace {

constexpr float EXPL_FONT_SIZE = 16.0f;
constexpr float EXPL_FONT_SPACING = 1.0f;
constexpr float EXPL_LINE_GAP = 3.0f;
constexpr float EXPL_PARAGRAPH_GAP = 4.0f;
constexpr float EXPL_SCROLL_SPEED = 22.0f;
constexpr float EXPL_SCROLL_SMOOTHING = 0.18f;
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
        if (MeasureAppText(candidate.c_str(), font_size) <= static_cast<int>(max_width)) {
            line = std::move(candidate);
        } else {
            if (!line.empty()) {
                DrawAppText(line.c_str(), static_cast<int>(x), static_cast<int>(cy), font_size, color);
                cy += static_cast<float>(font_size) + line_gap;
            }
            line = word;
        }
    }

    if (!line.empty()) {
        DrawAppText(line.c_str(), static_cast<int>(x), static_cast<int>(cy), font_size, color);
        cy += static_cast<float>(font_size);
    }

    return cy - y;
}

/// Draws wrapped text with DrawTextEx and returns consumed height.
float draw_wrapped_text_ex(const std::string& text, float x, float y, float max_width,
                           float font_size, float spacing, Color color,
                           float line_gap = EXPL_LINE_GAP) {
    Font font = get_app_font();
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
    Font font = get_app_font();
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
    const auto& sc = ui_scale();
    float PADDING = sc.padding;
    float ROW_HEIGHT = sc.row_height;
    int FONT_SIZE = sc.font_normal;
    int FONT_SIZE_SMALL = sc.font_small;
    int FONT_SIZE_BIG = sc.font_big;

    float cx = panel_x + PADDING;
    float cy = panel_y + PADDING;

    // --- Pre-compute panel height so background can be drawn first ---
    // Title + weight header + A row + B row + separator + S row + Cout +
    // carry row(s) + decimal result + status.  Use generous estimate.
    float est_h = PADDING;                       // top padding
    est_h += ROW_HEIGHT + 4.0f;                  // title
    est_h += ROW_HEIGHT - 2.0f;                  // weight header
    est_h += ROW_HEIGHT * 3.0f;                  // A, B, S rows
    est_h += 8.0f;                               // separator gap
    est_h += ROW_HEIGHT;                         // Cout
    est_h += ROW_HEIGHT * 2.0f;                  // carry (up to 2 lines)
    est_h += ROW_HEIGHT + 2.0f;                  // decimal result
    est_h += static_cast<float>(FONT_SIZE_SMALL) + 4.0f; // status
    est_h += PADDING;                            // bottom padding
    float panel_h = est_h;

    // Draw background first so content renders on top
    DrawRectangleRec({panel_x, panel_y, panel_w, panel_h}, BG_COLOR);
    DrawRectangleLinesEx({panel_x, panel_y, panel_w, panel_h}, 1.0f, BORDER_COLOR);

    // Title
    DrawAppText("RESULT", static_cast<int>(cx), static_cast<int>(cy), FONT_SIZE, TEXT_COLOR);
    cy += ROW_HEIGHT + 4.0f;

    // Weighted binary columns
    const int weights[NUM_BITS] = {64, 32, 16, 8, 4, 2, 1};
    float bit_x0 = cx + 56.0f;
    float bit_step = 28.0f;

    for (int i = 0; i < NUM_BITS; i++) {
        std::string w = std::to_string(weights[i]);
        DrawAppText(w.c_str(), static_cast<int>(bit_x0 + i * bit_step), static_cast<int>(cy),
                 FONT_SIZE_SMALL, LABEL_COLOR);
    }
    cy += ROW_HEIGHT - 2.0f;

    auto draw_bits_row = [&](const char* row_label, int value, bool output_row) {
        DrawAppText(row_label, static_cast<int>(cx), static_cast<int>(cy), FONT_SIZE, LABEL_COLOR);
        for (int i = 0; i < NUM_BITS; i++) {
            int bit_idx = NUM_BITS - 1 - i;
            bool bit_val = ((value >> bit_idx) & 1) != 0;
            bool resolved = true;
            if (output_row && bit_idx < static_cast<int>(circuit.output_wires().size())) {
                resolved = scheduler.is_wire_resolved(circuit.output_wires()[bit_idx]);
            }

            std::string glyph = resolved ? (bit_val ? "1" : "0") : "-";
            Color c = !resolved ? BIT_PENDING : (bit_val ? BIT_RESOLVED_ONE : BIT_RESOLVED_ZERO);
            DrawAppText(glyph.c_str(), static_cast<int>(bit_x0 + i * bit_step), static_cast<int>(cy),
                     FONT_SIZE, c);
        }

        if (!output_row || scheduler.is_complete()) {
            std::string dec = "= " + std::to_string(value);
            DrawAppText(dec.c_str(), static_cast<int>(bit_x0 + NUM_BITS * bit_step + 8),
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
    std::string cout_txt = std::string("Cout: ") + (cout_resolved ? (cout_val ? "1" : "0") : "-");
    DrawAppText(cout_txt.c_str(), static_cast<int>(cx), static_cast<int>(cy), FONT_SIZE,
             cout_resolved ? (cout_val ? BIT_RESOLVED_ONE : BIT_RESOLVED_ZERO) : BIT_PENDING);
    cy += ROW_HEIGHT;

    std::vector<const Wire*> carries = collect_carry_wires(circuit);
    if (!carries.empty()) {
        DrawAppText("Carry:", static_cast<int>(cx), static_cast<int>(cy), FONT_SIZE_SMALL,
                 LABEL_COLOR);
        float ccx = cx + 60.0f;
        constexpr float DOT_RADIUS = 5.0f;
        constexpr float DOT_SPACING = 28.0f;
        const Color CARRY_AMBER  = {245, 190, 70, 255};
        const Color CARRY_GRAY   = {110, 110, 130, 255};
        const Color CARRY_DIM    = {55, 55, 65, 255};
        float dot_cy = cy + static_cast<float>(FONT_SIZE_SMALL) / 2.0f;
        for (size_t i = 0; i < carries.size(); i++) {
            bool resolved = scheduler.is_wire_resolved(carries[i]);
            bool val = carries[i]->get_value();
            Color dot_color = !resolved ? CARRY_DIM
                            : (val ? CARRY_AMBER : CARRY_GRAY);
            DrawCircle(static_cast<int>(ccx), static_cast<int>(dot_cy),
                       DOT_RADIUS, dot_color);
            // Small label below each dot
            std::string clabel = "C" + std::to_string(i);
            int lw = MeasureAppText(clabel.c_str(), FONT_SIZE_SMALL - 3);
            DrawAppText(clabel.c_str(),
                     static_cast<int>(ccx) - lw / 2,
                     static_cast<int>(dot_cy + DOT_RADIUS + 2),
                     FONT_SIZE_SMALL - 3, LABEL_COLOR);
            ccx += DOT_SPACING;
        }
        cy += ROW_HEIGHT + 6.0f;
    }

    // Decimal result (only shown when complete)
    if (scheduler.is_complete()) {
        char result_str[32];
        std::snprintf(result_str, sizeof(result_str), "%d + %d = %d", input_a, input_b, result);
        DrawAppText(result_str, static_cast<int>(cx), static_cast<int>(cy), FONT_SIZE_BIG,
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
                             int result, float available_h) {
    const auto& sc = ui_scale();
    float PADDING = sc.padding;
    float ROW_HEIGHT = sc.row_height;
    int FONT_SIZE = sc.font_normal;

    float panel_h = std::max(available_h, 200.0f);

    static float scroll_target = 0.0f;
    static float scroll_current = 0.0f;
    static bool show_logic_gates = false;
    static bool show_ripple_carry = false;
    static bool show_reading_anim = false;

    const Color HEADER_COLOR = {180, 205, 240, 255};
    const Color ACCENT_XOR  = {100, 220, 220, 255};
    const Color ACCENT_AND  = {240, 170,  80, 255};
    const Color ACCENT_OR   = {240, 220, 100, 255};
    const Color ACCENT_NAND = {200, 140, 220, 255};

    float cx = panel_x + PADDING;
    float text_w = panel_w - 2.0f * PADDING;
    constexpr float INDENT = 10.0f;
    float ind_x = cx + INDENT;
    float ind_w = text_w - INDENT;

    float fs = EXPL_FONT_SIZE;
    float sp = EXPL_FONT_SPACING;
    float lg = EXPL_LINE_GAP;
    float pg = EXPL_PARAGRAPH_GAP;

    Vector2 mouse = GetMousePosition();

    // --- Draw background ---
    DrawRectangleRec({panel_x, panel_y, panel_w, panel_h}, BG_COLOR);
    DrawRectangleLinesEx({panel_x, panel_y, panel_w, panel_h}, 1.0f, BORDER_COLOR);

    float cy = panel_y + PADDING;

    // Title
    DrawAppText("EXPLANATION", static_cast<int>(cx), static_cast<int>(cy), FONT_SIZE,
                EXPL_LABEL_COLOR);
    cy += ROW_HEIGHT;

    // "What's happening now" — always visible, not scrolled
    DrawAppText("What's happening now", static_cast<int>(cx), static_cast<int>(cy), FONT_SIZE,
                {220, 220, 130, 255});
    cy += ROW_HEIGHT - 4.0f;
    cy += draw_wrapped_text_ex(whats_happening_now(scheduler, input_a, input_b, result),
                               cx, cy, text_w, fs + 1.0f, sp, {225, 225, 205, 255}, lg);
    cy += 8.0f;

    // --- Scrollable content region ---
    float content_top = cy;
    float viewport_h = panel_h - (content_top - panel_y) - PADDING;
    if (viewport_h < 20.0f) {
        return panel_h;
    }

    // Gate reference data
    struct GateEntry {
        const char* name;
        Color color;
        const char* desc;
        const char* truth;
    };
    const GateEntry gates[] = {
        {"XOR",  ACCENT_XOR,
         "Output is 1 when exactly one input is 1. Computes sum bits.",
         "0,0->0  0,1->1  1,0->1  1,1->0"},
        {"AND",  ACCENT_AND,
         "Output is 1 only when both inputs are 1. Detects carries.",
         "0,0->0  0,1->0  1,0->0  1,1->1"},
        {"OR",   ACCENT_OR,
         "Output is 1 when at least one input is 1. Combines carry paths.",
         "0,0->0  0,1->1  1,0->1  1,1->1"},
        {"NAND", ACCENT_NAND,
         "Inverse of AND. Any gate can be built from NAND alone.",
         "0,0->1  0,1->1  1,0->1  1,1->0"},
    };

    const char* rca_paras[] = {
        "Adds two binary numbers like decimal addition -- one column "
        "at a time, right to left, carrying overflow.",
        "Each column uses a full adder: 5 gates (2 XOR, 2 AND, 1 OR) "
        "producing a sum bit and carry-out.",
        "The carry-out of each adder feeds the next. This amber chain "
        "is the critical path -- the carry must 'ripple' through every bit.",
        "This delay is why real CPUs use carry-lookahead adders. The "
        "ripple-carry design makes the process visible.",
        "If the final carry-out (Cout) is 1, the result exceeds 7 bits "
        "-- overflow. Try 99 + 99.",
    };

    const char* legend_items[] = {
        "Green fill = output is 1 (active)",
        "Gray fill = output is 0 (resolved)",
        "Dim/translucent = not yet resolved",
        "Amber wire = carry chain",
        "Green wire = signal carrying 1",
        "Dark wire = signal carrying 0",
    };

    // Unified render lambda: do_draw=false measures only, true draws.
    auto render_sections = [&](float start_y, bool do_draw) -> float {
        float y = start_y;

        auto wrapped = [&](const char* text, Color col = EXPL_TEXT_COLOR) {
            if (do_draw) {
                y += draw_wrapped_text_ex(text, ind_x, y, ind_w, fs, sp, col, lg);
            } else {
                y += measure_wrapped_text_ex(text, ind_w, fs, sp, lg);
            }
        };

        auto label_line = [&](const char* text, int sz, Color col) {
            if (do_draw) {
                DrawAppText(text, static_cast<int>(ind_x), static_cast<int>(y), sz, col);
            }
            y += static_cast<float>(sz) + 2.0f;
        };

        auto tt_line = [&](const char* text) {
            if (do_draw) {
                DrawTextEx(get_app_font(), text, {ind_x, y}, fs, sp, LABEL_COLOR);
            }
            y += fs;
        };

        auto section_header = [&](const char* title, bool& expanded) {
            if (do_draw) {
                std::string lbl = std::string(expanded ? "v " : "> ") + title;
                Rectangle hit = {cx, y, text_w, ROW_HEIGHT};
                if (CheckCollisionPointRec(mouse, hit)) {
                    DrawRectangleRec(hit, {255, 255, 255, 12});
                    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                        expanded = !expanded;
                    }
                }
                DrawAppText(lbl.c_str(), static_cast<int>(cx), static_cast<int>(y),
                            FONT_SIZE, HEADER_COLOR);
            }
            y += ROW_HEIGHT;
        };

        // --- Section 1: Logic Gates ---
        section_header("Logic Gates", show_logic_gates);
        if (show_logic_gates) {
            wrapped("Logic gates are the building blocks of digital "
                    "circuits. Each takes binary inputs (0 or 1) and "
                    "produces a single output.");
            y += pg;
            for (const auto& g : gates) {
                label_line(g.name, FONT_SIZE, g.color);
                wrapped(g.desc);
                y += lg;
                tt_line(g.truth);
                y += pg;
            }
            y += 4.0f;
        }

        // --- Section 2: Ripple-Carry Adder ---
        section_header("Ripple-Carry Adder", show_ripple_carry);
        if (show_ripple_carry) {
            for (const char* p : rca_paras) {
                wrapped(p);
                y += pg;
            }
            y += 4.0f;
        }

        // --- Section 3: Reading the Animation ---
        section_header("Reading the Animation", show_reading_anim);
        if (show_reading_anim) {
            for (const char* item : legend_items) {
                wrapped(item);
                y += lg;
            }
            y += pg - lg;
            wrapped("Gate accents: teal = XOR, orange = AND, yellow = OR");
            y += pg;
            wrapped("Tip: Hover any gate to see its truth table with "
                    "current inputs highlighted.");
            y += lg;
            wrapped("Tip: Use the speed slider to slow propagation and "
                    "watch each gate resolve.");
            y += pg + 4.0f;
        }

        return y - start_y;
    };

    // Measure total scrollable content height
    float content_h = render_sections(0.0f, false);
    float max_scroll = std::max(0.0f, content_h - viewport_h);

    // Scroll input
    Rectangle panel_rect = {panel_x, panel_y, panel_w, panel_h};
    if (CheckCollisionPointRec(mouse, panel_rect)) {
        float wheel = GetMouseWheelMove();
        if (wheel != 0.0f) {
            scroll_target -= wheel * EXPL_SCROLL_SPEED;
        }
    }

    scroll_target = std::clamp(scroll_target, 0.0f, max_scroll);
    scroll_current += (scroll_target - scroll_current) * EXPL_SCROLL_SMOOTHING;
    scroll_current = std::clamp(scroll_current, 0.0f, max_scroll);

    // Draw scrollable sections in scissor region
    BeginScissorMode(static_cast<int>(panel_x + PADDING), static_cast<int>(content_top),
                     static_cast<int>(text_w), static_cast<int>(viewport_h));
    render_sections(content_top - scroll_current, true);
    EndScissorMode();

    // Scrollbar
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

    return panel_h;
}

} // namespace gateflow
