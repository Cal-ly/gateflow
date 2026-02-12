/// @file input_panel.cpp
/// @brief Implements the input panel with custom-drawn Raylib UI elements

#include "ui/input_panel.hpp"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <cmath>

namespace gateflow {

namespace {

// --- Layout constants ---
constexpr float ROW_HEIGHT = 32.0f;
constexpr float ROW_GAP = 8.0f;
constexpr float FIELD_HEIGHT = 28.0f;
constexpr float BUTTON_HEIGHT = 30.0f;
constexpr float SLIDER_HEIGHT = 20.0f;
constexpr float LABEL_WIDTH = 28.0f;
constexpr float PADDING = 10.0f;
constexpr int FONT_SIZE = 16;
constexpr int FONT_SIZE_SMALL = 13;
constexpr float SLIDER_EPSILON = 0.001f;

// --- Colors ---
const Color BG_COLOR = {35, 35, 42, 230};
const Color BORDER_COLOR = {70, 70, 85, 255};
const Color FIELD_BG = {25, 25, 32, 255};
const Color FIELD_BG_ACTIVE = {30, 30, 50, 255};
const Color FIELD_BORDER = {90, 90, 110, 255};
const Color FIELD_BORDER_ACTIVE = {100, 140, 255, 255};
const Color TEXT_COLOR = {220, 220, 230, 255};
const Color LABEL_COLOR = {160, 160, 180, 255};
const Color BUTTON_BG = {50, 50, 65, 255};
const Color BUTTON_BG_HOVER = {65, 65, 85, 255};
const Color BUTTON_BG_ACTIVE = {40, 120, 60, 255};
const Color BUTTON_TEXT = {220, 220, 230, 255};
const Color TOGGLE_ON = {40, 160, 70, 255};
const Color TOGGLE_OFF = {70, 70, 85, 255};
const Color SLIDER_TRACK = {50, 50, 60, 255};
const Color SLIDER_FILL = {60, 140, 200, 255};
const Color SLIDER_HANDLE = {180, 180, 200, 255};

/// Draw a text input field. Returns true if the value changed.
bool draw_number_field(const char* label, char* buf, int& cursor, bool& editing, float x, float y,
                       float w, int& out_value, int min_val, int max_val) {
    // Label
    DrawText(label, static_cast<int>(x), static_cast<int>(y + 6), FONT_SIZE, LABEL_COLOR);

    // Field rectangle
    float fx = x + LABEL_WIDTH;
    float fw = w - LABEL_WIDTH;
    Rectangle field_rect = {fx, y, fw, FIELD_HEIGHT};

    // Mouse interaction
    Vector2 mouse = GetMousePosition();
    bool hovered = CheckCollisionPointRec(mouse, field_rect);

    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        editing = hovered;
    }

    // Draw field background
    DrawRectangleRec(field_rect, editing ? FIELD_BG_ACTIVE : FIELD_BG);
    DrawRectangleLinesEx(field_rect, 1.0f, editing ? FIELD_BORDER_ACTIVE : FIELD_BORDER);

    bool changed = false;

    // Handle keyboard input when editing
    if (editing) {
        int key = GetCharPressed();
        while (key > 0) {
            if (key >= '0' && key <= '9' && cursor < 3) {
                buf[cursor] = static_cast<char>(key);
                cursor++;
                buf[cursor] = '\0';
            }
            key = GetCharPressed();
        }

        if (IsKeyPressed(KEY_BACKSPACE) && cursor > 0) {
            cursor--;
            buf[cursor] = '\0';
        }

        if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER)) {
            editing = false;
        }

        // Parse and clamp
        int val = 0;
        if (cursor > 0) {
            val = std::atoi(buf);
        }
        val = std::clamp(val, min_val, max_val);
        if (val != out_value) {
            out_value = val;
            changed = true;
        }

        // Draw cursor blink
        float text_w = static_cast<float>(MeasureText(buf, FONT_SIZE));
        float cx = fx + 6.0f + text_w;
        if (static_cast<int>(GetTime() * 2.0) % 2 == 0) {
            DrawLine(static_cast<int>(cx), static_cast<int>(y + 5), static_cast<int>(cx),
                     static_cast<int>(y + FIELD_HEIGHT - 5), TEXT_COLOR);
        }
    }

    // Draw text
    DrawText(buf, static_cast<int>(fx + 6), static_cast<int>(y + 6), FONT_SIZE, TEXT_COLOR);
    return changed;
}

/// Draw a button. Returns true if clicked this frame.
bool draw_button(const char* text, float x, float y, float w, float h, Color bg_normal,
                 Color bg_hover) {
    Rectangle rect = {x, y, w, h};
    Vector2 mouse = GetMousePosition();
    bool hovered = CheckCollisionPointRec(mouse, rect);
    bool clicked = hovered && IsMouseButtonPressed(MOUSE_BUTTON_LEFT);

    DrawRectangleRec(rect, hovered ? bg_hover : bg_normal);
    DrawRectangleLinesEx(rect, 1.0f, BORDER_COLOR);

    int tw = MeasureText(text, FONT_SIZE_SMALL);
    DrawText(text, static_cast<int>(x + (w - static_cast<float>(tw)) / 2.0f),
             static_cast<int>(y + (h - static_cast<float>(FONT_SIZE_SMALL)) / 2.0f),
             FONT_SIZE_SMALL, BUTTON_TEXT);

    return clicked;
}

/// Draw a toggle button. Returns true if toggled this frame.
bool draw_toggle(const char* label, bool value, float x, float y, float w, float h) {
    Rectangle rect = {x, y, w, h};
    Vector2 mouse = GetMousePosition();
    bool hovered = CheckCollisionPointRec(mouse, rect);
    bool clicked = hovered && IsMouseButtonPressed(MOUSE_BUTTON_LEFT);

    Color bg = value ? TOGGLE_ON : TOGGLE_OFF;
    if (hovered) {
        bg.r = static_cast<unsigned char>(std::min(255, bg.r + 20));
        bg.g = static_cast<unsigned char>(std::min(255, bg.g + 20));
        bg.b = static_cast<unsigned char>(std::min(255, bg.b + 20));
    }

    DrawRectangleRec(rect, bg);
    DrawRectangleLinesEx(rect, 1.0f, BORDER_COLOR);

    int tw = MeasureText(label, FONT_SIZE_SMALL);
    DrawText(label, static_cast<int>(x + (w - static_cast<float>(tw)) / 2.0f),
             static_cast<int>(y + (h - static_cast<float>(FONT_SIZE_SMALL)) / 2.0f),
             FONT_SIZE_SMALL, BUTTON_TEXT);

    return clicked;
}

/// Draw a horizontal slider. Returns true if the value changed.
bool draw_slider(const char* label, float& value, float min_val, float max_val, bool& dragging,
                 float x, float y, float w) {
    // Label
    DrawText(label, static_cast<int>(x), static_cast<int>(y), FONT_SIZE_SMALL, LABEL_COLOR);

    float track_y = y + static_cast<float>(FONT_SIZE_SMALL) + 4.0f;
    float track_w = w;
    Rectangle track = {x, track_y, track_w, SLIDER_HEIGHT};

    // Normalized position
    float norm = (value - min_val) / (max_val - min_val);
    norm = std::clamp(norm, 0.0f, 1.0f);

    // Handle interaction
    Vector2 mouse = GetMousePosition();
    bool hovered = CheckCollisionPointRec(mouse, track);

    if (hovered && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        dragging = true;
    }
    if (!IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
        dragging = false;
    }

    bool changed = false;
    if (dragging) {
        float new_norm = (mouse.x - x) / track_w;
        new_norm = std::clamp(new_norm, 0.0f, 1.0f);
        float new_val = min_val + new_norm * (max_val - min_val);
        if (std::fabs(new_val - value) > SLIDER_EPSILON) {
            value = new_val;
            changed = true;
        }
        norm = new_norm;
    }

    // Draw track
    DrawRectangleRec(track, SLIDER_TRACK);
    // Draw filled portion
    DrawRectangleRec({x, track_y, track_w * norm, SLIDER_HEIGHT}, SLIDER_FILL);
    // Draw handle
    float handle_x = x + track_w * norm - 4.0f;
    DrawRectangleRec({handle_x, track_y - 2.0f, 8.0f, SLIDER_HEIGHT + 4.0f}, SLIDER_HANDLE);

    // Draw value text
    char val_str[16];
    std::snprintf(val_str, sizeof(val_str), "%.1f", static_cast<double>(value));
    DrawText(val_str, static_cast<int>(x + track_w + 8.0f), static_cast<int>(track_y + 2.0f),
             FONT_SIZE_SMALL, TEXT_COLOR);

    return changed;
}

} // namespace

InputPanelResult draw_input_panel(UIState& state, float panel_x, float panel_y, float panel_w) {
    InputPanelResult result;
    UIAction& action = result.action;

    // Measure the panel's height from current control stack, so callers can
    // place subsequent panels without hardcoded offsets.
    float measure_y = panel_y + PADDING;
    measure_y += ROW_HEIGHT;                      // Title row
    measure_y += ROW_HEIGHT + ROW_GAP;            // Input A
    measure_y += ROW_HEIGHT + ROW_GAP + 4.0f;     // Input B
    measure_y += BUTTON_HEIGHT + ROW_GAP + 4.0f;  // Button row
    measure_y += ROW_HEIGHT + SLIDER_HEIGHT + ROW_GAP; // Slider block
    measure_y += BUTTON_HEIGHT;                   // NAND toggle
    result.panel_height = (measure_y + PADDING) - panel_y;

    float content_w = panel_w - 2.0f * PADDING;
    float cx = panel_x + PADDING;
    float cy = panel_y + PADDING;

    // Panel background
    DrawRectangleRec({panel_x, panel_y, panel_w, result.panel_height}, BG_COLOR);
    DrawRectangleLinesEx({panel_x, panel_y, panel_w, result.panel_height}, 1.0f, BORDER_COLOR);

    // Title
    DrawText("INPUTS", static_cast<int>(cx), static_cast<int>(cy), FONT_SIZE, TEXT_COLOR);
    cy += ROW_HEIGHT;

    // Input A
    if (draw_number_field("A:", state.buf_a, state.cursor_a, state.editing_a, cx, cy, content_w,
                          state.input_a, 0, 99)) {
        action.inputs_changed = true;
    }
    cy += ROW_HEIGHT + ROW_GAP;

    // Input B
    if (draw_number_field("B:", state.buf_b, state.cursor_b, state.editing_b, cx, cy, content_w,
                          state.input_b, 0, 99)) {
        action.inputs_changed = true;
    }
    cy += ROW_HEIGHT + ROW_GAP + 4.0f;

    // Button row: Run | Pause | Step | Reset
    float btn_w = (content_w - 3.0f * 4.0f) / 4.0f;
    if (draw_button("Run", cx, cy, btn_w, BUTTON_HEIGHT, BUTTON_BG_ACTIVE, BUTTON_BG_HOVER)) {
        action.run_pressed = true;
    }
    if (draw_button(state.is_running ? "Pause" : "Play", cx + btn_w + 4.0f, cy, btn_w,
                    BUTTON_HEIGHT, BUTTON_BG, BUTTON_BG_HOVER)) {
        action.pause_pressed = true;
    }
    if (draw_button("Step", cx + 2.0f * (btn_w + 4.0f), cy, btn_w, BUTTON_HEIGHT, BUTTON_BG,
                    BUTTON_BG_HOVER)) {
        action.step_pressed = true;
    }
    if (draw_button("Reset", cx + 3.0f * (btn_w + 4.0f), cy, btn_w, BUTTON_HEIGHT, BUTTON_BG,
                    BUTTON_BG_HOVER)) {
        action.reset_pressed = true;
    }
    cy += BUTTON_HEIGHT + ROW_GAP + 4.0f;

    // Speed slider
    if (draw_slider("Speed (depths/sec)", state.speed, 0.5f, 20.0f, state.dragging_speed, cx, cy,
                    content_w - 40.0f)) {
        action.speed_changed = true;
    }
    cy += ROW_HEIGHT + SLIDER_HEIGHT + ROW_GAP;

    // NAND toggle
    if (draw_toggle(state.show_nand ? "NAND View: ON" : "NAND View: OFF", state.show_nand, cx, cy,
                    content_w, BUTTON_HEIGHT)) {
        state.show_nand = !state.show_nand;
        action.nand_toggled = true;
    }

    return result;
}

} // namespace gateflow
