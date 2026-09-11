#include "GltfLightsCameras.h"

#define GLM_FORCE_RADIANS
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

namespace Phantom::Gltf {

namespace {

// Mirrors GltfSceneRenderer::nodeLocalTransform() / GltfBounds.cpp's copy of the same -- kept as
// a separate copy rather than shared for the same reason GltfBounds.cpp gives: GltfSceneRenderer's
// version is a private member function, and this file is Vulkan-free by design.
glm::mat4 nodeLocalTransform(const GltfNode& node) {
    if (node.hasMatrix) return node.matrix;
    glm::mat4 T = glm::translate(glm::mat4(1.f), node.translation);
    glm::quat q(node.rotation.w, node.rotation.x, node.rotation.y, node.rotation.z);
    glm::mat4 R = glm::mat4_cast(q);
    glm::mat4 S = glm::scale(glm::mat4(1.f), node.scale);
    return T * R * S;
}

void traverseNode(const GltfDocument& doc, int nodeIndex, const glm::mat4& parentTransform,
                   GltfSceneLightsCameras& out)
{
    if (nodeIndex < 0 || nodeIndex >= static_cast<int>(doc.nodes.size())) return;

    const auto&     node  = doc.nodes[nodeIndex];
    const glm::mat4 world = parentTransform * nodeLocalTransform(node);

    if (node.lightIndex >= 0 && node.lightIndex < static_cast<int>(doc.lights.size()))
        out.lights.push_back({ node.lightIndex, world });
    if (node.cameraIndex >= 0 && node.cameraIndex < static_cast<int>(doc.cameras.size()))
        out.cameras.push_back({ node.cameraIndex, world });

    for (int child : node.children)
        traverseNode(doc, child, world, out);
}

} // namespace

GltfSceneLightsCameras collectGltfLightsAndCameras(const GltfDocument& doc)
{
    GltfSceneLightsCameras out;
    if (doc.scenes.empty()) return out;

    int sceneIdx = doc.defaultScene;
    if (sceneIdx < 0 || sceneIdx >= static_cast<int>(doc.scenes.size())) sceneIdx = 0;

    for (int rootNode : doc.scenes[sceneIdx].nodes)
        traverseNode(doc, rootNode, glm::mat4(1.f), out);

    return out;
}

}
