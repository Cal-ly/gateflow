/// @file test_gate.cpp
/// @brief Tests for gate evaluation — all input combinations for every gate type

#include <catch2/catch_test_macros.hpp>

#include "simulation/gate.hpp"

using namespace gateflow;

// ---------- NOT gate ----------

TEST_CASE("NOT gate evaluation", "[gate]") {
    CHECK(evaluate(GateType::NOT, {false}) == true);
    CHECK(evaluate(GateType::NOT, {true}) == false);
}

// ---------- BUFFER gate ----------

TEST_CASE("BUFFER gate evaluation", "[gate]") {
    CHECK(evaluate(GateType::BUFFER, {false}) == false);
    CHECK(evaluate(GateType::BUFFER, {true}) == true);
}

// ---------- AND gate ----------

TEST_CASE("AND gate evaluation", "[gate]") {
    CHECK(evaluate(GateType::AND, {false, false}) == false);
    CHECK(evaluate(GateType::AND, {false, true}) == false);
    CHECK(evaluate(GateType::AND, {true, false}) == false);
    CHECK(evaluate(GateType::AND, {true, true}) == true);
}

// ---------- NAND gate ----------

TEST_CASE("NAND gate evaluation", "[gate]") {
    CHECK(evaluate(GateType::NAND, {false, false}) == true);
    CHECK(evaluate(GateType::NAND, {false, true}) == true);
    CHECK(evaluate(GateType::NAND, {true, false}) == true);
    CHECK(evaluate(GateType::NAND, {true, true}) == false);
}

// ---------- OR gate ----------

TEST_CASE("OR gate evaluation", "[gate]") {
    CHECK(evaluate(GateType::OR, {false, false}) == false);
    CHECK(evaluate(GateType::OR, {false, true}) == true);
    CHECK(evaluate(GateType::OR, {true, false}) == true);
    CHECK(evaluate(GateType::OR, {true, true}) == true);
}

// ---------- XOR gate ----------

TEST_CASE("XOR gate evaluation", "[gate]") {
    CHECK(evaluate(GateType::XOR, {false, false}) == false);
    CHECK(evaluate(GateType::XOR, {false, true}) == true);
    CHECK(evaluate(GateType::XOR, {true, false}) == true);
    CHECK(evaluate(GateType::XOR, {true, true}) == false);
}

// ---------- Multi-input gates ----------

TEST_CASE("AND gate with 3 inputs", "[gate]") {
    CHECK(evaluate(GateType::AND, {true, true, true}) == true);
    CHECK(evaluate(GateType::AND, {true, true, false}) == false);
    CHECK(evaluate(GateType::AND, {false, true, true}) == false);
}

TEST_CASE("OR gate with 3 inputs", "[gate]") {
    CHECK(evaluate(GateType::OR, {false, false, false}) == false);
    CHECK(evaluate(GateType::OR, {false, false, true}) == true);
    CHECK(evaluate(GateType::OR, {true, false, false}) == true);
}

TEST_CASE("XOR gate with 3 inputs (parity)", "[gate]") {
    CHECK(evaluate(GateType::XOR, {false, false, false}) == false);
    CHECK(evaluate(GateType::XOR, {true, false, false}) == true);
    CHECK(evaluate(GateType::XOR, {true, true, false}) == false);
    CHECK(evaluate(GateType::XOR, {true, true, true}) == true);
}

// ---------- Invalid input counts ----------

TEST_CASE("Gate evaluation rejects invalid input counts", "[gate]") {
    CHECK_THROWS_AS(evaluate(GateType::NOT, {true, false}), std::invalid_argument);
    CHECK_THROWS_AS(evaluate(GateType::BUFFER, {}), std::invalid_argument);
    CHECK_THROWS_AS(evaluate(GateType::AND, {true}), std::invalid_argument);
    CHECK_THROWS_AS(evaluate(GateType::NAND, {false}), std::invalid_argument);
    CHECK_THROWS_AS(evaluate(GateType::OR, {true}), std::invalid_argument);
    CHECK_THROWS_AS(evaluate(GateType::XOR, {false}), std::invalid_argument);
}

// ---------- Gate type name ----------

TEST_CASE("Gate type names", "[gate]") {
    CHECK(gate_type_name(GateType::NAND) == "NAND");
    CHECK(gate_type_name(GateType::AND) == "AND");
    CHECK(gate_type_name(GateType::OR) == "OR");
    CHECK(gate_type_name(GateType::XOR) == "XOR");
    CHECK(gate_type_name(GateType::NOT) == "NOT");
    CHECK(gate_type_name(GateType::BUFFER) == "BUFFER");
}
