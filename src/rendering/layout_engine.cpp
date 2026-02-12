/// @file layout_engine.cpp
/// @brief Computes gate and wire positions for circuit visualization

#include "rendering/layout_engine.hpp"

#include <algorithm>
#include <cmath>
#include <unordered_map>
#include <unordered_set>

namespace gateflow {

namespace {

// --- Layout constants (logical units) ---
constexpr float GATE_WIDTH = 3.0f;
constexpr float GATE_HEIGHT = 2.0f;
constexpr float GATE_VERTICAL_SPACING = 3.0f; // Between gates in a column
constexpr float COLUMN_SPACING = 8.0f;        // Between full-adder columns
constexpr float INPUT_MARGIN_TOP = 2.0f;      // Space above first row for inputs
constexpr float OUTPUT_MARGIN_BOTTOM = 2.0f;  // Space below last row for outputs
constexpr float CARRY_WIRE_Y = 0.0f;          // Y position for carry chain
constexpr float LABEL_AREA_HEIGHT = 3.0f;     // Space for input labels at top

/// Computes the topological depth of each gate (longest path from any input).
/// Used for generic fallback layout.
std::unordered_map<const Gate*, int> compute_gate_depths(const Circuit& circuit) {
    std::unordered_map<const Gate*, int> depth;
    for (const Gate* gate : circuit.topological_order()) {
        int max_input_depth = -1;
        for (const Wire* input_wire : gate->get_inputs()) {
            const Gate* src = input_wire->get_source();
            if (src != nullptr) {
                auto it = depth.find(src);
                if (it != depth.end()) {
                    max_input_depth = std::max(max_input_depth, it->second);
                }
            }
        }
        depth[gate] = max_input_depth + 1;
    }
    return depth;
}

/// Groups gates by depth column for generic layout
std::map<int, std::vector<const Gate*>>
group_by_depth(const std::unordered_map<const Gate*, int>& depths) {
    std::map<int, std::vector<const Gate*>> columns;
    for (auto& [gate, d] : depths) {
        columns[d].push_back(gate);
    }
    return columns;
}

/// Computes a Manhattan-routed wire path between two points.
/// Routes horizontally first, then vertically, then horizontally to destination.
std::vector<Vec2> route_wire(Vec2 from, Vec2 to, float channel_offset = 0.0f) {
    std::vector<Vec2> path;
    path.push_back(from);

    if (std::abs(from.x - to.x) < 0.01f) {
        // Vertical-only wire
        path.push_back(to);
    } else if (std::abs(from.y - to.y) < 0.01f) {
        // Horizontal-only wire
        path.push_back(to);
    } else {
        // Manhattan route: go down from source, then horizontal, then down to dest
        float mid_y = (from.y + to.y) / 2.0f + channel_offset;
        path.push_back({from.x, mid_y});
        path.push_back({to.x, mid_y});
        path.push_back(to);
    }
    return path;
}

/// Returns the center-right point of a gate rectangle (output connection point)
Vec2 gate_output_point(const Rect& r) {
    return {r.x + r.w, r.y + r.h / 2.0f};
}

/// Returns the center-left point of a gate rectangle (input connection point)
Vec2 gate_input_point(const Rect& r, int input_index, int total_inputs) {
    float step = r.h / static_cast<float>(total_inputs + 1);
    return {r.x, r.y + step * static_cast<float>(input_index + 1)};
}

} // namespace

Layout compute_layout(const Circuit& circuit) {
    Layout layout;

    // Detect the circuit structure:
    // A ripple-carry adder built by build_ripple_carry_adder(n) has
    //   - 2*n input wires, n+1 output wires
    //   - Bit 0: 2 gates (half adder: XOR, AND)
    //   - Bits 1..n-1: 5 gates each (full adder: XOR, AND, XOR, AND, OR)
    //
    // We lay out columns right-to-left. Each column is one bit position.

    int num_inputs = static_cast<int>(circuit.num_inputs());
    int num_outputs = static_cast<int>(circuit.num_outputs());
    int num_gates = static_cast<int>(circuit.gates().size());

    // Check if this looks like a ripple-carry adder
    bool is_rca = (num_inputs % 2 == 0) && (num_outputs == num_inputs / 2 + 1);
    int bits = num_inputs / 2;
    int expected_gates = (bits > 0) ? 2 + (bits - 1) * 5 : 0;
    is_rca = is_rca && (num_gates == expected_gates) && (bits >= 1);

    if (is_rca) {
        // --- Ripple-carry adder specific layout ---
        // Column 0 (rightmost) = bit 0 (half adder, 2 gates)
        // Columns 1..bits-1 (left) = bits 1..bits-1 (full adder, 5 gates each)

        float start_y = LABEL_AREA_HEIGHT + INPUT_MARGIN_TOP;

        // Walk through gates in creation order — they match the builder's pattern:
        //   Bit 0: gates[0]=XOR, gates[1]=AND
        //   Bit i>0: gates[2+5*(i-1)+0]=XOR1, +1=AND1, +2=XOR2, +3=AND2, +4=OR
        auto& gates = circuit.gates();

        // Position each gate in its column
        for (int bit = 0; bit < bits; bit++) {
            // Column x: bit 0 is rightmost, higher bits go left
            float col_x = static_cast<float>(bits - 1 - bit) * COLUMN_SPACING;

            if (bit == 0) {
                // Half adder: XOR (row 0), AND (row 1)
                Gate* xor_g = gates[0].get();
                Gate* and_g = gates[1].get();

                layout.gate_positions[xor_g] = {col_x, start_y, GATE_WIDTH, GATE_HEIGHT};
                layout.gate_positions[and_g] = {
                    col_x, start_y + GATE_HEIGHT + GATE_VERTICAL_SPACING, GATE_WIDTH, GATE_HEIGHT};
            } else {
                // Full adder: XOR1, AND1, XOR2, AND2, OR
                int base = 2 + (bit - 1) * 5;
                Gate* xor1 = gates[base + 0].get();
                Gate* and1 = gates[base + 1].get();
                Gate* xor2 = gates[base + 2].get();
                Gate* and2 = gates[base + 3].get();
                Gate* or_g = gates[base + 4].get();

                float y = start_y;
                float row_step = GATE_HEIGHT + GATE_VERTICAL_SPACING;

                layout.gate_positions[xor1] = {col_x, y, GATE_WIDTH, GATE_HEIGHT};
                y += row_step;
                layout.gate_positions[and1] = {col_x, y, GATE_WIDTH, GATE_HEIGHT};
                y += row_step;
                layout.gate_positions[xor2] = {col_x, y, GATE_WIDTH, GATE_HEIGHT};
                y += row_step;
                layout.gate_positions[and2] = {col_x, y, GATE_WIDTH, GATE_HEIGHT};
                y += row_step;
                layout.gate_positions[or_g] = {col_x, y, GATE_WIDTH, GATE_HEIGHT};
            }
        }

        // Input positions: A[0..n-1] then B[0..n-1]
        // Each input pair (A[i], B[i]) is at the top of column i
        for (int i = 0; i < bits; i++) {
            float col_x = static_cast<float>(bits - 1 - i) * COLUMN_SPACING;
            // A input slightly left of center
            layout.input_positions.push_back(
                {col_x + GATE_WIDTH * 0.33f, LABEL_AREA_HEIGHT}); // A[i]
        }
        for (int i = 0; i < bits; i++) {
            float col_x = static_cast<float>(bits - 1 - i) * COLUMN_SPACING;
            layout.input_positions.push_back(
                {col_x + GATE_WIDTH * 0.67f, LABEL_AREA_HEIGHT}); // B[i]
        }

        // Output positions: Sum[0..n-1] then Carry-out
        // Each sum output is at the bottom of its column
        float max_y = start_y; // Track the tallest column
        for (auto& [gate, rect] : layout.gate_positions) {
            max_y = std::max(max_y, rect.y + rect.h);
        }
        float output_y = max_y + OUTPUT_MARGIN_BOTTOM;

        for (int i = 0; i < bits; i++) {
            float col_x = static_cast<float>(bits - 1 - i) * COLUMN_SPACING;
            layout.output_positions.push_back({col_x + GATE_WIDTH / 2.0f, output_y});
        }
        // Carry-out: at the leftmost column, slightly above
        float carry_out_x = static_cast<float>(bits - 1) * COLUMN_SPACING + COLUMN_SPACING * 0.5f;
        layout.output_positions.push_back({carry_out_x, output_y});

        // --- Wire routing ---
        for (auto& wire_ptr : circuit.wires()) {
            const Wire* wire = wire_ptr.get();
            const Gate* src_gate = wire->get_source();
            const auto& dests = wire->get_destinations();

            if (src_gate == nullptr && dests.empty()) {
                continue; // Unconnected wire
            }

            if (src_gate == nullptr) {
                // Primary input wire — route from input position to gate input
                // Find which input index this wire is
                for (size_t idx = 0; idx < circuit.input_wires().size(); idx++) {
                    if (circuit.input_wires()[idx] == wire) {
                        Vec2 from = layout.input_positions[idx];
                        for (const Gate* dest : dests) {
                            auto it = layout.gate_positions.find(dest);
                            if (it == layout.gate_positions.end()) {
                                continue;
                            }
                            int inp_idx = 0;
                            for (size_t k = 0; k < dest->get_inputs().size(); k++) {
                                if (dest->get_inputs()[k] == wire) {
                                    inp_idx = static_cast<int>(k);
                                    break;
                                }
                            }
                            Vec2 to = gate_input_point(it->second, inp_idx,
                                                       static_cast<int>(dest->get_inputs().size()));
                            layout.wire_paths[wire] = route_wire(from, to);
                        }
                        break;
                    }
                }
                continue;
            }

            // Gate-to-gate wire or gate-to-output wire
            auto src_it = layout.gate_positions.find(src_gate);
            if (src_it == layout.gate_positions.end()) {
                continue;
            }
            Vec2 from = gate_output_point(src_it->second);

            if (dests.empty()) {
                // Output-only wire — route to output position
                for (size_t idx = 0; idx < circuit.output_wires().size(); idx++) {
                    if (circuit.output_wires()[idx] == wire) {
                        Vec2 to = layout.output_positions[idx];
                        layout.wire_paths[wire] = route_wire(from, to);
                        break;
                    }
                }
            } else {
                // Gate-to-gate wire — route to first destination
                // (for fan-out wires, we draw one path to each destination;
                //  here we store the path to the primary destination)
                const Gate* dest = dests[0];
                auto dest_it = layout.gate_positions.find(dest);
                if (dest_it == layout.gate_positions.end()) {
                    continue;
                }
                int inp_idx = 0;
                for (size_t k = 0; k < dest->get_inputs().size(); k++) {
                    if (dest->get_inputs()[k] == wire) {
                        inp_idx = static_cast<int>(k);
                        break;
                    }
                }
                Vec2 to = gate_input_point(dest_it->second, inp_idx,
                                           static_cast<int>(dest->get_inputs().size()));

                layout.wire_paths[wire] = route_wire(from, to);
            }
        }

    } else {
        // --- Generic fallback layout: arrange by topological depth ---
        auto depths = compute_gate_depths(circuit);
        auto columns = group_by_depth(depths);

        float start_y = LABEL_AREA_HEIGHT + INPUT_MARGIN_TOP;
        int max_depth = columns.empty() ? 0 : columns.rbegin()->first;

        for (auto& [depth, col_gates] : columns) {
            float col_x = static_cast<float>(depth) * COLUMN_SPACING;
            for (size_t row = 0; row < col_gates.size(); row++) {
                float y = start_y + static_cast<float>(row) * (GATE_HEIGHT + GATE_VERTICAL_SPACING);
                layout.gate_positions[col_gates[row]] = {col_x, y, GATE_WIDTH, GATE_HEIGHT};
            }
        }

        // Input/output positions for generic layout
        for (size_t i = 0; i < circuit.input_wires().size(); i++) {
            float y = start_y + static_cast<float>(i) * (GATE_HEIGHT + GATE_VERTICAL_SPACING);
            layout.input_positions.push_back({-COLUMN_SPACING * 0.5f, y});
        }
        float output_x = static_cast<float>(max_depth + 1) * COLUMN_SPACING;
        for (size_t i = 0; i < circuit.output_wires().size(); i++) {
            float y = start_y + static_cast<float>(i) * (GATE_HEIGHT + GATE_VERTICAL_SPACING);
            layout.output_positions.push_back({output_x, y});
        }

        // Wire routing for generic layout
        for (auto& wire_ptr : circuit.wires()) {
            const Wire* wire = wire_ptr.get();
            const Gate* src_gate = wire->get_source();
            const auto& dests = wire->get_destinations();

            if (src_gate == nullptr && !dests.empty()) {
                for (size_t idx = 0; idx < circuit.input_wires().size(); idx++) {
                    if (circuit.input_wires()[idx] == wire) {
                        Vec2 from = layout.input_positions[idx];
                        const Gate* dest = dests[0];
                        auto it = layout.gate_positions.find(dest);
                        if (it != layout.gate_positions.end()) {
                            Vec2 to = gate_input_point(it->second, 0,
                                                       static_cast<int>(dest->get_inputs().size()));
                            layout.wire_paths[wire] = route_wire(from, to);
                        }
                        break;
                    }
                }
            } else if (src_gate != nullptr) {
                auto src_it = layout.gate_positions.find(src_gate);
                if (src_it == layout.gate_positions.end()) {
                    continue;
                }
                Vec2 from = gate_output_point(src_it->second);

                if (dests.empty()) {
                    for (size_t idx = 0; idx < circuit.output_wires().size(); idx++) {
                        if (circuit.output_wires()[idx] == wire) {
                            layout.wire_paths[wire] =
                                route_wire(from, layout.output_positions[idx]);
                            break;
                        }
                    }
                } else {
                    const Gate* dest = dests[0];
                    auto dest_it = layout.gate_positions.find(dest);
                    if (dest_it != layout.gate_positions.end()) {
                        int inp_idx = 0;
                        for (size_t k = 0; k < dest->get_inputs().size(); k++) {
                            if (dest->get_inputs()[k] == wire) {
                                inp_idx = static_cast<int>(k);
                                break;
                            }
                        }
                        Vec2 to = gate_input_point(dest_it->second, inp_idx,
                                                   static_cast<int>(dest->get_inputs().size()));
                        layout.wire_paths[wire] = route_wire(from, to);
                    }
                }
            }
        }
    }

    // Compute bounding box
    float min_x = 1e9f, min_y = 1e9f, max_x = -1e9f, max_y = -1e9f;
    for (auto& [gate, rect] : layout.gate_positions) {
        min_x = std::min(min_x, rect.x);
        min_y = std::min(min_y, rect.y);
        max_x = std::max(max_x, rect.x + rect.w);
        max_y = std::max(max_y, rect.y + rect.h);
    }
    for (auto& pos : layout.input_positions) {
        min_x = std::min(min_x, pos.x);
        min_y = std::min(min_y, pos.y);
        max_x = std::max(max_x, pos.x);
        max_y = std::max(max_y, pos.y);
    }
    for (auto& pos : layout.output_positions) {
        min_x = std::min(min_x, pos.x);
        min_y = std::min(min_y, pos.y);
        max_x = std::max(max_x, pos.x);
        max_y = std::max(max_y, pos.y);
    }

    float padding = 2.0f;
    layout.bounding_box = {min_x - padding, min_y - padding, (max_x - min_x) + 2 * padding,
                           (max_y - min_y) + 2 * padding};

    return layout;
}

} // namespace gateflow
