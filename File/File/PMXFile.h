#pragma once
#include <array>
#include <cstdint>
#include <string>
#include <vector>

// PMX 2.0 format raw structures parsed by PMXFileReader.
// Indices are variable-width (1/2/4 bytes), so no packed structs are used;
// PMXFileReader normalises everything to int32_t / std::string.

namespace Phantom {
    namespace File {

struct PMXGlobals {
    uint8_t encoding;            // 0=UTF-16LE, 1=UTF-8
    uint8_t additionalUVs;       // 0-4 extra UV sets per vertex
    uint8_t vertexIndexSize;     // 1, 2, or 4 bytes
    uint8_t textureIndexSize;    // 1, 2, or 4 bytes
    uint8_t materialIndexSize;   // 1, 2, or 4 bytes
    uint8_t boneIndexSize;       // 1, 2, or 4 bytes
    uint8_t morphIndexSize;      // 1, 2, or 4 bytes
    uint8_t rigidBodyIndexSize;  // 1, 2, or 4 bytes
};

// Per-vertex skinning data.
// BDEF1/2/4 and SDEF are all normalised into the 4-bone layout here.
struct PMXVertex {
    float    position[3];
    float    normal[3];
    float    uv[2];
    std::vector<std::array<float, 4>> additionalUV; // size == globals.additionalUVs
    uint8_t  weightType;        // 0=BDEF1, 1=BDEF2, 2=BDEF4, 3=SDEF
    int32_t  boneIndices[4];    // -1 = unused slot
    float    boneWeights[4];    // sum == 1; unused slots are 0
    // SDEF extra data (weightType == 3) — stored for completeness
    float    sdefC[3];
    float    sdefR0[3];
    float    sdefR1[3];
    float    edgeFactor;
};

struct PMXMaterial {
    std::string name;              // JP (UTF-8)
    std::string nameEN;            // EN (UTF-8)
    float       diffuse[4];        // RGBA
    float       specular[3];       // RGB
    float       specularity;
    float       ambient[3];        // RGB
    uint8_t     drawFlags;
    float       edgeColor[4];      // RGBA
    float       edgeSize;
    int32_t     textureIndex;      // -1 = none
    int32_t     sphereTextureIndex;// -1 = none
    uint8_t     sphereMode;        // 0=disabled, 1=mul, 2=add, 3=sub-texture
    uint8_t     toonSharingFlag;   // 0=separate, 1=shared (index is byte 0-9)
    int32_t     toonTextureIndex;
    std::string comment;
    int32_t     faceCount;         // index count (triangles * 3)
};

struct PMXIKLink {
    int32_t boneIndex;
    bool    hasAngleLimit;
    float   angleMin[3];  // XYZ, radians
    float   angleMax[3];
};

struct PMXBone {
    std::string name;              // JP (UTF-8)
    std::string nameEN;            // EN (UTF-8)
    float       position[3];
    int32_t     parentBoneIndex;   // -1 = no parent
    int32_t     transformLayer;
    uint16_t    flags;
    // Tail: bone index if (flags & 0x0001), else position offset
    int32_t     tailBoneIndex;     // valid when (flags & 0x0001)
    float       tailPosition[3];   // valid when !(flags & 0x0001)
    // Grant / inherit (flags & 0x0100) || (flags & 0x0080)
    int32_t     inheritBoneIndex;
    float       inheritInfluence;
    // Fixed axis (flags & 0x0200)
    float       fixedAxis[3];
    // Local axis (flags & 0x0400)
    float       localAxisX[3];
    float       localAxisZ[3];
    // External parent transform (flags & 0x1000)
    int32_t     externalParentKey;
    // IK (flags & 0x0020)
    int32_t     ikTargetBoneIndex;
    int32_t     ikLoopCount;
    float       ikAngleLimit;      // radians per iteration
    std::vector<PMXIKLink> ikLinks;
};

// Only vertex morphs (morphType == 1) are stored; others are skipped.
struct PMXVertexMorph {
    int32_t vertexIndex;
    float   offset[3];
};

struct PMXMorph {
    std::string name;      // JP (UTF-8)
    std::string nameEN;    // EN (UTF-8)
    uint8_t     category;  // 0=system, 1=eyebrow, 2=eye, 3=mouth, 4=other
    uint8_t     morphType; // 1=vertex (others are skipped)
    std::vector<PMXVertexMorph> vertexMorphs; // populated only for morphType == 1
};

struct PMXFile {
    float       version;      // 2.0 or 2.1
    PMXGlobals  globals;
    std::string modelNameJP;
    std::string modelNameEN;
    std::string commentJP;
    std::string commentEN;
    std::vector<PMXVertex>   vertices;
    std::vector<int32_t>     indices;   // face vertex indices; size % 3 == 0
    std::vector<std::string> textures;
    std::vector<PMXMaterial> materials;
    std::vector<PMXBone>     bones;
    std::vector<PMXMorph>    morphs;
};

    } // namespace File
} // namespace Phantom
