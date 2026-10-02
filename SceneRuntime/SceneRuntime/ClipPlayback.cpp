#include "ClipPlayback.h"

#include <cmath>

namespace Phantom::SceneRuntime {

void advanceClip(ClipState& s, float dt, float duration)
{
    if (s.clip < 0 || !s.playing) return;
    s.time += dt * s.speed;
    if (duration > 0.f) {
        if (s.loop) {
            s.time = std::fmod(s.time, duration);
            if (s.time < 0.f) s.time += duration;
        } else if (s.time >= duration) {
            s.time = duration;
            s.playing = false;
        } else if (s.time <= 0.f) {
            s.time = 0.f;
            s.playing = false;
        }
    } else {
        s.time = 0.f;
        if (!s.loop) s.playing = false;
    }
}

} // namespace Phantom::SceneRuntime
