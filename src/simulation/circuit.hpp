#pragma once

/// @file circuit.hpp
/// @brief Circuit model — owns gates and wires, provides topological propagation

#include "simulation/gate.hpp"
#include "simulation/wire.hpp"

#include <cstdint>
#include <memory>
#include <vector>

namespace gateflow {

/// Records which gates and wires changed state during a propagation pass
struct PropagationResult {
    std::vector<Gate*> changed_gates;
    std::vector<Wire*> changed_wires;
};

/// A circuit is a directed acyclic graph of gates and wires.
///
/// Construction follows a builder pattern:
///   1. Create gates with add_gate()
///   2. Create wires with add_wire()
///   3. Connect them with connect()
///   4. Designate inputs/outputs with mark_input() / mark_output()
///   5. Call finalize() to compute topological order
///
/// After finalization, call set_input() and propagate() to simulate.
class Circuit {
  public:
    Circuit() = default;

    // Non-copyable, movable
    Circuit(const Circuit&) = delete;
    Circuit& operator=(const Circuit&) = delete;
    Circuit(Circuit&&) = default;
    Circuit& operator=(Circuit&&) = default;

    /// Creates a new gate in this circuit and returns a non-owning pointer
    Gate* add_gate(GateType type);

    /// Creates a new wire in this circuit and returns a non-owning pointer
    Wire* add_wire();

    /// Connects a wire from a gate's output to another gate's input.
    /// @param wire The wire to connect
    /// @param source The gate driving this wire (nullptr for primary inputs)
    /// @param destination The gate receiving this wire
    void connect(Wire* wire, Gate* source, Gate* destination);

    /// Marks a wire as a primary input (ordered; index matters for bit position)
    void mark_input(Wire* wire);

    /// Marks a wire as a primary output (ordered; index matters for bit position)
    void mark_output(Wire* wire);

    /// Computes topological order. Must be called after all connections are made.
    /// @throws std::runtime_error if the circuit contains a cycle
    void finalize();

    /// Sets the value of the i-th primary input wire
    void set_input(size_t index, bool value);

    /// Evaluates all gates in topological order, propagating signals from
    /// inputs to outputs. Returns which gates/wires changed.
    [[nodiscard]] PropagationResult propagate();

    /// Read the value of the i-th primary output wire
    [[nodiscard]] bool get_output(size_t index) const;

    // --- Accessors ---
    [[nodiscard]] const std::vector<std::unique_ptr<Gate>>& gates() const { return gates_; }
    [[nodiscard]] const std::vector<std::unique_ptr<Wire>>& wires() const { return wires_; }
    [[nodiscard]] std::vector<std::unique_ptr<Gate>>& gates() { return gates_; }
    [[nodiscard]] std::vector<std::unique_ptr<Wire>>& wires() { return wires_; }
    [[nodiscard]] const std::vector<Wire*>& input_wires() const { return input_wires_; }
    [[nodiscard]] const std::vector<Wire*>& output_wires() const { return output_wires_; }
    [[nodiscard]] const std::vector<Gate*>& topological_order() const { return topo_order_; }
    [[nodiscard]] size_t num_inputs() const { return input_wires_.size(); }
    [[nodiscard]] size_t num_outputs() const { return output_wires_.size(); }

  private:
    /// Validates bidirectional connectivity invariants between gates and wires.
    /// @throws std::runtime_error if an inconsistent link is found.
    void validate_connectivity() const;

    uint32_t next_gate_id_ = 0;
    uint32_t next_wire_id_ = 0;

    std::vector<std::unique_ptr<Gate>> gates_;
    std::vector<std::unique_ptr<Wire>> wires_;
    std::vector<Wire*> input_wires_;
    std::vector<Wire*> output_wires_;
    std::vector<Gate*> topo_order_;
    bool finalized_ = false;
};

} // namespace gateflow
