#pragma once
#include <cstdint>
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>

namespace Phantom::Animation {

struct MmdMaterialProps {
    glm::vec4 diffuse;       // RGBA
    glm::vec3 specular;
    float     specularity;
    glm::vec3 ambient;
    uint8_t   drawFlags;     // PMX drawFlags
    glm::vec4 edgeColor;
    float     edgeSize;
    uint8_t   sphereMode;    // 0=off 1=mul 2=add
    bool      toonShared;    // toonSharingFlag == 1
};

struct MmdSubMesh {
    uint32_t         indexOffset;      // index number (not byte offset)
    uint32_t         indexCount;
    int              textureIdx;       // PMXFile.textures[] index, -1=none
    int              toonTextureIdx;   // -1=none, 0-9=shared toon
    int              sphereTextureIdx; // -1=none
    MmdMaterialProps props;
};

} // namespace Phantom::Animation
