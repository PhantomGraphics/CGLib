#pragma once

#include "GltfDocument.h"

#define GLM_FORCE_RADIANS
#include <glm/glm.hpp>

#include <vector>

namespace Phantom::Gltf {

    // A glTF camera/light positioned by its owning node's world transform. A camera/light
    // *definition* (GltfDocument::cameras[cameraIndex] / lights[lightIndex]) can in principle be
    // referenced by more than one node -- each occurrence gets its own instance here. Per the
    // glTF spec, a camera/light node points along its local -Z axis; extract a world-space
    // direction with glm::vec3(worldMatrix * glm::vec4(0,0,-1,0)) and a position with
    // glm::vec3(worldMatrix[3]).
    struct GltfLightInstance {
        int       lightIndex = -1;  // into GltfDocument::lights
        glm::mat4 worldMatrix{ 1.f };
    };

    struct GltfCameraInstance {
        int       cameraIndex = -1; // into GltfDocument::cameras
        glm::mat4 worldMatrix{ 1.f };
    };

    struct GltfSceneLightsCameras {
        std::vector<GltfLightInstance>  lights;
        std::vector<GltfCameraInstance> cameras;
    };

    // Walks doc's default scene node hierarchy (the same traversal computeGltfBounds() /
    // GltfSceneRenderer::traverseNode() use) collecting every node with a lightIndex/cameraIndex
    // and its world transform. Pure CPU, no Vulkan dependency -- usable from ImportReport,
    // LoadAsset, or a scenario command alike.
    GltfSceneLightsCameras collectGltfLightsAndCameras(const GltfDocument& doc);

}
