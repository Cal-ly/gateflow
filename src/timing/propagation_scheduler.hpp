/// @file propagation_scheduler.hpp
/// @brief Manages temporal propagation of signals through a circuit.
///
/// The scheduler computes the topological depth of each gate and advances
/// a "current depth" each frame. Gates at depth <= current_depth are
/// "resolved" (their true output is visible). This creates the visual
/// effect of signals flowing through the circuit over time.

#pragma once

#include "simulation/circuit.hpp"

#include <unordered_map>

namespace gateflow {

/// Propagation playback mode
enum class PlaybackMode { REALTIME, PAUSED, STEP };

/// Controls the temporal unfolding of a fully-propagated circuit.
///
/// Usage:
///   1. Construct with a finalized, propagated circuit
///   2. Each frame, call tick(delta_time)
///   3. Query is_gate_resolved() / is_wire_resolved() to determine visibility
///   4. Use gate_resolve_fraction() for smooth fade-in transitions
class PropagationScheduler {
  public:
    /// Constructs the scheduler and computes gate depths from the circuit.
    /// The circuit must already be finalized (topological order available).
    /// @param circuit Non-owning pointer to the circuit
    explicit PropagationScheduler(const Circuit* circuit);

    /// Advances the propagation by one frame.
    /// @param delta_time Seconds since last frame
    void tick(float delta_time);

    /// Resets propagation to the beginning (depth = -1, nothing resolved)
    void reset();

    /// Advances exactly one depth level (for step mode)
    void step();

    // --- Mode control ---
    void set_mode(PlaybackMode mode) { mode_ = mode; }
    [[nodiscard]] PlaybackMode mode() const { return mode_; }

    /// Toggle between REALTIME and PAUSED
    void toggle_pause();

    // --- Speed control ---
    void set_speed(float depths_per_second) { speed_ = depths_per_second; }
    [[nodiscard]] float speed() const { return speed_; }

    // --- Query ---

    /// Whether a gate's output is resolved (signal has reached it)
    [[nodiscard]] bool is_gate_resolved(const Gate* gate) const;

    /// Whether a wire's signal has fully arrived at its destination.
    /// A wire is resolved when its source gate is resolved.
    [[nodiscard]] bool is_wire_resolved(const Wire* wire) const;

    /// Returns a 0.0–1.0 fraction for smooth gate fade-in.
    /// 0.0 = just became resolved this moment, 1.0 = fully resolved.
    /// Returns 0.0 for unresolved gates, 1.0 for long-resolved gates.
    [[nodiscard]] float gate_resolve_fraction(const Gate* gate) const;

    /// Returns a 0.0–1.0 fraction for wire signal travel animation.
    /// 0.0 = signal just started traveling, 1.0 = fully arrived.
    /// Returns 0.0 for unresolved wires, 1.0 for long-resolved wires.
    [[nodiscard]] float wire_signal_progress(const Wire* wire) const;

    /// The topological depth of a gate (0 = directly connected to inputs)
    [[nodiscard]] int gate_depth(const Gate* gate) const;

    /// Current propagation depth (fractional for smooth animation)
    [[nodiscard]] float current_depth() const { return current_depth_; }

    /// Maximum depth in the circuit
    [[nodiscard]] int max_depth() const { return max_depth_; }

    /// Whether propagation has reached all gates
    [[nodiscard]] bool is_complete() const;

  private:
    /// Computes the longest-path depth from any input for each gate
    void compute_depths();

    const Circuit* circuit_;
    std::unordered_map<const Gate*, int> gate_depths_;
    int max_depth_ = 0;
    float current_depth_ = -1.0f; // Start before depth 0 so nothing is resolved
    float speed_ = 3.0f;          // Depths per second (user-adjustable)
    PlaybackMode mode_ = PlaybackMode::REALTIME;
    bool step_requested_ = false;
};

} // namespace gateflow
