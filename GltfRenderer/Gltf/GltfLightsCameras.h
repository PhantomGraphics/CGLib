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

    struct GltfCameraViewProj {
        glm::mat4 view{ 1.f };
        glm::mat4 proj{ 1.f };
        glm::vec3 eye{ 0.f };
    };

    // "Camera-as-scene-component": computes a Vulkan-ready (Y-flipped) view/projection/eye for
    // one camera instance from collectGltfLightsAndCameras(), so a consumer can actually look
    // through a glTF's own authored camera instead of (or in addition to) its own free/orbit
    // camera. First implemented inline in Universe's Renderer::pushAssetCameraOverride()
    // (2026-09-14); extracted here 2026-09-16 so GltfViewer can share the same math rather than
    // re-deriving it (per docs/todo/PLAN_blender_universe_authoring_loop.md's "pure rendering
    // logic belongs in CGLib/GltfRenderer" policy). A glTF camera node looks along local -Z with
    // +Y up, so the world matrix's inverse *is* the view matrix -- no glm::lookAt needed.
    // `viewportAspect` is used only when the camera itself has no aspectRatio (glTF spec
    // default: derive from the viewport) -- a caller should recompute this after every resize so
    // an aspectRatio-less perspective camera doesn't stay stretched/squashed at a stale ratio.
    // `fallbackFar` clamps glTF's optional infinite far plane (zfar omitted) to a finite value,
    // matching the fixed-far convention this codebase's other Vulkan projections use.
    //
    // This overload takes the camera definition and world matrix by value -- for a caller (e.g.
    // Universe's Renderer) that needs to keep the captured camera around past the GltfDocument's
    // own lifetime (LoadAsset's doc is often a short-lived temporary) rather than re-deriving it
    // from doc+inst on every resize.
    GltfCameraViewProj computeCameraViewProj(const GltfCamera& cam, const glm::mat4& worldMatrix,
                                              float viewportAspect, float fallbackFar = 200.f);

    // Convenience overload for a caller that still has doc/inst on hand (e.g. right after
    // collectGltfLightsAndCameras()).
    GltfCameraViewProj computeCameraViewProj(const GltfDocument& doc, const GltfCameraInstance& inst,
                                              float viewportAspect, float fallbackFar = 200.f);

}
