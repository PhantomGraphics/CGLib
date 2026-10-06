#include "GltfBounds.h"
#include "GltfAccessorView.h"
#include "GltfNodeTransform.h"

#define GLM_FORCE_RADIANS
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

namespace Phantom::Gltf {

namespace {

void expandByPrimitive(const GltfDocument& doc, const GltfPrimitive& prim,
                        const glm::mat4& world, GltfAabb& out)
{
    if (prim.positionAccessor < 0) return;
    GltfAccessorView posView(doc, prim.positionAccessor);
    for (size_t i = 0; i < posView.count(); ++i) {
        const glm::vec3 local = posView.get<glm::vec3>(i);
        const glm::vec3 p     = glm::vec3(world * glm::vec4(local, 1.f));
        if (!out.valid) {
            out.min   = p;
            out.max   = p;
            out.valid = true;
        } else {
            out.min = glm::min(out.min, p);
            out.max = glm::max(out.max, p);
        }
    }
}

void traverseNode(const GltfDocument& doc, int nodeIndex, const glm::mat4& parentTransform, GltfAabb& out)
{
    if (nodeIndex < 0 || nodeIndex >= static_cast<int>(doc.nodes.size())) return;

    const auto&     node  = doc.nodes[nodeIndex];
    const glm::mat4 world = parentTransform * nodeLocalMatrix(node);

    if (node.meshIndex >= 0 && node.meshIndex < static_cast<int>(doc.meshes.size())) {
        const auto& mesh = doc.meshes[node.meshIndex];
        for (const auto& prim : mesh.primitives)
            expandByPrimitive(doc, prim, world, out);
    }

    for (int child : node.children)
        traverseNode(doc, child, world, out);
}

} // namespace

GltfAabb computeGltfBounds(const GltfDocument& doc)
{
    GltfAabb out;
    if (doc.scenes.empty()) return out;

    int sceneIdx = doc.defaultScene;
    if (sceneIdx < 0 || sceneIdx >= static_cast<int>(doc.scenes.size())) sceneIdx = 0;

    for (int rootNode : doc.scenes[sceneIdx].nodes)
        traverseNode(doc, rootNode, glm::mat4(1.f), out);

    return out;
}

}
