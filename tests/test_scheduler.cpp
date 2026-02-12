/// @file test_scheduler.cpp
/// @brief Tests for propagation scheduler depth and progression behavior

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include "simulation/circuit.hpp"
#include "timing/propagation_scheduler.hpp"

using namespace gateflow;
using Catch::Approx;

namespace {

Circuit build_not_chain_3() {
    Circuit circuit;

    Wire* in = circuit.add_wire();
    circuit.mark_input(in);

    Gate* not1 = circuit.add_gate(GateType::NOT);
    Gate* not2 = circuit.add_gate(GateType::NOT);
    Gate* not3 = circuit.add_gate(GateType::NOT);

    Wire* w1 = circuit.add_wire();
    Wire* w2 = circuit.add_wire();
    Wire* out = circuit.add_wire();
    circuit.mark_output(out);

    circuit.connect(in, nullptr, not1);
    circuit.connect(w1, not1, not2);
    circuit.connect(w2, not2, not3);
    circuit.connect(out, not3, nullptr);

    circuit.finalize();
    return circuit;
}

Circuit build_single_not() {
    Circuit circuit;

    Wire* in = circuit.add_wire();
    circuit.mark_input(in);

    Gate* not_gate = circuit.add_gate(GateType::NOT);
    Wire* out = circuit.add_wire();
    circuit.mark_output(out);

    circuit.connect(in, nullptr, not_gate);
    circuit.connect(out, not_gate, nullptr);

    circuit.finalize();
    return circuit;
}

} // namespace

TEST_CASE("PropagationScheduler computes gate depths for a chain", "[scheduler]") {
    Circuit circuit = build_not_chain_3();
    circuit.set_input(0, true);
    (void)circuit.propagate();

    PropagationScheduler scheduler(&circuit);

    auto& order = circuit.topological_order();
    REQUIRE(order.size() == 3);

    CHECK(scheduler.gate_depth(order[0]) == 0);
    CHECK(scheduler.gate_depth(order[1]) == 1);
    CHECK(scheduler.gate_depth(order[2]) == 2);
    CHECK(scheduler.max_depth() == 2);
}

TEST_CASE("PropagationScheduler step advances exactly one depth", "[scheduler]") {
    Circuit circuit = build_not_chain_3();
    circuit.set_input(0, false);
    (void)circuit.propagate();

    PropagationScheduler scheduler(&circuit);

    CHECK(scheduler.current_depth() == Approx(-1.0f));

    scheduler.step();
    scheduler.tick(0.0f);
    CHECK(scheduler.current_depth() == Approx(0.0f));

    scheduler.tick(1.0f); // no-op while paused unless another step requested
    CHECK(scheduler.current_depth() == Approx(0.0f));

    scheduler.step();
    scheduler.tick(0.0f);
    CHECK(scheduler.current_depth() == Approx(1.0f));

    scheduler.step();
    scheduler.tick(0.0f);
    CHECK(scheduler.current_depth() == Approx(2.0f));
}

TEST_CASE("PropagationScheduler wire and gate resolution boundaries", "[scheduler]") {
    Circuit circuit = build_single_not();
    circuit.set_input(0, true);
    (void)circuit.propagate();

    const Wire* in = circuit.input_wires()[0];
    const Wire* out = circuit.output_wires()[0];
    const Gate* gate = circuit.topological_order()[0];

    PropagationScheduler scheduler(&circuit);
    scheduler.set_mode(PlaybackMode::REALTIME);
    scheduler.set_speed(1.0f);

    CHECK_FALSE(scheduler.is_gate_resolved(gate));
    CHECK_FALSE(scheduler.is_wire_resolved(out));
    CHECK(scheduler.wire_signal_progress(in) == Approx(0.0f));
    CHECK(scheduler.wire_signal_progress(out) == Approx(0.0f));

    scheduler.tick(1.0f); // -1 -> 0

    CHECK(scheduler.is_gate_resolved(gate));
    CHECK(scheduler.is_wire_resolved(out));
    CHECK(scheduler.gate_resolve_fraction(gate) == Approx(0.0f));
    CHECK(scheduler.wire_signal_progress(in) == Approx(1.0f));
    CHECK(scheduler.wire_signal_progress(out) == Approx(0.0f));

    scheduler.tick(0.5f); // 0 -> 0.5
    CHECK(scheduler.wire_signal_progress(out) == Approx(0.5f));

    scheduler.tick(0.5f); // 0.5 -> 1.0
    CHECK(scheduler.gate_resolve_fraction(gate) == Approx(1.0f));
    CHECK(scheduler.wire_signal_progress(out) == Approx(1.0f));
    CHECK(scheduler.is_complete());
}
