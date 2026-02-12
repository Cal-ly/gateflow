#pragma once

/// @file circuit_builder.hpp
/// @brief Factory functions that construct standard circuits from primitive gates

#include "simulation/circuit.hpp"

#include <memory>

namespace gateflow {

/// Builds a half adder circuit.
/// Inputs: A (index 0), B (index 1)
/// Outputs: Sum (index 0), Carry (index 1)
[[nodiscard]] std::unique_ptr<Circuit> build_half_adder();

/// Builds a full adder circuit.
/// Inputs: A (index 0), B (index 1), Cin (index 2)
/// Outputs: Sum (index 0), Cout (index 1)
[[nodiscard]] std::unique_ptr<Circuit> build_full_adder();

/// Builds a ripple-carry adder for N-bit inputs.
/// Inputs: A[0..N-1] (indices 0..N-1), B[0..N-1] (indices N..2N-1)
///         where index 0 = LSB
/// Outputs: Sum[0..N-1] (indices 0..N-1), Carry-out (index N)
///
/// For a 7-bit adder: 14 inputs, 8 outputs (7 sum bits + carry-out)
[[nodiscard]] std::unique_ptr<Circuit> build_ripple_carry_adder(int bits);

} // namespace gateflow
