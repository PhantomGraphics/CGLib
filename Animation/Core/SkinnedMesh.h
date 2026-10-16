#pragma once

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>

#include <vector>
#include <cstdint>

namespace Phantom::Animation {

struct SkinVertex {
    glm::vec3  position{0.f, 0.f, 0.f};
    glm::vec3  normal{0.f, 1.f, 0.f};
    glm::vec2  texCoord{0.f, 0.f};
    glm::vec4  color{1.f, 1.f, 1.f, 1.f};
    glm::ivec4 boneIndices{0, 0, 0, 0}; // up to 4 bone influences
    glm::vec4  boneWeights{1.f, 0.f, 0.f, 0.f}; // must sum to 1.0
};

struct SkinnedMesh {
    std::vector<SkinVertex> vertices;
    std::vector<uint32_t>   indices;
    int                     materialIndex = -1;
};

} // namespace Phantom::Animation
