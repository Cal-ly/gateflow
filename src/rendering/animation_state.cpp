/// @file animation_state.cpp
/// @brief Implements per-gate and per-wire animation transitions

#include "rendering/animation_state.hpp"

#include <algorithm>
#include <cmath>

namespace gateflow {

// Static defaults for lookup misses
const GateAnim AnimationState::DEFAULT_GATE_ANIM = {};
const WireAnim AnimationState::DEFAULT_WIRE_ANIM = {};

namespace {

constexpr float PULSE_SPEED = 4.0f;   ///< Radians per second for pending pulse
constexpr float FADE_IN_SPEED = 5.0f; ///< Alpha units per second for gate fade-in
constexpr float PI_2 = 6.2831853f;    ///< 2π

} // namespace

AnimationState::AnimationState(const Circuit* circuit) : circuit_(circuit) {
    // Pre-allocate entries for all gates and wires
    for (auto& gate_ptr : circuit_->gates()) {
        gate_anims_[gate_ptr.get()] = {};
    }
    for (auto& wire_ptr : circuit_->wires()) {
        wire_anims_[wire_ptr.get()] = {};
    }
}

void AnimationState::update(float delta_time, const PropagationScheduler& scheduler) {
    // Update gate animations
    for (auto& [gate, anim] : gate_anims_) {
        anim.resolved = scheduler.is_gate_resolved(gate);

        if (anim.resolved) {
            // Fade in toward full opacity
            anim.alpha = std::min(1.0f, anim.alpha + FADE_IN_SPEED * delta_time);
            anim.pulse_phase = 0.0f; // Stop pulsing once resolved
        } else {
            // Pending state: gentle pulse, low alpha
            anim.pulse_phase += PULSE_SPEED * delta_time;
            if (anim.pulse_phase > PI_2) {
                anim.pulse_phase -= PI_2;
            }
            // Base alpha for pending gates: 0.3 + 0.15 * sin(phase) → range [0.15, 0.45]
            anim.alpha = 0.3f + 0.15f * std::sin(anim.pulse_phase);
        }
    }

    // Update wire animations
    for (auto& [wire, anim] : wire_anims_) {
        anim.resolved = scheduler.is_wire_resolved(wire);
        anim.signal_progress = scheduler.wire_signal_progress(wire);
    }
}

void AnimationState::reset() {
    for (auto& [gate, anim] : gate_anims_) {
        anim = {};
    }
    for (auto& [wire, anim] : wire_anims_) {
        anim = {};
    }
}

const GateAnim& AnimationState::gate_anim(const Gate* gate) const {
    auto it = gate_anims_.find(gate);
    if (it != gate_anims_.end()) {
        return it->second;
    }
    return DEFAULT_GATE_ANIM;
}

const WireAnim& AnimationState::wire_anim(const Wire* wire) const {
    auto it = wire_anims_.find(wire);
    if (it != wire_anims_.end()) {
        return it->second;
    }
    return DEFAULT_WIRE_ANIM;
}

} // namespace gateflow
