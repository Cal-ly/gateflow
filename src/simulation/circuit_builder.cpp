/// @file circuit_builder.cpp
/// @brief Implementation of standard circuit builder functions

#include "simulation/circuit_builder.hpp"

#include <stdexcept>

namespace gateflow {

std::unique_ptr<Circuit> build_half_adder() {
    auto circuit = std::make_unique<Circuit>();

    // Primary inputs
    Wire* wire_a = circuit->add_wire();
    Wire* wire_b = circuit->add_wire();
    circuit->mark_input(wire_a); // index 0 = A
    circuit->mark_input(wire_b); // index 1 = B

    // Gates
    Gate* xor_gate = circuit->add_gate(GateType::XOR); // Sum = A XOR B
    Gate* and_gate = circuit->add_gate(GateType::AND); // Carry = A AND B

    // Internal and output wires
    Wire* wire_sum = circuit->add_wire();
    Wire* wire_carry = circuit->add_wire();

    // Connect inputs to XOR gate
    circuit->connect(wire_a, nullptr, xor_gate);
    circuit->connect(wire_b, nullptr, and_gate);
    // Also fan-in A and B to both gates
    circuit->connect(wire_b, nullptr, xor_gate);
    circuit->connect(wire_a, nullptr, and_gate);

    // Connect gate outputs
    circuit->connect(wire_sum, xor_gate, nullptr);
    circuit->connect(wire_carry, and_gate, nullptr);

    // Mark outputs
    circuit->mark_output(wire_sum);   // index 0 = Sum
    circuit->mark_output(wire_carry); // index 1 = Carry

    circuit->finalize();
    return circuit;
}

std::unique_ptr<Circuit> build_full_adder() {
    auto circuit = std::make_unique<Circuit>();

    // Primary inputs
    Wire* wire_a = circuit->add_wire();
    Wire* wire_b = circuit->add_wire();
    Wire* wire_cin = circuit->add_wire();
    circuit->mark_input(wire_a);   // index 0 = A
    circuit->mark_input(wire_b);   // index 1 = B
    circuit->mark_input(wire_cin); // index 2 = Cin

    // Half adder 1: A XOR B, A AND B
    Gate* xor1 = circuit->add_gate(GateType::XOR);
    Gate* and1 = circuit->add_gate(GateType::AND);

    Wire* xor1_out = circuit->add_wire();
    Wire* and1_out = circuit->add_wire();

    circuit->connect(wire_a, nullptr, xor1);
    circuit->connect(wire_b, nullptr, xor1);
    circuit->connect(wire_a, nullptr, and1);
    circuit->connect(wire_b, nullptr, and1);
    circuit->connect(xor1_out, xor1, nullptr);
    circuit->connect(and1_out, and1, nullptr);

    // Half adder 2: (A XOR B) XOR Cin, (A XOR B) AND Cin
    Gate* xor2 = circuit->add_gate(GateType::XOR);
    Gate* and2 = circuit->add_gate(GateType::AND);

    Wire* sum_wire = circuit->add_wire();
    Wire* and2_out = circuit->add_wire();

    circuit->connect(xor1_out, nullptr, xor2);
    circuit->connect(wire_cin, nullptr, xor2);
    circuit->connect(xor1_out, nullptr, and2);
    circuit->connect(wire_cin, nullptr, and2);
    circuit->connect(sum_wire, xor2, nullptr);
    circuit->connect(and2_out, and2, nullptr);

    // OR gate: Cout = (A AND B) OR ((A XOR B) AND Cin)
    Gate* or_gate = circuit->add_gate(GateType::OR);
    Wire* cout_wire = circuit->add_wire();

    circuit->connect(and1_out, nullptr, or_gate);
    circuit->connect(and2_out, nullptr, or_gate);
    circuit->connect(cout_wire, or_gate, nullptr);

    // Mark outputs
    circuit->mark_output(sum_wire);  // index 0 = Sum
    circuit->mark_output(cout_wire); // index 1 = Cout

    circuit->finalize();
    return circuit;
}

std::unique_ptr<Circuit> build_ripple_carry_adder(int bits) {
    if (bits < 1) {
        throw std::invalid_argument("Ripple-carry adder requires at least 1 bit");
    }

    auto circuit = std::make_unique<Circuit>();

    // Create input wires: A[0..bits-1], B[0..bits-1]
    std::vector<Wire*> a_wires(bits);
    std::vector<Wire*> b_wires(bits);

    for (int i = 0; i < bits; i++) {
        a_wires[i] = circuit->add_wire();
        circuit->mark_input(a_wires[i]); // indices 0..bits-1
    }
    for (int i = 0; i < bits; i++) {
        b_wires[i] = circuit->add_wire();
        circuit->mark_input(b_wires[i]); // indices bits..2*bits-1
    }

    // Build chain of full adders
    std::vector<Wire*> sum_wires(bits);
    Wire* carry_in = nullptr; // First adder has no carry-in (implicitly 0)

    for (int i = 0; i < bits; i++) {
        if (i == 0) {
            // First bit: use a half adder (no carry-in)
            Gate* xor_gate = circuit->add_gate(GateType::XOR);
            Gate* and_gate = circuit->add_gate(GateType::AND);

            Wire* sum_out = circuit->add_wire();
            Wire* carry_out = circuit->add_wire();

            circuit->connect(a_wires[i], nullptr, xor_gate);
            circuit->connect(b_wires[i], nullptr, xor_gate);
            circuit->connect(a_wires[i], nullptr, and_gate);
            circuit->connect(b_wires[i], nullptr, and_gate);
            circuit->connect(sum_out, xor_gate, nullptr);
            circuit->connect(carry_out, and_gate, nullptr);

            sum_wires[i] = sum_out;
            carry_in = carry_out;
        } else {
            // Subsequent bits: full adder with carry chain
            // Half adder 1: A[i] XOR B[i], A[i] AND B[i]
            Gate* xor1 = circuit->add_gate(GateType::XOR);
            Gate* and1 = circuit->add_gate(GateType::AND);
            Wire* xor1_out = circuit->add_wire();
            Wire* and1_out = circuit->add_wire();

            circuit->connect(a_wires[i], nullptr, xor1);
            circuit->connect(b_wires[i], nullptr, xor1);
            circuit->connect(a_wires[i], nullptr, and1);
            circuit->connect(b_wires[i], nullptr, and1);
            circuit->connect(xor1_out, xor1, nullptr);
            circuit->connect(and1_out, and1, nullptr);

            // Half adder 2: (A XOR B) XOR Cin, (A XOR B) AND Cin
            Gate* xor2 = circuit->add_gate(GateType::XOR);
            Gate* and2 = circuit->add_gate(GateType::AND);
            Wire* sum_out = circuit->add_wire();
            Wire* and2_out = circuit->add_wire();

            circuit->connect(xor1_out, nullptr, xor2);
            circuit->connect(carry_in, nullptr, xor2);
            circuit->connect(xor1_out, nullptr, and2);
            circuit->connect(carry_in, nullptr, and2);
            circuit->connect(sum_out, xor2, nullptr);
            circuit->connect(and2_out, and2, nullptr);

            // OR gate: carry-out
            Gate* or_gate = circuit->add_gate(GateType::OR);
            Wire* carry_out = circuit->add_wire();

            circuit->connect(and1_out, nullptr, or_gate);
            circuit->connect(and2_out, nullptr, or_gate);
            circuit->connect(carry_out, or_gate, nullptr);

            sum_wires[i] = sum_out;
            carry_in = carry_out;
        }
    }

    // Mark outputs: Sum[0..bits-1], then final carry-out
    for (int i = 0; i < bits; i++) {
        circuit->mark_output(sum_wires[i]);
    }
    circuit->mark_output(carry_in); // index bits = carry-out (8th bit for 7-bit adder)

    circuit->finalize();
    return circuit;
}

} // namespace gateflow
