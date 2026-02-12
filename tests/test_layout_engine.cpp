/// @file test_layout_engine.cpp
/// @brief Tests for layout determinism and wire fan-out routing coverage

#include <catch2/catch_test_macros.hpp>

#include "rendering/layout_engine.hpp"
#include "simulation/circuit.hpp"
#include "simulation/circuit_builder.hpp"

using namespace gateflow;

namespace {

Circuit build_fanout_circuit() {
    Circuit circuit;

    Wire* in = circuit.add_wire();
    circuit.mark_input(in);

    Gate* not1 = circuit.add_gate(GateType::NOT);
    Gate* not2 = circuit.add_gate(GateType::NOT);

    Wire* out1 = circuit.add_wire();
    Wire* out2 = circuit.add_wire();
    circuit.mark_output(out1);
    circuit.mark_output(out2);

    circuit.connect(in, nullptr, not1);
    circuit.connect(in, nullptr, not2);
    circuit.connect(out1, not1, nullptr);
    circuit.connect(out2, not2, nullptr);

    circuit.finalize();
    return circuit;
}

} // namespace

TEST_CASE("Layout routes one branch per wire destination", "[layout]") {
    Circuit circuit = build_fanout_circuit();
    Layout layout = compute_layout(circuit);

    for (const auto& wire_ptr : circuit.wires()) {
        const Wire* wire = wire_ptr.get();
        const auto& dests = wire->get_destinations();

        int expected_branches = 0;
        if (dests.empty()) {
            if (wire->get_source() != nullptr) {
                expected_branches = 1; // output-only wire
            }
        } else {
            expected_branches = static_cast<int>(dests.size());
        }

        auto it = layout.wire_paths.find(wire);
        int actual_branches = (it == layout.wire_paths.end()) ? 0 : static_cast<int>(it->second.size());

        INFO("Wire id=" << wire->get_id());
        CHECK(actual_branches == expected_branches);

        if (it != layout.wire_paths.end()) {
            for (const WirePath& branch : it->second) {
                CHECK(branch.points.size() >= 2);
                CHECK(branch.cumulative_lengths.size() == branch.points.size());
                CHECK(branch.total_length >= 0.0f);
            }
        }
    }
}

TEST_CASE("Layout is deterministic for same circuit", "[layout]") {
    auto circuit = build_ripple_carry_adder(7);

    Layout a = compute_layout(*circuit);
    Layout b = compute_layout(*circuit);

    CHECK(a.bounding_box.x == b.bounding_box.x);
    CHECK(a.bounding_box.y == b.bounding_box.y);
    CHECK(a.bounding_box.w == b.bounding_box.w);
    CHECK(a.bounding_box.h == b.bounding_box.h);

    CHECK(a.gate_positions.size() == b.gate_positions.size());
    CHECK(a.wire_paths.size() == b.wire_paths.size());
}
