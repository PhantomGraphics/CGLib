#pragma once

#define GLM_FORCE_RADIANS
#include <glm/glm.hpp>

#include "GltfDocument.h"

namespace Phantom::Gltf {

    // A node's local matrix: its `matrix` if it has one, otherwise T * R * S (glTF spec).
    // Vulkan-free, so scenario/CPU-only code can use it too.
    glm::mat4 nodeLocalMatrix(const GltfNode& node);

}
