/// @file wire_renderer.cpp
/// @brief Draws wires as polylines with signal travel animation

#include "rendering/wire_renderer.hpp"

#include <algorithm>
#include <cmath>

namespace gateflow {

namespace {

// --- Wire colors and thickness ---
const Color WIRE_INACTIVE_COLOR = {60, 60, 60, 255}; // Dark gray
const Color WIRE_ACTIVE_COLOR = {50, 220, 80, 255};  // Bright green
const Color WIRE_PENDING_COLOR = {35, 35, 40, 255};  // Very dim
const Color WIRE_SIGNAL_GLOW = {100, 255, 130, 255}; // Bright glow for signal pulse
constexpr float WIRE_INACTIVE_THICKNESS = 1.5f;
constexpr float WIRE_ACTIVE_THICKNESS = 3.0f;
constexpr float WIRE_PENDING_THICKNESS = 1.0f;
constexpr float SIGNAL_PULSE_RADIUS = 5.0f;

/// Converts a logical-unit vec2 to screen-space
Vector2 to_screen(Vec2 v, float scale, Vector2 offset) {
    return {v.x * scale + offset.x, v.y * scale + offset.y};
}

/// Compute the total length of a polyline path
float path_length(const std::vector<Vec2>& path) {
    float len = 0.0f;
    for (size_t i = 0; i + 1 < path.size(); i++) {
        float dx = path[i + 1].x - path[i].x;
        float dy = path[i + 1].y - path[i].y;
        len += std::sqrt(dx * dx + dy * dy);
    }
    return len;
}

/// Interpolate a position along a polyline path at a given fraction (0.0–1.0)
Vec2 lerp_along_path(const std::vector<Vec2>& path, float t) {
    if (path.empty()) {
        return {0.0f, 0.0f};
    }
    if (t <= 0.0f) {
        return path.front();
    }
    if (t >= 1.0f) {
        return path.back();
    }

    float total = path_length(path);
    float target_dist = t * total;
    float accumulated = 0.0f;

    for (size_t i = 0; i + 1 < path.size(); i++) {
        float dx = path[i + 1].x - path[i].x;
        float dy = path[i + 1].y - path[i].y;
        float seg_len = std::sqrt(dx * dx + dy * dy);

        if (accumulated + seg_len >= target_dist) {
            float frac = (seg_len > 0.001f) ? (target_dist - accumulated) / seg_len : 0.0f;
            return {path[i].x + dx * frac, path[i].y + dy * frac};
        }
        accumulated += seg_len;
    }
    return path.back();
}

} // namespace

void draw_wires(const Circuit& circuit, const Layout& layout, const AnimationState& anim,
                float scale, Vector2 offset) {
    for (auto& wire_ptr : circuit.wires()) {
        const Wire* wire = wire_ptr.get();
        auto it = layout.wire_paths.find(wire);
        if (it == layout.wire_paths.end()) {
            continue;
        }

        const auto& path = it->second;
        if (path.size() < 2) {
            continue;
        }

        const WireAnim& wa = anim.wire_anim(wire);
        bool active = wire->get_value();

        Color color;
        float thickness;

        if (!wa.resolved) {
            if (wa.signal_progress > 0.0f) {
                // Signal is traveling along this wire — draw the resolved portion
                // in the active/inactive color and the rest as pending

                // Draw the full wire as pending first
                for (size_t i = 0; i + 1 < path.size(); i++) {
                    Vector2 from = to_screen(path[i], scale, offset);
                    Vector2 to = to_screen(path[i + 1], scale, offset);
                    DrawLineEx(from, to, WIRE_PENDING_THICKNESS, WIRE_PENDING_COLOR);
                }

                // Draw the resolved portion on top
                Color resolved_color = active ? WIRE_ACTIVE_COLOR : WIRE_INACTIVE_COLOR;
                float resolved_thickness = active ? WIRE_ACTIVE_THICKNESS : WIRE_INACTIVE_THICKNESS;

                // Draw segments up to the signal position
                float total = path_length(path);
                float target_dist = wa.signal_progress * total;
                float accumulated = 0.0f;

                for (size_t i = 0; i + 1 < path.size(); i++) {
                    float dx = path[i + 1].x - path[i].x;
                    float dy = path[i + 1].y - path[i].y;
                    float seg_len = std::sqrt(dx * dx + dy * dy);

                    if (accumulated + seg_len <= target_dist) {
                        // Full segment is resolved
                        Vector2 from = to_screen(path[i], scale, offset);
                        Vector2 to = to_screen(path[i + 1], scale, offset);
                        DrawLineEx(from, to, resolved_thickness, resolved_color);
                    } else if (accumulated < target_dist) {
                        // Partial segment
                        float frac =
                            (seg_len > 0.001f) ? (target_dist - accumulated) / seg_len : 0.0f;
                        Vec2 mid = {path[i].x + dx * frac, path[i].y + dy * frac};
                        Vector2 from = to_screen(path[i], scale, offset);
                        Vector2 to = to_screen(mid, scale, offset);
                        DrawLineEx(from, to, resolved_thickness, resolved_color);
                    }
                    accumulated += seg_len;
                }

                // Draw signal pulse dot at the wavefront
                Vec2 pulse_pos = lerp_along_path(path, wa.signal_progress);
                Vector2 pulse_screen = to_screen(pulse_pos, scale, offset);
                DrawCircleV(pulse_screen, SIGNAL_PULSE_RADIUS, WIRE_SIGNAL_GLOW);

                continue; // Skip the normal drawing below
            }

            // Wire not yet started — draw as pending
            color = WIRE_PENDING_COLOR;
            thickness = WIRE_PENDING_THICKNESS;
        } else {
            // Fully resolved
            color = active ? WIRE_ACTIVE_COLOR : WIRE_INACTIVE_COLOR;
            thickness = active ? WIRE_ACTIVE_THICKNESS : WIRE_INACTIVE_THICKNESS;
        }

        // Draw polyline segments
        for (size_t i = 0; i + 1 < path.size(); i++) {
            Vector2 from = to_screen(path[i], scale, offset);
            Vector2 to = to_screen(path[i + 1], scale, offset);
            DrawLineEx(from, to, thickness, color);
        }

        // Draw small dot at each waypoint for visual clarity (only for resolved wires)
        if (wa.resolved) {
            for (size_t i = 1; i + 1 < path.size(); i++) {
                Vector2 pt = to_screen(path[i], scale, offset);
                DrawCircleV(pt, thickness * 0.8f, color);
            }
        }
    }
}

} // namespace gateflow
