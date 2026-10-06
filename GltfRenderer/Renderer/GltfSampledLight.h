#pragma once
#include <glm/glm.hpp>

namespace Phantom::Gltf {
// One importance-sampled point light. Flux is divided by selection probability;
// a caller averages independently shadowed draws to estimate all light sources.
// Matches the fragment push constants at byte 64 in gltf_sampled_light.glsl.
struct GltfSampledLight {
    glm::vec4 positionGain{0};
    glm::vec4 fluxRadius{0};
};
static_assert(sizeof(GltfSampledLight)==32);
}
