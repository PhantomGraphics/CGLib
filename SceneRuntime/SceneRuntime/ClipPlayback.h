#pragma once

// glTF clip playback state and its time advance rule, shared by Universe (GltfRenderer::tick) and
// PhantomStudio (ClipSimWorld) so a clip reaches the same time after the same steps in both.
// Mirrors the "animationPlayer" component block of a .universe scene.

namespace Phantom::SceneRuntime {

struct ClipState {
    int   clip = -1;       // < 0: no clip
    float time = 0.f;
    float speed = 1.f;     // negative plays backward
    bool  loop = true;
    bool  playing = false;
};

// Advances `s` by `dt` seconds of a clip lasting `duration`. Looping clips wrap; a non-looping
// clip stops (playing = false) when it reaches either end. Does nothing for a stopped state.
void advanceClip(ClipState& s, float dt, float duration);

} // namespace Phantom::SceneRuntime
