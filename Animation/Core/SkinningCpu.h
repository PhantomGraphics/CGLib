#pragma once
// CPU linear-blend skinning without any rendering dependency. Shared by SkinnedMeshRenderer and the C# Phantom.Animation API.
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>

#include <cstddef>

#include "SkinnedMesh.h"

namespace Phantom::Animation {

// Skins `count` bind-pose vertices with up to 4 influences each: position = sum(w_k * M[bone_k] * p).
// Influences with weight <= 0 or a bone index outside [0, skinMatrixCount) are skipped (a vertex with no valid influence
// lands on the origin, as the renderer always did).
// Normals use the upper 3x3 of each matrix (exact for rotation / uniform scale), are blended with the same weights and
// normalised; a normal that blends to zero keeps its bind-pose value.
// Outputs are tightly packed xyz floats: outPositions[3 * count], outNormals[3 * count] (outNormals may be null).
void skinVertices(const SkinVertex* vertices, std::size_t count,
                  const glm::mat4* skinMatrices, std::size_t skinMatrixCount,
                  float* outPositions, float* outNormals);

} // namespace Phantom::Animation
