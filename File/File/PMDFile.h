#pragma once
#include <cstdint>
#include <vector>

// PMD binary format raw structs. All packed to match on-disk layout.
// Shift-JIS strings are kept as raw byte arrays.

namespace Phantom {
    namespace File {

#pragma pack(push, 1)

struct PMDHeader {
    char    magic[3];       // "Pmd"
    float   version;
    char    modelName[20];  // Shift-JIS
    char    comment[256];
};

struct PMDVertex {
    float    pos[3];
    float    normal[3];
    float    uv[2];
    uint16_t boneNum[2];
    uint8_t  boneWeight;    // bone[0] influence 0-100
    uint8_t  edgeFlag;
};
static_assert(sizeof(PMDVertex) == 38, "PMDVertex size mismatch");

struct PMDMaterial {
    float    diffuse[4];
    float    specularity;
    float    specular[3];
    float    ambient[3];
    uint8_t  toonIndex;
    uint8_t  edgeFlag;
    uint32_t faceVertCount;
    char     textureFile[20];
};
static_assert(sizeof(PMDMaterial) == 70, "PMDMaterial size mismatch");

struct PMDBone {
    char     name[20];       // Shift-JIS
    uint16_t parentIndex;    // 0xFFFF = root
    uint16_t tailIndex;
    uint8_t  type;
    uint16_t ikParent;
    float    headPos[3];
};
static_assert(sizeof(PMDBone) == 39, "PMDBone size mismatch");

// IK entry on-disk header (link bones follow as uint16_t[linkCount])
struct PMDIKHeader {
    uint16_t ikBoneIndex;     // IK controller bone (e.g. foot IK)
    uint16_t targetBoneIndex; // effector tip bone (e.g. ankle)
    uint8_t  linkCount;
    uint16_t iterationCount;
    float    controlWeight;   // angle limit factor
};
static_assert(sizeof(PMDIKHeader) == 11, "PMDIKHeader size mismatch");

// Morph (face) entry on-disk header (vertices follow as PMDMorphVertexPacked[vertexCount])
struct PMDMorphHeader {
    char     name[20];      // Shift-JIS
    uint32_t vertexCount;
    uint8_t  category;      // 0=base, 1=eyebrow, 2=eye, 3=mouth, 4=other
};
static_assert(sizeof(PMDMorphHeader) == 25, "PMDMorphHeader size mismatch");

struct PMDMorphVertexPacked {
    uint32_t vertexIndex;   // global idx (base morph) or base-local idx (other morphs)
    float    posOffset[3];
};
static_assert(sizeof(PMDMorphVertexPacked) == 16, "PMDMorphVertexPacked size mismatch");

#pragma pack(pop)

// Non-packed aggregate structs (built by PMDFileReader)

struct PMDIKRecord {
    uint16_t              ikBoneIndex;     // IK controller bone
    uint16_t              targetBoneIndex; // effector tip bone
    uint16_t              iterationCount;
    float                 controlWeight;   // angle limit per iteration
    std::vector<uint16_t> linkBones;       // chain bones, child → parent order
};

struct PMDMorphVertexData {
    uint32_t vertexIndex;   // global (base) or base-local (non-base) vertex index
    float    posOffset[3];
};

struct PMDMorphRecord {
    char                            name[20]; // Shift-JIS (no null termination guaranteed)
    uint8_t                         category; // 0=base, 1=eyebrow, 2=eye, 3=mouth, 4=other
    std::vector<PMDMorphVertexData> vertices;
};

struct PMDFile {
    PMDHeader                header;
    std::vector<PMDVertex>   vertices;
    std::vector<uint16_t>    indices;
    std::vector<PMDMaterial> materials;
    std::vector<PMDBone>     bones;
    std::vector<PMDIKRecord>    iks;
    std::vector<PMDMorphRecord> morphs;
};

    } // namespace File
} // namespace Phantom
