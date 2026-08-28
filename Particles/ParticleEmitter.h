#pragma once

#include "ParticleGpu.h"

#include <cstdint>

namespace Phantom::Particles {

// Owns emitter configuration and, every frame, decides which ring-buffer slots of the
// particle SSBO to (re)spawn. Emission uses a simple ring cursor rather than a free list:
// each frame it claims the next emitRate*dt particle slots starting at the cursor and
// forces them to respawn, overwriting whatever was there (dead or still alive). This keeps
// ParticleSimulator's compute shader branch-free and allocation-free, at the cost of older
// particles being recycled early once the emit rate exceeds what maxParticles can hold for
// a full lifeTime -- an accepted trade-off for a fixed-capacity GPU pool (see "GPU パーティクルの同期"
// in docs/todo/PLAN_universe_app.md).
class ParticleEmitter {
public:
    // Ring-buffer range of particle indices to force-spawn this frame (count may exceed
    // maxParticles - start range wraps modulo maxParticles).
    struct EmitBatch {
        uint32_t start = 0;
        uint32_t count = 0;
        uint32_t seed  = 0;
    };

    void setParams(const EmitterParams& p) { params_ = p; }
    EmitterParams&       params()       { return params_; }
    const EmitterParams& params() const { return params_; }

    void setEnabled(bool v) { enabled_ = v; }
    bool isEnabled() const  { return enabled_; }

    // Advances the emission accumulator by dt and returns the slot range to spawn this
    // frame. Returns count == 0 when disabled or when emitRate*dt hasn't accumulated a
    // whole particle yet.
    EmitBatch update(float dt, uint32_t maxParticles);

    void reset();

private:
    EmitterParams params_;
    bool     enabled_  = true;
    float    accum_    = 0.f;
    uint32_t cursor_   = 0;
    uint32_t frameSeed_ = 1;
};

} // namespace Phantom::Particles
