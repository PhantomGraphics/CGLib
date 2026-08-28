#pragma once

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <cstdint>

namespace Phantom::Gltf
{

constexpr uint32_t kMaxGltfBones = 256;

// set=0 binding 5: per-frame joint matrix UBO (vert only). Index i corresponds to position i
// within the GltfSkin::joints array a skinned primitive's JOINTS_0 values index into, not a
// node index. Written each frame from GltfSceneRenderer::updateSkinMatrices(); bones beyond
// what was supplied (or the whole array, if it was never called) default to identity, which is
// what every unskinned Vertex (jointIndices=(0,0,0,0), jointWeights=(1,0,0,0)) resolves to.
struct BoneUBO {
    glm::mat4 bones[kMaxGltfBones];
}; // 16384 bytes, std140 OK

// set=0 binding 0: per-frame global UBO (vert + frag)
struct GlobalUBO {
    glm::mat4 model;          // identity unless overridden via GltfSceneRenderer::setModelMatrix()
                               // (node transforms within a document are still baked at build time)
    glm::mat4 view;
    glm::mat4 proj;
    glm::mat4 lightVP;        // shadow-casting light's view-projection (Phase C); unused when shadowEnabled=0
    glm::vec4 camPos;         // w unused
    glm::vec4 lightPos;       // w=0: directional, w=1: point
    glm::vec4 lightColor;     // w=intensity
    int       useIBL;
    int       shadowEnabled;  // Phase C: set via GltfSceneRenderer::setShadowMap()/clearShadowMap()
    float     shadowBias;
    float     shadowStrength; // 0 = no shadow attenuation, 1 = full attenuation
};  // 320 bytes, std140 OK

// set=1 binding 0: per-material UBO (frag)
struct MaterialUBO {
    glm::vec4 baseColorFactor;
    float     metallicFactor;
    float     roughnessFactor;
    float     normalScale;
    float     occlusionStrength;
    glm::vec3 emissiveFactor;
    int       hasBaseColorTex;
    int       hasMetallicRoughnessTex;
    int       hasNormalTex;
    int       hasOcclusionTex;
    int       hasEmissiveTex;
    float     _pad[3];
};

}
