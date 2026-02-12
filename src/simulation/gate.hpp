#pragma once

/// @file gate.hpp
/// @brief Logic gate model — types, evaluation, and the Gate class

#include <cstdint>
#include <string_view>
#include <vector>

namespace gateflow {

class Wire; // Forward declaration

/// Types of logic gates supported by the simulator
enum class GateType { NAND, AND, OR, XOR, NOT, BUFFER };

/// Returns the human-readable name of a gate type
[[nodiscard]] constexpr std::string_view gate_type_name(GateType type) {
    switch (type) {
    case GateType::NAND:
        return "NAND";
    case GateType::AND:
        return "AND";
    case GateType::OR:
        return "OR";
    case GateType::XOR:
        return "XOR";
    case GateType::NOT:
        return "NOT";
    case GateType::BUFFER:
        return "BUFFER";
    }
    return "UNKNOWN";
}

/// Evaluates a logic gate given its type and input values.
/// This is a pure function with no side effects.
/// @throws std::invalid_argument if the input count is wrong for the gate type
[[nodiscard]] bool evaluate(GateType type, const std::vector<bool>& inputs);

/// Represents a single logic gate in a circuit DAG.
///
/// A gate has typed logic (AND, XOR, etc.), a set of input wires,
/// a single output wire, and a cached output state.
class Gate {
  public:
    /// Construct a gate with a unique ID and type
    explicit Gate(uint32_t id, GateType type);

    [[nodiscard]] uint32_t get_id() const { return id_; }
    [[nodiscard]] GateType get_type() const { return type_; }
    [[nodiscard]] bool get_state() const { return state_; }
    [[nodiscard]] bool is_dirty() const { return dirty_; }
    [[nodiscard]] Wire* get_output() const { return output_; }
    [[nodiscard]] const std::vector<Wire*>& get_inputs() const { return inputs_; }

    void set_state(bool state) { state_ = state; }
    void set_dirty(bool dirty) { dirty_ = dirty; }
    void set_type(GateType type) { type_ = type; }
    void set_output(Wire* wire) { output_ = wire; }

    /// Appends a wire to this gate's input list
    void add_input(Wire* wire);

    /// Removes all input wires
    void clear_inputs() { inputs_.clear(); }

  private:
    uint32_t id_;
    GateType type_;
    std::vector<Wire*> inputs_;
    Wire* output_ = nullptr;
    bool state_ = false;
    bool dirty_ = true;
};

} // namespace gateflow
