#pragma once

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>

#include <cstddef>
#include <vector>

namespace Phantom::Volume {

struct Particle {
    glm::vec3 pos;
    glm::vec3 color;
};

struct ParticleSet {
    std::vector<Particle> particles;

    size_t count() const { return particles.size(); }
    void clear() { particles.clear(); }
};

} // namespace Phantom::Volume
