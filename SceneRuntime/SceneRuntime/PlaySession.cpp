#include "PlaySession.h"

namespace Phantom::SceneRuntime {

const char* toString(PlayState state)
{
    switch (state) {
    case PlayState::Edit:    return "Edit";
    case PlayState::Playing: return "Playing";
    case PlayState::Paused:  return "Paused";
    }
    return "Edit";
}

void TimelineClock::advance(double dt)
{
    sceneTime    += dt;
    physicsTime  += dt;
    objectTime   += dt;
    skeletalTime += dt;
    morphTime    += dt;
    ++stepCount;
}

void PlaySession::runStep(double dt)
{
    if (hooks_.step) hooks_.step(dt);
    clock_.advance(dt);
}

void PlaySession::play()
{
    if (state_ == PlayState::Edit) {
        if (hooks_.capture) hooks_.capture();
        clock_.reset();
        accumulator_ = 0.0;
    }
    state_ = PlayState::Playing;
}

void PlaySession::pause()
{
    if (state_ == PlayState::Playing) state_ = PlayState::Paused;
}

void PlaySession::stop()
{
    if (state_ == PlayState::Edit) return;
    if (hooks_.restore) hooks_.restore();
    clock_.reset();
    accumulator_ = 0.0;
    state_ = PlayState::Edit;
}

void PlaySession::reset()
{
    if (state_ == PlayState::Edit) return;
    if (hooks_.restore) hooks_.restore();
    if (hooks_.capture) hooks_.capture();
    clock_.reset();
    accumulator_ = 0.0;
    state_ = PlayState::Paused;
}

bool PlaySession::step(double dt)
{
    if (!(dt > 0.0)) return false;
    if (state_ == PlayState::Edit) {
        if (hooks_.capture) hooks_.capture();
        clock_.reset();
        accumulator_ = 0.0;
    }
    state_ = PlayState::Paused;
    runStep(dt);
    return true;
}

int PlaySession::advance(double wallDt)
{
    if (state_ != PlayState::Playing || !(wallDt > 0.0)) return 0;
    accumulator_ += wallDt;
    int steps = 0;
    // The epsilon keeps 1/30 s == two 1/60 s steps exactly despite floating-point residue.
    constexpr double kEps = 1e-9;
    while (accumulator_ + kEps >= kFixedDt && steps < kMaxStepsPerAdvance) {
        accumulator_ -= kFixedDt;
        runStep(kFixedDt);
        ++steps;
    }
    // Drop time we refused to simulate (stall) rather than carrying a backlog forward.
    if (steps == kMaxStepsPerAdvance && accumulator_ + kEps >= kFixedDt) accumulator_ = 0.0;
    if (accumulator_ < 0.0) accumulator_ = 0.0;
    return steps;
}

void PlaySession::discard()
{
    clock_.reset();
    accumulator_ = 0.0;
    state_ = PlayState::Edit;
}

} // namespace Phantom::SceneRuntime
