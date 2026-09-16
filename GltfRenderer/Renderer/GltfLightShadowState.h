#pragma once

#define GLM_FORCE_RADIANS
#include <glm/glm.hpp>
#include <vulkan/vulkan.h>

namespace Phantom::Gltf {

    class GltfSceneRenderer;

    // The "one shared directional light + one shared shadow map" state that a caller managing
    // many GltfSceneRenderer instances (one per entity/body/etc.) needs to push to both existing
    // and newly created instances. Universe's Rendering/GltfRenderer, and PhysicsView's
    // GltfBodyRenderer/GltfSoftRenderer, each independently reimplement this exact shape (~6
    // scalar/handle member fields + a 2-3 line conditional applied whenever a new instance is
    // created) -- this bundles the fields into one value type and applyLightShadowState() (below)
    // bundles the conditional. Deliberately narrow in scope (2026-09-16): each of those classes
    // keeps its own instance map, attach/detach lifecycle, and per-setter fan-out loop over that
    // map -- only "what do I do to one instance" is shared here, not the collection management
    // itself (see docs/todo/PLAN_blender_universe_authoring_loop.md's refactoring-candidates
    // section for why a fuller unification was scoped down to this).
    struct GltfLightShadowState {
        glm::vec4 lightPos   = { 1.f, 1.f, 1.f, 0.f }; // matches GltfSceneRenderer's own default
        glm::vec4 lightColor = { 1.f, 1.f, 1.f, 3.f };

        bool        shadowEnabled = false;
        VkImageView shadowView    = VK_NULL_HANDLE;
        VkSampler   shadowSampler = VK_NULL_HANDLE;
        glm::mat4   shadowVP      = glm::mat4(1.f);

        // VK_NULL_HANDLE = shadow-casting not enabled (createShadowPipeline() never called).
        VkRenderPass shadowCasterRenderPass = VK_NULL_HANDLE;
    };

    // Applies every field currently set in `state` to one GltfSceneRenderer instance: setLight()
    // always, createShadowPipeline() if shadowCasterRenderPass is set, setShadowMap() if
    // shadowEnabled -- the same conditional shape Universe's GltfRenderer::attachDocument() and
    // PhysicsView's GltfBodyRenderer::makeInstance()/GltfSoftRenderer's equivalent duplicate
    // today. Deliberately mirrors that "only call what's currently active" shape rather than
    // unconditionally calling clearShadowMap() when shadowEnabled is false, since
    // GltfSceneRenderer::clearShadowMap() rewrites the global descriptor set even when there was
    // never a shadow map to clear in the first place.
    void applyLightShadowState(GltfSceneRenderer& renderer, const GltfLightShadowState& state);

}
