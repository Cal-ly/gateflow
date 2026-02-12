/// @file propagation_scheduler.cpp
/// @brief Implements temporal propagation scheduling

#include "timing/propagation_scheduler.hpp"

#include <algorithm>

namespace gateflow {

PropagationScheduler::PropagationScheduler(const Circuit* circuit) : circuit_(circuit) {
    compute_depths();
}

void PropagationScheduler::compute_depths() {
    gate_depths_.clear();
    max_depth_ = 0;

    for (const Gate* gate : circuit_->topological_order()) {
        int max_input_depth = -1;
        for (const Wire* input_wire : gate->get_inputs()) {
            const Gate* src = input_wire->get_source();
            if (src != nullptr) {
                auto it = gate_depths_.find(src);
                if (it != gate_depths_.end()) {
                    max_input_depth = std::max(max_input_depth, it->second);
                }
            }
        }
        int depth = max_input_depth + 1;
        gate_depths_[gate] = depth;
        max_depth_ = std::max(max_depth_, depth);
    }
}

void PropagationScheduler::tick(float delta_time) {
    if (mode_ == PlaybackMode::PAUSED && !step_requested_) {
        return;
    }

    if (step_requested_) {
        // Advance to the next integer depth
        float target = static_cast<float>(static_cast<int>(current_depth_) + 1);
        if (current_depth_ < 0.0f) {
            target = 0.0f;
        }
        current_depth_ = std::min(target, static_cast<float>(max_depth_) + 1.0f);
        step_requested_ = false;
        return;
    }

    // REALTIME mode: advance continuously
    current_depth_ += speed_ * delta_time;

    // Clamp to slightly past max_depth so all gates are fully resolved
    if (current_depth_ > static_cast<float>(max_depth_) + 1.0f) {
        current_depth_ = static_cast<float>(max_depth_) + 1.0f;
    }
}

void PropagationScheduler::reset() {
    current_depth_ = -1.0f;
    if (mode_ == PlaybackMode::STEP) {
        mode_ = PlaybackMode::PAUSED;
    }
}

void PropagationScheduler::step() {
    step_requested_ = true;
    if (mode_ == PlaybackMode::REALTIME) {
        mode_ = PlaybackMode::PAUSED;
    }
}

void PropagationScheduler::toggle_pause() {
    if (mode_ == PlaybackMode::PAUSED) {
        mode_ = PlaybackMode::REALTIME;
    } else {
        mode_ = PlaybackMode::PAUSED;
    }
}

bool PropagationScheduler::is_gate_resolved(const Gate* gate) const {
    auto it = gate_depths_.find(gate);
    if (it == gate_depths_.end()) {
        return false;
    }
    return current_depth_ >= static_cast<float>(it->second);
}

bool PropagationScheduler::is_wire_resolved(const Wire* wire) const {
    const Gate* src = wire->get_source();
    if (src == nullptr) {
        // Primary input wires are always resolved (they're set before propagation)
        return current_depth_ >= 0.0f;
    }
    return is_gate_resolved(src);
}

float PropagationScheduler::gate_resolve_fraction(const Gate* gate) const {
    auto it = gate_depths_.find(gate);
    if (it == gate_depths_.end()) {
        return 0.0f;
    }
    float gate_d = static_cast<float>(it->second);
    if (current_depth_ < gate_d) {
        return 0.0f; // Not yet resolved
    }
    // Fraction: how far past the gate's depth we are (clamped to 1.0)
    // A gate takes 1.0 depth-units to fully "fade in"
    float fraction = current_depth_ - gate_d;
    return std::min(fraction, 1.0f);
}

float PropagationScheduler::wire_signal_progress(const Wire* wire) const {
    const Gate* src = wire->get_source();
    if (src == nullptr) {
        // Primary input wire: resolved as soon as propagation starts
        if (current_depth_ < 0.0f) {
            return 0.0f;
        }
        return std::min(current_depth_ + 1.0f, 1.0f);
    }

    auto it = gate_depths_.find(src);
    if (it == gate_depths_.end()) {
        return 0.0f;
    }
    float gate_d = static_cast<float>(it->second);
    if (current_depth_ < gate_d) {
        return 0.0f; // Source gate not yet resolved
    }
    // Signal travels along the wire during the depth between source and destination
    // Takes 1.0 depth-units to travel
    float progress = current_depth_ - gate_d;
    return std::min(progress, 1.0f);
}

int PropagationScheduler::gate_depth(const Gate* gate) const {
    auto it = gate_depths_.find(gate);
    if (it == gate_depths_.end()) {
        return -1;
    }
    return it->second;
}

bool PropagationScheduler::is_complete() const {
    return current_depth_ >= static_cast<float>(max_depth_) + 1.0f;
}

} // namespace gateflow
