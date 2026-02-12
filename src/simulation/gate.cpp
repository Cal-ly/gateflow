/// @file gate.cpp
/// @brief Gate evaluation logic and Gate class implementation

#include "simulation/gate.hpp"

#include <stdexcept>

namespace gateflow {

bool evaluate(GateType type, const std::vector<bool>& inputs) {
    switch (type) {
    case GateType::NOT:
        if (inputs.size() != 1) {
            throw std::invalid_argument("NOT gate requires exactly 1 input");
        }
        return !inputs[0];

    case GateType::BUFFER:
        if (inputs.size() != 1) {
            throw std::invalid_argument("BUFFER gate requires exactly 1 input");
        }
        return inputs[0];

    case GateType::AND:
        if (inputs.size() < 2) {
            throw std::invalid_argument("AND gate requires at least 2 inputs");
        }
        for (bool v : inputs) {
            if (!v) {
                return false;
            }
        }
        return true;

    case GateType::NAND:
        if (inputs.size() < 2) {
            throw std::invalid_argument("NAND gate requires at least 2 inputs");
        }
        for (bool v : inputs) {
            if (!v) {
                return true;
            }
        }
        return false;

    case GateType::OR:
        if (inputs.size() < 2) {
            throw std::invalid_argument("OR gate requires at least 2 inputs");
        }
        for (bool v : inputs) {
            if (v) {
                return true;
            }
        }
        return false;

    case GateType::XOR:
        if (inputs.size() < 2) {
            throw std::invalid_argument("XOR gate requires at least 2 inputs");
        }
        {
            bool result = false;
            for (bool v : inputs) {
                result ^= v;
            }
            return result;
        }
    }
    throw std::invalid_argument("Unknown gate type");
}

Gate::Gate(uint32_t id, GateType type) : id_(id), type_(type) {}

void Gate::add_input(Wire* wire) {
    inputs_.push_back(wire);
}

} // namespace gateflow
