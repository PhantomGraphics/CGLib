#include "ParticleGenerator.h"

#include <algorithm>
#include <cmath>

namespace Phantom::Volume {
namespace {

glm::vec3 randomInCell(const Phantom::Math::Vector3df& center,
                       float voxelSize,
                       std::mt19937& rng) {
    std::uniform_real_distribution<float> dist(-0.5f, 0.5f);

    const float dx = dist(rng) * voxelSize;
    const float dy = dist(rng) * voxelSize;
    const float dz = dist(rng) * voxelSize;

    return glm::vec3(center.x + dx, center.y + dy, center.z + dz);
}

} // namespace

ParticleSet ParticleGenerator::generate(const Phantom::Volume::SparseVolumef& sv) const {
    ParticleSet ps;
    if (!tf_) {
        return ps;
    }

    const float voxelSize = sv.getVoxelSize();
    const float cellVol = voxelSize * voxelSize * voxelSize;
    if (voxelSize <= 0.0f || cellVol <= 0.0f) {
        return ps;
    }

    const float densityScale = std::max(densityScale_, 0.0f);
    std::uniform_real_distribution<float> dist01(0.0f, 1.0f);

    sv.forEachActive([&](const Phantom::Volume::Coord&, const Phantom::Math::Vector3df& worldPos, float value) {
        const TFSample tf = tf_->sample(value);
        if (tf.a < 1.0e-4f) {
            return;
        }

        const float nf = tf.a * densityScale * cellVol;
        int count = static_cast<int>(std::floor(nf));
        if (dist01(rng_) < (nf - static_cast<float>(count))) {
            ++count;
        }

        for (int i = 0; i < count; ++i) {
            Particle p{};
            p.pos = randomInCell(worldPos, voxelSize, rng_);
            p.color = glm::vec3(tf.r, tf.g, tf.b);
            ps.particles.push_back(p);
        }
    });

    return ps;
}

} // namespace PBVR
