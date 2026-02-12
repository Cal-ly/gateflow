/// @file test_nand_decompose.cpp
/// @brief Tests that NAND decomposition produces identical results for all gate types

#include <catch2/catch_test_macros.hpp>

#include "simulation/circuit.hpp"
#include "simulation/circuit_builder.hpp"
#include "simulation/nand_decompose.hpp"

#include <cstdlib>
#include <vector>

using namespace gateflow;

namespace {

/// Helper: build a simple 2-input single-gate circuit
std::unique_ptr<Circuit> build_single_gate(GateType type) {
    auto circuit = std::make_unique<Circuit>();

    Wire* a = circuit->add_wire();
    Wire* b = circuit->add_wire();
    circuit->mark_input(a);
    circuit->mark_input(b);

    Gate* gate = circuit->add_gate(type);
    Wire* out = circuit->add_wire();
    circuit->mark_output(out);

    circuit->connect(a, nullptr, gate);
    circuit->connect(b, nullptr, gate);
    circuit->connect(out, gate, nullptr);

    circuit->finalize();
    return circuit;
}

/// Helper: build a 1-input single-gate circuit (NOT, BUFFER)
std::unique_ptr<Circuit> build_unary_gate(GateType type) {
    auto circuit = std::make_unique<Circuit>();

    Wire* a = circuit->add_wire();
    circuit->mark_input(a);

    Gate* gate = circuit->add_gate(type);
    Wire* out = circuit->add_wire();
    circuit->mark_output(out);

    circuit->connect(a, nullptr, gate);
    circuit->connect(out, gate, nullptr);

    circuit->finalize();
    return circuit;
}

/// Verify all gates are NAND after decomposition
void verify_all_nand(const Circuit& circuit) {
    for (auto& gate : circuit.gates()) {
        CHECK(gate->get_type() == GateType::NAND);
    }
}

} // namespace

// ---------- Individual gate type decomposition ----------

TEST_CASE("NAND decomposition — NOT gate", "[nand]") {
    auto circuit = build_unary_gate(GateType::NOT);

    // Record original outputs
    std::vector<bool> original;
    for (bool input : {false, true}) {
        circuit->set_input(0, input);
        (void)circuit->propagate();
        original.push_back(circuit->get_output(0));
    }

    // Decompose
    decompose_to_nand(*circuit);
    verify_all_nand(*circuit);

    // Verify identical outputs
    int idx = 0;
    for (bool input : {false, true}) {
        circuit->set_input(0, input);
        (void)circuit->propagate();
        INFO("NOT input=" << input);
        CHECK(circuit->get_output(0) == original[idx++]);
    }
}

TEST_CASE("NAND decomposition — BUFFER gate", "[nand]") {
    auto circuit = build_unary_gate(GateType::BUFFER);

    std::vector<bool> original;
    for (bool input : {false, true}) {
        circuit->set_input(0, input);
        (void)circuit->propagate();
        original.push_back(circuit->get_output(0));
    }

    decompose_to_nand(*circuit);
    verify_all_nand(*circuit);

    int idx = 0;
    for (bool input : {false, true}) {
        circuit->set_input(0, input);
        (void)circuit->propagate();
        INFO("BUFFER input=" << input);
        CHECK(circuit->get_output(0) == original[idx++]);
    }
}

TEST_CASE("NAND decomposition — AND gate", "[nand]") {
    auto circuit = build_single_gate(GateType::AND);

    std::vector<bool> original;
    for (int a = 0; a <= 1; a++) {
        for (int b = 0; b <= 1; b++) {
            circuit->set_input(0, a);
            circuit->set_input(1, b);
            (void)circuit->propagate();
            original.push_back(circuit->get_output(0));
        }
    }

    decompose_to_nand(*circuit);
    verify_all_nand(*circuit);

    int idx = 0;
    for (int a = 0; a <= 1; a++) {
        for (int b = 0; b <= 1; b++) {
            circuit->set_input(0, a);
            circuit->set_input(1, b);
            (void)circuit->propagate();
            INFO("AND a=" << a << " b=" << b);
            CHECK(circuit->get_output(0) == original[idx++]);
        }
    }
}

TEST_CASE("NAND decomposition — OR gate", "[nand]") {
    auto circuit = build_single_gate(GateType::OR);

    std::vector<bool> original;
    for (int a = 0; a <= 1; a++) {
        for (int b = 0; b <= 1; b++) {
            circuit->set_input(0, a);
            circuit->set_input(1, b);
            (void)circuit->propagate();
            original.push_back(circuit->get_output(0));
        }
    }

    decompose_to_nand(*circuit);
    verify_all_nand(*circuit);

    int idx = 0;
    for (int a = 0; a <= 1; a++) {
        for (int b = 0; b <= 1; b++) {
            circuit->set_input(0, a);
            circuit->set_input(1, b);
            (void)circuit->propagate();
            INFO("OR a=" << a << " b=" << b);
            CHECK(circuit->get_output(0) == original[idx++]);
        }
    }
}

TEST_CASE("NAND decomposition — XOR gate", "[nand]") {
    auto circuit = build_single_gate(GateType::XOR);

    std::vector<bool> original;
    for (int a = 0; a <= 1; a++) {
        for (int b = 0; b <= 1; b++) {
            circuit->set_input(0, a);
            circuit->set_input(1, b);
            (void)circuit->propagate();
            original.push_back(circuit->get_output(0));
        }
    }

    decompose_to_nand(*circuit);
    verify_all_nand(*circuit);

    int idx = 0;
    for (int a = 0; a <= 1; a++) {
        for (int b = 0; b <= 1; b++) {
            circuit->set_input(0, a);
            circuit->set_input(1, b);
            (void)circuit->propagate();
            INFO("XOR a=" << a << " b=" << b);
            CHECK(circuit->get_output(0) == original[idx++]);
        }
    }
}

TEST_CASE("NAND decomposition — NAND gate stays unchanged", "[nand]") {
    auto circuit = build_single_gate(GateType::NAND);
    size_t gate_count_before = circuit->gates().size();

    decompose_to_nand(*circuit);

    // No new gates should be added
    CHECK(circuit->gates().size() == gate_count_before);
    verify_all_nand(*circuit);
}

// ---------- Full circuit decomposition ----------

TEST_CASE("NAND decomposition — half adder produces identical results", "[nand]") {
    auto circuit = build_half_adder();

    // Capture original results
    struct Row {
        bool a, b, sum, carry;
    };
    std::vector<Row> original;
    for (int a = 0; a <= 1; a++) {
        for (int b = 0; b <= 1; b++) {
            circuit->set_input(0, a);
            circuit->set_input(1, b);
            (void)circuit->propagate();
            original.push_back({static_cast<bool>(a), static_cast<bool>(b), circuit->get_output(0),
                                circuit->get_output(1)});
        }
    }

    decompose_to_nand(*circuit);
    verify_all_nand(*circuit);

    for (auto& [a, b, expected_sum, expected_carry] : original) {
        circuit->set_input(0, a);
        circuit->set_input(1, b);
        (void)circuit->propagate();
        INFO("Half adder NAND: A=" << a << " B=" << b);
        CHECK(circuit->get_output(0) == expected_sum);
        CHECK(circuit->get_output(1) == expected_carry);
    }
}

TEST_CASE("NAND decomposition — full adder produces identical results", "[nand]") {
    auto circuit = build_full_adder();

    struct Row {
        bool a, b, cin, sum, cout;
    };
    std::vector<Row> original;
    for (int a = 0; a <= 1; a++) {
        for (int b = 0; b <= 1; b++) {
            for (int cin = 0; cin <= 1; cin++) {
                circuit->set_input(0, a);
                circuit->set_input(1, b);
                circuit->set_input(2, cin);
                (void)circuit->propagate();
                original.push_back({static_cast<bool>(a), static_cast<bool>(b),
                                    static_cast<bool>(cin), circuit->get_output(0),
                                    circuit->get_output(1)});
            }
        }
    }

    decompose_to_nand(*circuit);
    verify_all_nand(*circuit);

    for (auto& [a, b, cin, expected_sum, expected_cout] : original) {
        circuit->set_input(0, a);
        circuit->set_input(1, b);
        circuit->set_input(2, cin);
        (void)circuit->propagate();
        INFO("Full adder NAND: A=" << a << " B=" << b << " Cin=" << cin);
        CHECK(circuit->get_output(0) == expected_sum);
        CHECK(circuit->get_output(1) == expected_cout);
    }
}

TEST_CASE("NAND decomposition — 7-bit ripple-carry adder spot checks", "[nand]") {
    auto circuit = build_ripple_carry_adder(7);

    // Helper to set/read adder values
    auto set_inputs = [&](int a, int b) {
        for (int i = 0; i < 7; i++) {
            circuit->set_input(i, (a >> i) & 1);
            circuit->set_input(7 + i, (b >> i) & 1);
        }
    };
    auto read_output = [&]() {
        int result = 0;
        for (int i = 0; i < 8; i++) {
            if (circuit->get_output(i)) {
                result |= (1 << i);
            }
        }
        return result;
    };

    // Capture some original results
    struct TC {
        int a, b, result;
    };
    std::vector<TC> cases = {{0, 0, 0}, {50, 49, 0}, {99, 99, 0}, {1, 1, 0}, {42, 37, 0}};
    for (auto& tc : cases) {
        set_inputs(tc.a, tc.b);
        (void)circuit->propagate();
        tc.result = read_output();
    }

    // Decompose and verify
    decompose_to_nand(*circuit);
    verify_all_nand(*circuit);

    for (auto& tc : cases) {
        set_inputs(tc.a, tc.b);
        (void)circuit->propagate();
        INFO("RCA NAND: " << tc.a << " + " << tc.b);
        CHECK(read_output() == tc.result);
    }
}
