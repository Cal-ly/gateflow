/// @file test_circuit_builder.cpp
/// @brief Tests for half adder, full adder, and ripple-carry adder builders

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include "simulation/circuit_builder.hpp"

#include <cstdlib>
#include <ctime>
#include <vector>

using namespace gateflow;

// ---------- Half adder ----------

TEST_CASE("Half adder — exhaustive truth table", "[builder]") {
    auto circuit = build_half_adder();
    REQUIRE(circuit->num_inputs() == 2);
    REQUIRE(circuit->num_outputs() == 2);

    // Truth table: A, B -> Sum, Carry
    struct Row {
        bool a, b, sum, carry;
    };
    std::vector<Row> truth_table = {
        {false, false, false, false},
        {false, true, true, false},
        {true, false, true, false},
        {true, true, false, true},
    };

    for (auto& [a, b, expected_sum, expected_carry] : truth_table) {
        circuit->set_input(0, a);
        circuit->set_input(1, b);
        (void)circuit->propagate();

        INFO("A=" << a << " B=" << b);
        CHECK(circuit->get_output(0) == expected_sum);
        CHECK(circuit->get_output(1) == expected_carry);
    }
}

// ---------- Full adder ----------

TEST_CASE("Full adder — exhaustive truth table", "[builder]") {
    auto circuit = build_full_adder();
    REQUIRE(circuit->num_inputs() == 3);
    REQUIRE(circuit->num_outputs() == 2);

    // Truth table: A, B, Cin -> Sum, Cout
    struct Row {
        bool a, b, cin, sum, cout;
    };
    std::vector<Row> truth_table = {
        {false, false, false, false, false}, {false, false, true, true, false},
        {false, true, false, true, false},   {false, true, true, false, true},
        {true, false, false, true, false},   {true, false, true, false, true},
        {true, true, false, false, true},    {true, true, true, true, true},
    };

    for (auto& [a, b, cin, expected_sum, expected_cout] : truth_table) {
        circuit->set_input(0, a);
        circuit->set_input(1, b);
        circuit->set_input(2, cin);
        (void)circuit->propagate();

        INFO("A=" << a << " B=" << b << " Cin=" << cin);
        CHECK(circuit->get_output(0) == expected_sum);
        CHECK(circuit->get_output(1) == expected_cout);
    }
}

// ---------- Ripple-carry adder helpers ----------

namespace {

/// Sets the inputs of a ripple-carry adder from two integers.
/// Input layout: A[0..bits-1] at indices 0..bits-1, B[0..bits-1] at indices bits..2*bits-1
void set_adder_inputs(Circuit& circuit, int bits, int a, int b) {
    for (int i = 0; i < bits; i++) {
        circuit.set_input(i, (a >> i) & 1);
    }
    for (int i = 0; i < bits; i++) {
        circuit.set_input(bits + i, (b >> i) & 1);
    }
}

/// Reads the output of a ripple-carry adder as an integer.
/// Output layout: Sum[0..bits-1], Carry-out at index bits
int read_adder_output(Circuit& circuit, int bits) {
    int result = 0;
    for (int i = 0; i <= bits; i++) {
        if (circuit.get_output(i)) {
            result |= (1 << i);
        }
    }
    return result;
}

} // namespace

// ---------- Ripple-carry adder — specified test cases ----------

TEST_CASE("Ripple-carry adder (7-bit) — boundary values", "[builder]") {
    auto circuit = build_ripple_carry_adder(7);
    REQUIRE(circuit->num_inputs() == 14);
    REQUIRE(circuit->num_outputs() == 8); // 7 sum bits + carry-out

    struct TestCase {
        int a, b, expected;
    };

    // Required test cases from spec
    std::vector<TestCase> cases = {
        {0, 0, 0},    {1, 0, 1},   {0, 1, 1},   {1, 1, 2},
        {50, 49, 99}, {99, 0, 99}, {0, 99, 99}, {99, 99, 198},
    };

    for (auto& [a, b, expected] : cases) {
        set_adder_inputs(*circuit, 7, a, b);
        (void)circuit->propagate();

        INFO("A=" << a << " B=" << b << " expected=" << expected);
        CHECK(read_adder_output(*circuit, 7) == expected);
    }
}

TEST_CASE("Ripple-carry adder (7-bit) — random pairs", "[builder]") {
    auto circuit = build_ripple_carry_adder(7);

    // Use a fixed seed for reproducibility
    std::srand(42);

    for (int i = 0; i < 20; i++) {
        int a = std::rand() % 100; // 0–99
        int b = std::rand() % 100;
        int expected = a + b;

        set_adder_inputs(*circuit, 7, a, b);
        (void)circuit->propagate();

        INFO("Random test " << i << ": A=" << a << " B=" << b << " expected=" << expected);
        CHECK(read_adder_output(*circuit, 7) == expected);
    }
}

// ---------- Small adder edge cases ----------

TEST_CASE("1-bit ripple-carry adder", "[builder]") {
    auto circuit = build_ripple_carry_adder(1);
    REQUIRE(circuit->num_inputs() == 2);
    REQUIRE(circuit->num_outputs() == 2);

    // 0 + 0 = 0
    set_adder_inputs(*circuit, 1, 0, 0);
    (void)circuit->propagate();
    CHECK(read_adder_output(*circuit, 1) == 0);

    // 1 + 1 = 2
    set_adder_inputs(*circuit, 1, 1, 1);
    (void)circuit->propagate();
    CHECK(read_adder_output(*circuit, 1) == 2);
}

TEST_CASE("4-bit ripple-carry adder", "[builder]") {
    auto circuit = build_ripple_carry_adder(4);

    // 15 + 15 = 30
    set_adder_inputs(*circuit, 4, 15, 15);
    (void)circuit->propagate();
    CHECK(read_adder_output(*circuit, 4) == 30);

    // 7 + 8 = 15
    set_adder_inputs(*circuit, 4, 7, 8);
    (void)circuit->propagate();
    CHECK(read_adder_output(*circuit, 4) == 15);
}
