#pragma once

/// @file nand_decompose.hpp
/// @brief Decomposes all gates in a circuit into NAND-only equivalents

#include "simulation/circuit.hpp"

namespace gateflow {

/// Replaces every non-NAND gate in the circuit with its NAND-equivalent
/// sub-circuit, preserving external connectivity.
///
/// Decomposition rules:
///   NOT(A)    = NAND(A, A)
///   AND(A,B)  = NAND(NAND(A,B), NAND(A,B))
///   OR(A,B)   = NAND(NAND(A,A), NAND(B,B))
///   XOR(A,B)  = NAND(NAND(A, NAND(A,B)), NAND(B, NAND(A,B)))
///   BUFFER(A) = NAND(NAND(A,A), NAND(A,A))
///
/// The circuit must be finalized before calling this function.
/// After decomposition, the circuit is re-finalized.
void decompose_to_nand(Circuit& circuit);

} // namespace gateflow
