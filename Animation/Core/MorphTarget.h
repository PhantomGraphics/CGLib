#pragma once

#define GLM_FORCE_RADIANS
#include <glm/glm.hpp>

#include <string>
#include <vector>

namespace Phantom::Animation {

struct MorphVertex {
    int       vertexIndex   = 0;
    glm::vec3 positionOffset{0.f, 0.f, 0.f};
};

struct MorphTarget {
    std::string              name;
    std::vector<MorphVertex> deltas;
};

struct MorphState {
    std::vector<float> weights; // weights[i] in [0,1], indexed per MorphTarget
};

} // namespace Phantom::Animation
