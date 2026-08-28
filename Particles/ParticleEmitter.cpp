#include "ParticleEmitter.h"

#include <algorithm>

namespace Phantom::Particles {

ParticleEmitter::EmitBatch ParticleEmitter::update(float dt, uint32_t maxParticles)
{
    EmitBatch batch{};
    if (!enabled_ || maxParticles == 0)
        return batch;

    accum_ += params_.emitRate * dt;
    uint32_t count = static_cast<uint32_t>(accum_);
    count = std::min(count, maxParticles);
    accum_ -= static_cast<float>(count);

    batch.start = cursor_;
    batch.count = count;
    batch.seed  = frameSeed_;

    cursor_ = (cursor_ + count) % maxParticles;
    ++frameSeed_;

    return batch;
}

void ParticleEmitter::reset()
{
    accum_    = 0.f;
    cursor_   = 0;
    frameSeed_ = 1;
}

} // namespace Phantom::Particles
