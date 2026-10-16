#pragma once

// Play/Pause/Step/Stop/Reset state machine and the clock it drives (docs/todo/
// PLAN_blender_universe_authoring_loop.md stage 2). UI- and renderer-independent: Universe and
// PhantomStudio each install their own capture/restore/step hooks and share these rules, so a
// scenario run through either app steps and rewinds the same way.
//
// States
//   Edit     authored state; nothing is simulated; no snapshot exists.
//   Playing  advance() runs fixed steps; the Edit snapshot is held.
//   Paused   Play was entered but time does not advance; only step() moves it.
// Edit -> Playing/Paused takes the snapshot (capture hook); Stop restores it (restore hook) and
// returns to Edit. Reset restores, re-captures and stays Paused. The snapshot itself is owned by
// the hook owner -- this class only decides *when* to take and apply it.
//
// Time
//   advance(wallDt) is the interactive path: it feeds a fixed-step accumulator, so the number of
//   simulated steps depends only on elapsed wall time, never on frame rate jitter within a step.
//   step(dt) is the scenario/manual path: exactly one step of the given dt, no accumulator, so
//   results never depend on render waits or UI frame counts.
//   Both advance TimelineClock, which every time-driven component (physics, object/skeletal/morph
//   animation) reads instead of keeping its own clock.

#include <cstdint>
#include <functional>

namespace Phantom::SceneRuntime {

enum class PlayState { Edit, Playing, Paused };

const char* toString(PlayState state);

// Time since the current Play session began, in seconds. Reset on Stop/Reset. The per-domain
// times start equal and advance together; they exist as separate fields so a domain can later be
// offset or scaled (e.g. a cached playback) without changing the consumers' contract.
struct TimelineClock {
    double sceneTime    = 0.0;
    double physicsTime  = 0.0;
    double objectTime   = 0.0;
    double skeletalTime = 0.0;
    double morphTime    = 0.0;
    std::uint64_t stepCount = 0;

    void advance(double dt);
    void reset() { *this = TimelineClock{}; }
};

struct PlayHooks {
    std::function<void()>       capture; // entering Play from Edit: snapshot editable state
    std::function<void()>       restore; // Stop/Reset: put the snapshot back
    std::function<void(double)> step;    // advance the simulation by one fixed step (seconds)
};

class PlaySession {
public:
    static constexpr double kFixedDt = 1.0 / 60.0;
    // advance() never runs more than this many steps per call, so a stall cannot cause a spiral.
    static constexpr int kMaxStepsPerAdvance = 4;

    void setHooks(PlayHooks hooks) { hooks_ = std::move(hooks); }

    PlayState state() const { return state_; }
    bool isEdit() const { return state_ == PlayState::Edit; }
    bool isPlaying() const { return state_ == PlayState::Playing; }
    bool isPaused() const { return state_ == PlayState::Paused; }
    // True from the moment Play is entered until Stop (Playing or Paused).
    bool snapshotActive() const { return state_ != PlayState::Edit; }
    const TimelineClock& clock() const { return clock_; }

    // Edit: capture, then Playing. Paused: resume. Playing: no-op.
    void play();
    // Playing -> Paused. Edit/Paused: no-op (pausing never creates a snapshot).
    void pause();
    // Restore the snapshot, reset the clock, return to Edit. No-op in Edit.
    void stop();
    // Restore, re-capture, clock back to 0, stay Paused. No-op in Edit.
    void reset();
    // Runs exactly one step of dt. From Edit it captures first (so Stop can undo it) and leaves
    // the session Paused. Returns false (and does nothing) for dt <= 0.
    bool step(double dt);
    // Interactive tick. Does nothing unless Playing. Runs whole kFixedDt steps for the
    // accumulated time and returns how many were run.
    int advance(double wallDt);
    // Seconds a clip-style animation should advance this frame. Edit previews with wall time;
    // Playing follows the fixed steps advance() just ran (the same ones that move physics);
    // Paused freezes. stepsRun is advance()'s return value for this frame.
    double animationDt(double wallDt, int stepsRun) const;
    // The edited scene was cleared out from under the session: forget the snapshot without
    // restoring it.
    void discard();

private:
    void runStep(double dt);

    PlayHooks hooks_;
    PlayState state_ = PlayState::Edit;
    TimelineClock clock_;
    double accumulator_ = 0.0;
};

} // namespace Phantom::SceneRuntime
