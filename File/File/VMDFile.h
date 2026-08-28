#pragma once
#include <cstdint>
#include <vector>

// VMD binary format raw structs. All packed to match on-disk layout.
// Shift-JIS strings are kept as raw byte arrays.

namespace Phantom {
    namespace File {

#pragma pack(push, 1)

struct VMDHeader {
    char magic[30];      // "Vocaloid Motion Data 0002" + padding
    char modelName[20];  // Shift-JIS
};
static_assert(sizeof(VMDHeader) == 50, "VMDHeader size mismatch");

struct VMDBoneKeyframe {
    char     boneName[15];      // Shift-JIS
    uint32_t frameNo;
    float    position[3];
    float    rotation[4];       // XYZW quaternion
    uint8_t  interpolation[64]; // Bezier control points per channel
};
static_assert(sizeof(VMDBoneKeyframe) == 111, "VMDBoneKeyframe size mismatch");

struct VMDMorphKeyframe {
    char     morphName[15];  // Shift-JIS
    uint32_t frameNo;
    float    weight;
};
static_assert(sizeof(VMDMorphKeyframe) == 23, "VMDMorphKeyframe size mismatch");

#pragma pack(pop)

struct VMDFile {
    VMDHeader                     header;
    std::vector<VMDBoneKeyframe>  boneKeyframes;
    std::vector<VMDMorphKeyframe> morphKeyframes;
};

    } // namespace File
} // namespace Phantom
