/// @file circuit.cpp
/// @brief Circuit construction, topological sort (Kahn's algorithm), and propagation

#include "simulation/circuit.hpp"

#include <algorithm>
#include <queue>
#include <stdexcept>
#include <unordered_map>

namespace gateflow {

Gate* Circuit::add_gate(GateType type) {
    gates_.push_back(std::make_unique<Gate>(next_gate_id_++, type));
    return gates_.back().get();
}

Wire* Circuit::add_wire() {
    wires_.push_back(std::make_unique<Wire>(next_wire_id_++));
    return wires_.back().get();
}

void Circuit::connect(Wire* wire, Gate* source, Gate* destination) {
    if (source != nullptr) {
        wire->set_source(source);
        source->set_output(wire);
    }
    if (destination != nullptr) {
        wire->add_destination(destination);
        destination->add_input(wire);
    }
}

void Circuit::mark_input(Wire* wire) {
    input_wires_.push_back(wire);
}

void Circuit::mark_output(Wire* wire) {
    output_wires_.push_back(wire);
}

void Circuit::finalize() {
    // Kahn's algorithm for topological sort
    // in-degree = number of input wires whose source is another gate
    std::unordered_map<Gate*, int> in_degree;
    for (auto& gate : gates_) {
        in_degree[gate.get()] = 0;
    }

    // Count in-degrees: for each gate, count how many of its input wires
    // come from another gate (have a non-null source)
    for (auto& gate : gates_) {
        for (Wire* input_wire : gate->get_inputs()) {
            if (input_wire->get_source() != nullptr) {
                in_degree[gate.get()]++;
            }
        }
    }

    // Seed the queue with gates that have all primary-input feeds (in_degree 0)
    std::queue<Gate*> ready;
    for (auto& gate : gates_) {
        if (in_degree[gate.get()] == 0) {
            ready.push(gate.get());
        }
    }

    topo_order_.clear();
    topo_order_.reserve(gates_.size());

    while (!ready.empty()) {
        Gate* current = ready.front();
        ready.pop();
        topo_order_.push_back(current);

        // For each gate that depends on current's output wire
        Wire* out = current->get_output();
        if (out == nullptr) {
            continue;
        }
        for (Gate* dest : out->get_destinations()) {
            in_degree[dest]--;
            if (in_degree[dest] == 0) {
                ready.push(dest);
            }
        }
    }

    if (topo_order_.size() != gates_.size()) {
        throw std::runtime_error("Circuit contains a cycle — topological sort failed");
    }

    finalized_ = true;
}

void Circuit::set_input(size_t index, bool value) {
    if (index >= input_wires_.size()) {
        throw std::out_of_range("Input index out of range");
    }
    input_wires_[index]->set_value(value);
}

PropagationResult Circuit::propagate() {
    if (!finalized_) {
        throw std::runtime_error("Circuit must be finalized before propagation");
    }

    PropagationResult result;

    for (Gate* gate : topo_order_) {
        // Gather current input values from this gate's input wires
        std::vector<bool> input_values;
        input_values.reserve(gate->get_inputs().size());
        for (Wire* w : gate->get_inputs()) {
            input_values.push_back(w->get_value());
        }

        bool new_state = evaluate(gate->get_type(), input_values);
        bool old_state = gate->get_state();
        gate->set_state(new_state);
        gate->set_dirty(false);

        // Update the output wire
        Wire* out = gate->get_output();
        if (out != nullptr) {
            out->set_value(new_state);
            if (out->value_changed()) {
                result.changed_wires.push_back(out);
            }
        }

        if (new_state != old_state) {
            result.changed_gates.push_back(gate);
        }
    }

    return result;
}

bool Circuit::get_output(size_t index) const {
    if (index >= output_wires_.size()) {
        throw std::out_of_range("Output index out of range");
    }
    return output_wires_[index]->get_value();
}

} // namespace gateflow
