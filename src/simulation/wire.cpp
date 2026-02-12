/// @file wire.cpp
/// @brief Wire class implementation

#include "simulation/wire.hpp"

#include <algorithm>

namespace gateflow {

Wire::Wire(uint32_t id) : id_(id) {}

void Wire::set_value(bool value) {
    previous_value_ = value_;
    value_ = value;
}

void Wire::add_destination(Gate* gate) {
    destinations_.push_back(gate);
}

void Wire::remove_destination(Gate* gate) {
    auto it = std::find(destinations_.begin(), destinations_.end(), gate);
    if (it != destinations_.end()) {
        destinations_.erase(it);
    }
}

} // namespace gateflow
