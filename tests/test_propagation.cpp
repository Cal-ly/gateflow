/// @file test_propagation.cpp
/// @brief Tests for topological sort correctness and propagation behavior

#include <catch2/catch_test_macros.hpp>

#include "simulation/circuit.hpp"

#include <algorithm>
#include <unordered_map>

using namespace gateflow;

TEST_CASE("Topological order respects dependencies", "[propagation]") {
    // Build: input -> AND -> NOT -> output
    //              \---^
    Circuit circuit;

    Wire* w_a = circuit.add_wire();
    Wire* w_b = circuit.add_wire();
    circuit.mark_input(w_a);
    circuit.mark_input(w_b);

    Gate* and_gate = circuit.add_gate(GateType::AND);
    Gate* not_gate = circuit.add_gate(GateType::NOT);

    Wire* and_out = circuit.add_wire();
    Wire* not_out = circuit.add_wire();
    circuit.mark_output(not_out);

    circuit.connect(w_a, nullptr, and_gate);
    circuit.connect(w_b, nullptr, and_gate);
    circuit.connect(and_out, and_gate, not_gate);
    circuit.connect(not_out, not_gate, nullptr);

    circuit.finalize();

    auto& order = circuit.topological_order();
    REQUIRE(order.size() == 2);

    // AND must come before NOT
    auto and_pos = std::find(order.begin(), order.end(), and_gate);
    auto not_pos = std::find(order.begin(), order.end(), not_gate);
    CHECK(and_pos < not_pos);
}

TEST_CASE("Diamond-shaped circuit propagates correctly", "[propagation]") {
    // input_a ---v          v--- output
    //          XOR -> wire -> NOT
    // input_b ---^
    //
    // But let's do a diamond: input fans out to two gates, then results merge
    //
    //       input
    //      /       |
    //   NOT1     NOT2
    //      |       /
    //       AND
    //        |
    //      output
    Circuit circuit;

    Wire* input = circuit.add_wire();
    circuit.mark_input(input);

    Gate* not1 = circuit.add_gate(GateType::NOT);
    Gate* not2 = circuit.add_gate(GateType::NOT);
    Gate* and_gate = circuit.add_gate(GateType::AND);

    Wire* not1_out = circuit.add_wire();
    Wire* not2_out = circuit.add_wire();
    Wire* output = circuit.add_wire();
    circuit.mark_output(output);

    circuit.connect(input, nullptr, not1);
    circuit.connect(input, nullptr, not2);
    circuit.connect(not1_out, not1, and_gate);
    circuit.connect(not2_out, not2, and_gate);
    circuit.connect(output, and_gate, nullptr);

    circuit.finalize();

    // NOT(x) AND NOT(x) = NOT(x) for any x
    circuit.set_input(0, false);
    (void)circuit.propagate();
    CHECK(circuit.get_output(0) == true); // NOT(false) AND NOT(false) = true

    circuit.set_input(0, true);
    (void)circuit.propagate();
    CHECK(circuit.get_output(0) == false); // NOT(true) AND NOT(true) = false
}

TEST_CASE("Fan-out: one wire drives multiple gates", "[propagation]") {
    // input -> wire -> NOT1 -> out1
    //               -> NOT2 -> out2
    Circuit circuit;

    Wire* input = circuit.add_wire();
    circuit.mark_input(input);

    Gate* not1 = circuit.add_gate(GateType::NOT);
    Gate* not2 = circuit.add_gate(GateType::NOT);

    Wire* out1 = circuit.add_wire();
    Wire* out2 = circuit.add_wire();
    circuit.mark_output(out1);
    circuit.mark_output(out2);

    circuit.connect(input, nullptr, not1);
    circuit.connect(input, nullptr, not2);
    circuit.connect(out1, not1, nullptr);
    circuit.connect(out2, not2, nullptr);

    circuit.finalize();

    circuit.set_input(0, true);
    (void)circuit.propagate();
    CHECK(circuit.get_output(0) == false);
    CHECK(circuit.get_output(1) == false);

    circuit.set_input(0, false);
    (void)circuit.propagate();
    CHECK(circuit.get_output(0) == true);
    CHECK(circuit.get_output(1) == true);
}

TEST_CASE("Multiple propagations update state correctly", "[propagation]") {
    auto make_and_circuit = []() {
        Circuit circuit;
        Wire* a = circuit.add_wire();
        Wire* b = circuit.add_wire();
        circuit.mark_input(a);
        circuit.mark_input(b);

        Gate* and_gate = circuit.add_gate(GateType::AND);
        Wire* out = circuit.add_wire();
        circuit.mark_output(out);

        circuit.connect(a, nullptr, and_gate);
        circuit.connect(b, nullptr, and_gate);
        circuit.connect(out, and_gate, nullptr);

        circuit.finalize();
        return circuit;
    };

    auto circuit = make_and_circuit();

    // Start with 1 AND 1 = 1
    circuit.set_input(0, true);
    circuit.set_input(1, true);
    (void)circuit.propagate();
    CHECK(circuit.get_output(0) == true);

    // Change to 1 AND 0 = 0
    circuit.set_input(1, false);
    (void)circuit.propagate();
    CHECK(circuit.get_output(0) == false);

    // Change to 0 AND 0 = 0
    circuit.set_input(0, false);
    (void)circuit.propagate();
    CHECK(circuit.get_output(0) == false);
}

TEST_CASE("PropagationResult accurately reports wire changes", "[propagation]") {
    Circuit circuit;

    Wire* input = circuit.add_wire();
    circuit.mark_input(input);

    Gate* not_gate = circuit.add_gate(GateType::NOT);
    Wire* output = circuit.add_wire();
    circuit.mark_output(output);

    circuit.connect(input, nullptr, not_gate);
    circuit.connect(output, not_gate, nullptr);

    circuit.finalize();

    // First propagation: output goes false -> true
    circuit.set_input(0, false);
    auto r1 = circuit.propagate();
    CHECK(r1.changed_wires.size() == 1);
    CHECK(r1.changed_wires[0] == output);

    // Same input, no change
    circuit.set_input(0, false);
    auto r2 = circuit.propagate();
    CHECK(r2.changed_wires.empty());

    // Toggle input: output goes true -> false
    circuit.set_input(0, true);
    auto r3 = circuit.propagate();
    CHECK(r3.changed_wires.size() >= 1);
}
