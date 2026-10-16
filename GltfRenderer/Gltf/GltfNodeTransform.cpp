#include "GltfNodeTransform.h"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

namespace Phantom::Gltf {

glm::mat4 nodeLocalMatrix(const GltfNode& node) {
    if (node.hasMatrix) return node.matrix;
    const glm::mat4 T = glm::translate(glm::mat4(1.f), node.translation);
    const glm::quat q(node.rotation.w, node.rotation.x, node.rotation.y, node.rotation.z);
    const glm::mat4 R = glm::mat4_cast(q);
    const glm::mat4 S = glm::scale(glm::mat4(1.f), node.scale);
    return T * R * S;
}

int findNodeForMesh(const GltfDocument& doc, int meshIndex) {
    for (size_t i = 0; i < doc.nodes.size(); ++i)
        if (doc.nodes[i].meshIndex == meshIndex) return static_cast<int>(i);
    return -1;
}

}
