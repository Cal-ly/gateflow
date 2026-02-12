/// @file test_circuit.cpp
/// @brief Tests for the Circuit class — construction, topological sort, propagation

#include <catch2/catch_test_macros.hpp>

#include "simulation/circuit.hpp"

using namespace gateflow;

TEST_CASE("Simple NOT circuit", "[circuit]") {
    Circuit circuit;

    Wire* input = circuit.add_wire();
    circuit.mark_input(input);

    Gate* not_gate = circuit.add_gate(GateType::NOT);

    Wire* output = circuit.add_wire();
    circuit.mark_output(output);

    circuit.connect(input, nullptr, not_gate);
    circuit.connect(output, not_gate, nullptr);

    circuit.finalize();

    SECTION("NOT(false) = true") {
        circuit.set_input(0, false);
        (void)circuit.propagate();
        CHECK(circuit.get_output(0) == true);
    }

    SECTION("NOT(true) = false") {
        circuit.set_input(0, true);
        (void)circuit.propagate();
        CHECK(circuit.get_output(0) == false);
    }
}

TEST_CASE("Two-gate chain: NOT -> NOT = BUFFER", "[circuit]") {
    Circuit circuit;

    Wire* input = circuit.add_wire();
    circuit.mark_input(input);

    Gate* not1 = circuit.add_gate(GateType::NOT);
    Gate* not2 = circuit.add_gate(GateType::NOT);

    Wire* mid = circuit.add_wire();
    Wire* output = circuit.add_wire();
    circuit.mark_output(output);

    circuit.connect(input, nullptr, not1);
    circuit.connect(mid, not1, not2);
    circuit.connect(output, not2, nullptr);

    circuit.finalize();

    circuit.set_input(0, true);
    (void)circuit.propagate();
    CHECK(circuit.get_output(0) == true);

    circuit.set_input(0, false);
    (void)circuit.propagate();
    CHECK(circuit.get_output(0) == false);
}

TEST_CASE("Propagation result tracks changes", "[circuit]") {
    Circuit circuit;

    Wire* input = circuit.add_wire();
    circuit.mark_input(input);

    Gate* not_gate = circuit.add_gate(GateType::NOT);

    Wire* output = circuit.add_wire();
    circuit.mark_output(output);

    circuit.connect(input, nullptr, not_gate);
    circuit.connect(output, not_gate, nullptr);

    circuit.finalize();

    // First propagation with default (false) input — NOT(false) = true
    circuit.set_input(0, false);
    auto result = circuit.propagate();
    // The gate changes from default false to true
    CHECK(result.changed_gates.size() == 1);

    // Propagate again with same input — nothing should change
    auto result2 = circuit.propagate();
    CHECK(result2.changed_gates.empty());
}

TEST_CASE("Topological order is computed correctly", "[circuit]") {
    Circuit circuit;

    Wire* input = circuit.add_wire();
    circuit.mark_input(input);

    Gate* not1 = circuit.add_gate(GateType::NOT);
    Gate* not2 = circuit.add_gate(GateType::NOT);

    Wire* mid = circuit.add_wire();
    Wire* output = circuit.add_wire();
    circuit.mark_output(output);

    circuit.connect(input, nullptr, not1);
    circuit.connect(mid, not1, not2);
    circuit.connect(output, not2, nullptr);

    circuit.finalize();

    auto& order = circuit.topological_order();
    REQUIRE(order.size() == 2);
    CHECK(order[0] == not1);
    CHECK(order[1] == not2);
}

TEST_CASE("Circuit rejects propagation before finalize", "[circuit]") {
    Circuit circuit;
    CHECK_THROWS_AS(circuit.propagate(), std::runtime_error);
}

TEST_CASE("Input/output index bounds checking", "[circuit]") {
    Circuit circuit;
    Wire* w = circuit.add_wire();
    circuit.mark_input(w);
    circuit.mark_output(w);

    CHECK_THROWS_AS(circuit.set_input(1, true), std::out_of_range);
    CHECK_THROWS_AS(circuit.get_output(1), std::out_of_range);
}

TEST_CASE("connect() rejects multi-driver wire source reassignment", "[circuit]") {
    Circuit circuit;

    Gate* g1 = circuit.add_gate(GateType::NOT);
    Gate* g2 = circuit.add_gate(GateType::NOT);
    Wire* w = circuit.add_wire();

    circuit.connect(w, g1, nullptr);
    CHECK_THROWS_AS(circuit.connect(w, g2, nullptr), std::runtime_error);
}

TEST_CASE("connect() rejects multiple output wires for one gate", "[circuit]") {
    Circuit circuit;

    Gate* g = circuit.add_gate(GateType::NOT);
    Wire* w1 = circuit.add_wire();
    Wire* w2 = circuit.add_wire();

    circuit.connect(w1, g, nullptr);
    CHECK_THROWS_AS(circuit.connect(w2, g, nullptr), std::runtime_error);
}

TEST_CASE("finalize() rejects inconsistent bidirectional connectivity", "[circuit]") {
    Circuit circuit;

    Wire* in = circuit.add_wire();
    circuit.mark_input(in);
    Gate* not_gate = circuit.add_gate(GateType::NOT);
    Wire* out = circuit.add_wire();
    circuit.mark_output(out);

    // Build one-sided links intentionally (bypassing Circuit::connect)
    not_gate->add_input(in);
    not_gate->set_output(out);

    CHECK_THROWS_AS(circuit.finalize(), std::runtime_error);
}
