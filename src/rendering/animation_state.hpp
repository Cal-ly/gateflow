/// @file animation_state.hpp
/// @brief Per-gate and per-wire animation state for smooth visual transitions.
///
/// Animation state is completely separate from simulation state (per the spec:
/// "Don't store animation state inside Gate/Wire"). The simulation layer has
/// already computed the correct final state; the animation system controls
/// which parts of that state are currently *visible* and how they look.

#pragma once

#include "simulation/circuit.hpp"
#include "timing/propagation_scheduler.hpp"

#include <unordered_map>

namespace gateflow {

/// Per-gate animation parameters
struct GateAnim {
    float alpha = 0.0f;       ///< Overall opacity (0=invisible, 1=fully visible)
    float pulse_phase = 0.0f; ///< Phase for pending-state pulse (0–2π)
    bool resolved = false;    ///< Whether the gate's output is currently visible
};

/// Per-wire animation parameters
struct WireAnim {
    float signal_progress = 0.0f; ///< 0.0–1.0, how far signal has traveled
    bool resolved = false;        ///< Whether the wire value is fully visible
};

/// Manages all animation state for a circuit visualization.
/// Updated each frame from the propagation scheduler.
class AnimationState {
  public:
    /// Initialize animation state for all gates and wires in the circuit
    explicit AnimationState(const Circuit* circuit);

    /// Update all animations based on the scheduler's current depth.
    /// @param delta_time Seconds since last frame
    /// @param scheduler The propagation scheduler driving the animation
    void update(float delta_time, const PropagationScheduler& scheduler);

    /// Reset all animations to initial state (nothing resolved)
    void reset();

    /// Get animation state for a specific gate
    [[nodiscard]] const GateAnim& gate_anim(const Gate* gate) const;

    /// Get animation state for a specific wire
    [[nodiscard]] const WireAnim& wire_anim(const Wire* wire) const;

  private:
    const Circuit* circuit_;
    std::unordered_map<const Gate*, GateAnim> gate_anims_;
    std::unordered_map<const Wire*, WireAnim> wire_anims_;

    // Default return values for missing keys
    static const GateAnim DEFAULT_GATE_ANIM;
    static const WireAnim DEFAULT_WIRE_ANIM;
};

} // namespace gateflow
