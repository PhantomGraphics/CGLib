#include "GltfLightShadowState.h"
#include "GltfSceneRenderer.h"

namespace Phantom::Gltf {

void applyLightShadowState(GltfSceneRenderer& renderer, const GltfLightShadowState& state)
{
    renderer.setLight(state.lightPos, state.lightColor);
    if (state.shadowCasterRenderPass != VK_NULL_HANDLE)
        renderer.createShadowPipeline(state.shadowCasterRenderPass);
    if (state.shadowEnabled)
        renderer.setShadowMap(state.shadowView, state.shadowSampler, state.shadowVP);
}

}
