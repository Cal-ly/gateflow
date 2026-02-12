/// @file wire_renderer.cpp
/// @brief Draws wires as polylines with signal travel animation

#include "rendering/wire_renderer.hpp"

#include "simulation/gate.hpp"

#include <algorithm>
#include <cmath>

namespace gateflow {

namespace {

// --- Wire colors and thickness ---
const Color WIRE_INACTIVE_COLOR = {60, 60, 60, 255}; // Dark gray
const Color WIRE_ACTIVE_COLOR = {50, 220, 80, 255};  // Bright green
const Color WIRE_PENDING_COLOR = {35, 35, 40, 255};  // Very dim
const Color WIRE_SIGNAL_GLOW = {100, 255, 130, 255}; // Bright glow for signal pulse
const Color CARRY_ACTIVE_COLOR = {245, 190, 70, 255};
const Color CARRY_INACTIVE_COLOR = {120, 95, 50, 255};
const Color CARRY_PENDING_COLOR = {70, 55, 35, 255};
const Color CARRY_SIGNAL_GLOW = {255, 220, 120, 255};
constexpr float WIRE_INACTIVE_THICKNESS = 1.5f;
constexpr float WIRE_ACTIVE_THICKNESS = 3.0f;
constexpr float WIRE_PENDING_THICKNESS = 1.0f;
constexpr float SIGNAL_PULSE_RADIUS = 5.0f;
constexpr float CARRY_THICKNESS_SCALE = 2.3f;
constexpr float CARRY_PULSE_RADIUS_SCALE = 1.6f;

/// Converts a logical-unit vec2 to screen-space
Vector2 to_screen(Vec2 v, float scale, Vector2 offset) {
    return {v.x * scale + offset.x, v.y * scale + offset.y};
}

/// Interpolate a position along a precomputed wire path at fraction [0, 1].
Vec2 lerp_along_path(const WirePath& path, float t) {
    if (path.points.empty()) {
        return {0.0f, 0.0f};
    }
    if (t <= 0.0f || path.total_length <= 0.0f || path.points.size() < 2) {
        return path.points.front();
    }
    if (t >= 1.0f) {
        return path.points.back();
    }

    float target_dist = t * path.total_length;

    for (size_t i = 0; i + 1 < path.points.size(); i++) {
        float seg_start = path.cumulative_lengths[i];
        float seg_end = path.cumulative_lengths[i + 1];
        if (target_dist <= seg_end) {
            float seg_len = seg_end - seg_start;
            float frac = (seg_len > 0.001f) ? (target_dist - seg_start) / seg_len : 0.0f;
            float dx = path.points[i + 1].x - path.points[i].x;
            float dy = path.points[i + 1].y - path.points[i].y;
            return {path.points[i].x + dx * frac, path.points[i].y + dy * frac};
        }
    }

    return path.points.back();
}

void draw_polyline(const std::vector<Vec2>& points, float scale, Vector2 offset, float thickness,
                   Color color) {
    for (size_t i = 0; i + 1 < points.size(); i++) {
        Vector2 from = to_screen(points[i], scale, offset);
        Vector2 to = to_screen(points[i + 1], scale, offset);
        DrawLineEx(from, to, thickness, color);
    }
}

bool is_carry_wire(const Wire* wire) {
    const Gate* src = wire->get_source();
    if (src == nullptr) {
        return false;
    }

    // Full-adder carry outputs are driven by OR gates in this architecture.
    if (src->get_type() == GateType::OR) {
        return true;
    }

    // Bit 0 carry from half-adder is AND -> (XOR, AND) in next stage.
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
        return has_xor_dest && has_and_dest;
    }

    return false;
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

        const auto& branches = it->second;
        if (branches.empty()) {
            continue;
        }

        const WireAnim& wa = anim.wire_anim(wire);
        bool active = wire->get_value();
        bool carry_wire = is_carry_wire(wire);

        Color color;
        float thickness;

        if (!wa.resolved) {
            if (wa.signal_progress > 0.0f) {
                // Signal is traveling along this wire — draw the resolved portion
                // in the active/inactive color and the rest as pending

                // Draw the resolved portion on top
                Color resolved_color = active ? WIRE_ACTIVE_COLOR : WIRE_INACTIVE_COLOR;
                float resolved_thickness = active ? WIRE_ACTIVE_THICKNESS : WIRE_INACTIVE_THICKNESS;
                Color pending_color = WIRE_PENDING_COLOR;
                Color pulse_color = WIRE_SIGNAL_GLOW;

                if (carry_wire) {
                    resolved_color = active ? CARRY_ACTIVE_COLOR : CARRY_INACTIVE_COLOR;
                    resolved_thickness *= CARRY_THICKNESS_SCALE;
                    pending_color = CARRY_PENDING_COLOR;
                    pulse_color = CARRY_SIGNAL_GLOW;
                }

                for (const WirePath& branch : branches) {
                    if (branch.points.size() < 2) {
                        continue;
                    }

                    // Draw the full branch as pending first
                    float pending_thickness =
                        carry_wire ? (WIRE_PENDING_THICKNESS * CARRY_THICKNESS_SCALE)
                                   : WIRE_PENDING_THICKNESS;
                    draw_polyline(branch.points, scale, offset, pending_thickness, pending_color);

                    // Draw segments up to the signal position
                    float target_dist = wa.signal_progress * branch.total_length;

                    for (size_t i = 0; i + 1 < branch.points.size(); i++) {
                        float seg_start = branch.cumulative_lengths[i];
                        float seg_end = branch.cumulative_lengths[i + 1];

                        if (seg_end <= target_dist) {
                            // Full segment is resolved
                            Vector2 from = to_screen(branch.points[i], scale, offset);
                            Vector2 to = to_screen(branch.points[i + 1], scale, offset);
                            DrawLineEx(from, to, resolved_thickness, resolved_color);
                        } else if (seg_start < target_dist) {
                            // Partial segment
                            float seg_len = seg_end - seg_start;
                            float frac =
                                (seg_len > 0.001f) ? (target_dist - seg_start) / seg_len : 0.0f;
                            float dx = branch.points[i + 1].x - branch.points[i].x;
                            float dy = branch.points[i + 1].y - branch.points[i].y;
                            Vec2 mid = {
                                branch.points[i].x + dx * frac,
                                branch.points[i].y + dy * frac,
                            };
                            Vector2 from = to_screen(branch.points[i], scale, offset);
                            Vector2 to = to_screen(mid, scale, offset);
                            DrawLineEx(from, to, resolved_thickness, resolved_color);
                        }
                    }

                    // Draw signal pulse dot at the wavefront for each branch
                    Vec2 pulse_pos = lerp_along_path(branch, wa.signal_progress);
                    Vector2 pulse_screen = to_screen(pulse_pos, scale, offset);
                    DrawCircleV(pulse_screen,
                                SIGNAL_PULSE_RADIUS * (carry_wire ? CARRY_PULSE_RADIUS_SCALE : 1.0f),
                                pulse_color);
                }

                continue; // Skip the normal drawing below
            }

            // Wire not yet started — draw as pending
            color = carry_wire ? CARRY_PENDING_COLOR : WIRE_PENDING_COLOR;
            thickness = carry_wire ? (WIRE_PENDING_THICKNESS * CARRY_THICKNESS_SCALE)
                                   : WIRE_PENDING_THICKNESS;
        } else {
            // Fully resolved
            if (carry_wire) {
                color = active ? CARRY_ACTIVE_COLOR : CARRY_INACTIVE_COLOR;
                thickness = (active ? WIRE_ACTIVE_THICKNESS : WIRE_INACTIVE_THICKNESS) *
                            CARRY_THICKNESS_SCALE;
            } else {
                color = active ? WIRE_ACTIVE_COLOR : WIRE_INACTIVE_COLOR;
                thickness = active ? WIRE_ACTIVE_THICKNESS : WIRE_INACTIVE_THICKNESS;
            }
        }

        for (const WirePath& branch : branches) {
            if (branch.points.size() < 2) {
                continue;
            }

            // Draw polyline segments
            draw_polyline(branch.points, scale, offset, thickness, color);

            // Draw small dot at each waypoint for visual clarity (only for resolved wires)
            if (wa.resolved) {
                for (size_t i = 1; i + 1 < branch.points.size(); i++) {
                    Vector2 pt = to_screen(branch.points[i], scale, offset);
                    DrawCircleV(pt, thickness * 0.8f, color);
                }

                if (carry_wire) {
                    Vec2 mid = lerp_along_path(branch, 0.5f);
                    Vector2 smid = to_screen(mid, scale, offset);
                    DrawText("C", static_cast<int>(smid.x + 2), static_cast<int>(smid.y - 10), 12,
                             CARRY_ACTIVE_COLOR);
                }
            }
        }
    }
}

} // namespace gateflow
