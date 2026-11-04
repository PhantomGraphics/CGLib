#include "ParticleEmitter.h"

#include <algorithm>
#include <cmath>

namespace Phantom::Effects::Particles {

ParticleEmitter::EmitBatch ParticleEmitter::update(float dt, uint32_t maxParticles)
{
    EmitBatch batch{};
    if (!enabled_ || maxParticles == 0 || !std::isfinite(dt) || dt <= 0
        || !std::isfinite(params_.emitRate) || params_.emitRate < 0)
        return batch;

    accum_ = static_cast<float>(std::min(static_cast<double>(accum_) + static_cast<double>(params_.emitRate) * dt,
        static_cast<double>(maxParticles)));
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

} // namespace Phantom::Effects::Particles
