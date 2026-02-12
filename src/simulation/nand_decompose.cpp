/// @file nand_decompose.cpp
/// @brief Replaces all non-NAND gates with NAND-equivalent sub-circuits

#include "simulation/nand_decompose.hpp"

#include <algorithm>
#include <vector>

namespace gateflow {

namespace {

/// Helper: connect a wire as input to a gate, updating both sides
void link_input(Wire* wire, Gate* gate) {
    gate->add_input(wire);
    wire->add_destination(gate);
}

/// Helper: connect a wire as output from a gate, updating both sides
void link_output(Wire* wire, Gate* gate) {
    gate->set_output(wire);
    wire->set_source(gate);
}

/// Helper: disconnect a gate from all its input wires' destination lists,
/// then clear the gate's own input list
void disconnect_inputs(Gate* gate) {
    for (Wire* w : gate->get_inputs()) {
        w->remove_destination(gate);
    }
    gate->clear_inputs();
}

} // namespace

void decompose_to_nand(Circuit& circuit) {
    // Collect gates to decompose (snapshot the current list; we'll add new gates)
    std::vector<Gate*> original_gates;
    for (auto& g : circuit.gates()) {
        if (g->get_type() != GateType::NAND) {
            original_gates.push_back(g.get());
        }
    }

    for (Gate* gate : original_gates) {
        GateType type = gate->get_type();
        const auto& inputs = gate->get_inputs();
        Wire* original_output = gate->get_output();

        switch (type) {
        case GateType::NOT: {
            // NOT(A) = NAND(A, A)
            Wire* a = inputs[0];
            disconnect_inputs(gate);
            gate->set_type(GateType::NAND);
            link_input(a, gate);
            link_input(a, gate);
            break;
        }

        case GateType::BUFFER: {
            // BUFFER(A) = NAND(NAND(A,A), NAND(A,A))
            Wire* a = inputs[0];

            disconnect_inputs(gate);
            gate->set_type(GateType::NAND);
            link_input(a, gate);
            link_input(a, gate);

            Wire* not_out = circuit.add_wire();
            link_output(not_out, gate);

            Gate* nand2 = circuit.add_gate(GateType::NAND);
            link_input(not_out, nand2);
            link_input(not_out, nand2);
            link_output(original_output, nand2);
            break;
        }

        case GateType::AND: {
            // AND(A,B) = NAND(NAND(A,B), NAND(A,B))
            Wire* a = inputs[0];
            Wire* b = inputs[1];

            disconnect_inputs(gate);
            gate->set_type(GateType::NAND);
            link_input(a, gate);
            link_input(b, gate);

            Wire* nand_out = circuit.add_wire();
            link_output(nand_out, gate);

            Gate* nand2 = circuit.add_gate(GateType::NAND);
            link_input(nand_out, nand2);
            link_input(nand_out, nand2);
            link_output(original_output, nand2);
            break;
        }

        case GateType::OR: {
            // OR(A,B) = NAND(NAND(A,A), NAND(B,B))
            Wire* a = inputs[0];
            Wire* b = inputs[1];

            disconnect_inputs(gate);
            gate->set_type(GateType::NAND);
            link_input(a, gate);
            link_input(a, gate);

            Wire* not_a_out = circuit.add_wire();
            link_output(not_a_out, gate);

            Gate* not_b = circuit.add_gate(GateType::NAND);
            link_input(b, not_b);
            link_input(b, not_b);
            Wire* not_b_out = circuit.add_wire();
            link_output(not_b_out, not_b);

            Gate* final_nand = circuit.add_gate(GateType::NAND);
            link_input(not_a_out, final_nand);
            link_input(not_b_out, final_nand);
            link_output(original_output, final_nand);
            break;
        }

        case GateType::XOR: {
            // XOR(A,B) = NAND(NAND(A, NAND(A,B)), NAND(B, NAND(A,B)))
            Wire* a = inputs[0];
            Wire* b = inputs[1];

            disconnect_inputs(gate);
            gate->set_type(GateType::NAND);
            link_input(a, gate);
            link_input(b, gate);

            Wire* nand_ab_out = circuit.add_wire();
            link_output(nand_ab_out, gate);

            Gate* nand_a = circuit.add_gate(GateType::NAND);
            link_input(a, nand_a);
            link_input(nand_ab_out, nand_a);
            Wire* nand_a_out = circuit.add_wire();
            link_output(nand_a_out, nand_a);

            Gate* nand_b = circuit.add_gate(GateType::NAND);
            link_input(b, nand_b);
            link_input(nand_ab_out, nand_b);
            Wire* nand_b_out = circuit.add_wire();
            link_output(nand_b_out, nand_b);

            Gate* final_nand = circuit.add_gate(GateType::NAND);
            link_input(nand_a_out, final_nand);
            link_input(nand_b_out, final_nand);
            link_output(original_output, final_nand);
            break;
        }

        case GateType::NAND:
            break;
        }
    }

    // Re-finalize to recompute topological order with the new gates
    circuit.finalize();
}

} // namespace gateflow
