#pragma once

/// @file wire.hpp
/// @brief Wire model — connects gate outputs to gate inputs

#include <cstdint>
#include <vector>

namespace gateflow {

class Gate; // Forward declaration

/// Represents a wire connecting a gate output to one or more gate inputs.
///
/// A wire with a nullptr source is a primary input wire (its value is set
/// externally). previous_value is retained for edge detection and animation.
class Wire {
  public:
    /// Construct a wire with a unique ID
    explicit Wire(uint32_t id);

    [[nodiscard]] uint32_t get_id() const { return id_; }
    [[nodiscard]] bool get_value() const { return value_; }
    [[nodiscard]] bool get_previous_value() const { return previous_value_; }
    [[nodiscard]] Gate* get_source() const { return source_; }
    [[nodiscard]] const std::vector<Gate*>& get_destinations() const { return destinations_; }

    /// Sets the wire's value, saving the old value in previous_value
    void set_value(bool value);

    void set_source(Gate* gate) { source_ = gate; }

    /// Adds a destination gate that reads from this wire
    void add_destination(Gate* gate);

    /// Removes one occurrence of a gate from the destination list
    void remove_destination(Gate* gate);

    /// Returns true if the value changed on the last set_value call
    [[nodiscard]] bool value_changed() const { return value_ != previous_value_; }

  private:
    uint32_t id_;
    bool value_ = false;
    bool previous_value_ = false;
    Gate* source_ = nullptr;
    std::vector<Gate*> destinations_;
};

} // namespace gateflow
