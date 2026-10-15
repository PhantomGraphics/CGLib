#pragma once

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>

namespace Phantom::Gltf {

// CPU-side description of a single light. Consumed by LightManager::buildGpuBuffer() into
// LightGpu. Sampled by Universe's gltf.frag (GltfSceneRenderer::setPunctualLights(),
// docs/todo/PLAN_blender_universe_authoring_loop.md Phase 4B) when a loaded glTF document
// carries KHR_lights_punctual lights; every other GltfSceneRenderer consumer leaves this empty
// and keeps using the single lightPos_/lightColor_ pair in GlobalUBO unchanged.
struct LightEntry {
    enum class Type { Directional = 0, Point = 1, Spot = 2 };

    Type      type        = Type::Directional;
    glm::vec3 position     = { 0.f, 10.f, 0.f };
    glm::vec3 direction    = { 0.f, -1.f, 0.f };
    glm::vec3 color        = { 1.f, 1.f, 1.f };
    float     intensity    = 1.0f;
    float     range        = 20.0f;  // Point/Spot only; <= 0 = infinite (no cutoff)
    float     spotAngleDeg = 30.0f;  // Spot only -- the *outer* cone half-angle (fully dark beyond it)
    float     spotInnerAngleDeg = 0.0f; // Spot only -- the *inner* cone half-angle (fully bright within
                                         // it; smoothly falls off between inner and outer). 0 = hard edge.
    bool      castShadow   = false;
};

} // namespace Phantom::Gltf
