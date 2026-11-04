#include "SkinningCpu.h"

namespace Phantom::Animation {

void skinVertices(const SkinVertex* vertices, std::size_t count,
                  const glm::mat4* skinMatrices, std::size_t skinMatrixCount,
                  float* outPositions, float* outNormals)
{
    for (std::size_t i = 0; i < count; ++i) {
        const SkinVertex& v = vertices[i];
        const glm::vec4 p{v.position, 1.f};
        glm::vec4 sp{0.f};
        glm::vec3 sn{0.f};
        for (int k = 0; k < 4; ++k) {
            const float w = v.boneWeights[k];
            if (w <= 0.f) continue;
            const int bi = v.boneIndices[k];
            if (bi < 0 || static_cast<std::size_t>(bi) >= skinMatrixCount) continue;
            sp += w * (skinMatrices[bi] * p);
            if (outNormals != nullptr) sn += w * (glm::mat3(skinMatrices[bi]) * v.normal);
        }
        outPositions[3 * i] = sp.x;
        outPositions[3 * i + 1] = sp.y;
        outPositions[3 * i + 2] = sp.z;
        if (outNormals != nullptr) {
            const float len = glm::length(sn);
            const glm::vec3 n = len > 1e-12f ? sn / len : v.normal;
            outNormals[3 * i] = n.x;
            outNormals[3 * i + 1] = n.y;
            outNormals[3 * i + 2] = n.z;
        }
    }
}

} // namespace Phantom::Animation
