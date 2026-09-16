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

GltfCameraViewProj computeCameraViewProj(const GltfCamera& cam, const glm::mat4& worldMatrix,
                                          float viewportAspect, float fallbackFar)
{
    GltfCameraViewProj out;

    out.view = glm::inverse(worldMatrix);
    out.eye  = glm::vec3(worldMatrix[3]);

    const bool  ortho  = (cam.type == "Orthographic");
    const float znear  = cam.znear > 0.f ? cam.znear : 0.05f;
    const float zfar   = cam.zfar  > 0.f ? cam.zfar  : fallbackFar;
    const float aspect = cam.aspectRatio > 0.f
        ? cam.aspectRatio
        : (viewportAspect > 0.f ? viewportAspect : 1.f);

    out.proj = ortho
        ? glm::ortho(-cam.xmag, cam.xmag, -cam.ymag, cam.ymag, znear, zfar)
        : glm::perspective(cam.yfov, aspect, znear, zfar);
    out.proj[1][1] *= -1.f; // Vulkan Y-flip

    return out;
}

GltfCameraViewProj computeCameraViewProj(const GltfDocument& doc, const GltfCameraInstance& inst,
                                          float viewportAspect, float fallbackFar)
{
    return computeCameraViewProj(doc.cameras[inst.cameraIndex], inst.worldMatrix, viewportAspect, fallbackFar);
}

}
