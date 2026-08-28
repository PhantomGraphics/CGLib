#include "PMXFileReader.h"
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>

namespace Phantom {
    namespace File {

// ---------------------------------------------------------------------------
// helpers
// ---------------------------------------------------------------------------

static bool skipBytes(std::istream& s, std::streamsize n)
{
    s.seekg(n, std::ios::cur);
    return s.good();
}

// Portable UTF-16LE -> UTF-8 (no OS API). Decodes byte pairs explicitly
// rather than reinterpreting as wchar_t*, since wchar_t is 2 bytes on
// Windows but 4 bytes on Linux/glibc -- a reinterpret_cast<const wchar_t*>
// over PMX's on-disk UTF-16LE bytes is only correct on Windows.
static std::string utf16leToUtf8(const uint8_t* bytes, int byteLen)
{
    std::string out;
    const int count = byteLen / 2;
    out.reserve(static_cast<size_t>(count));
    for (int i = 0; i < count; ++i) {
        uint32_t cp = static_cast<uint32_t>(bytes[2 * i]) |
                      (static_cast<uint32_t>(bytes[2 * i + 1]) << 8);
        if (cp >= 0xD800 && cp <= 0xDBFF && i + 1 < count) {
            const uint32_t lo = static_cast<uint32_t>(bytes[2 * (i + 1)]) |
                                 (static_cast<uint32_t>(bytes[2 * (i + 1) + 1]) << 8);
            if (lo >= 0xDC00 && lo <= 0xDFFF) {
                cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00);
                ++i;
            }
        }
        if (cp <= 0x7F) {
            out.push_back(static_cast<char>(cp));
        } else if (cp <= 0x7FF) {
            out.push_back(static_cast<char>(0xC0 | (cp >> 6)));
            out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        } else if (cp <= 0xFFFF) {
            out.push_back(static_cast<char>(0xE0 | (cp >> 12)));
            out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        } else {
            out.push_back(static_cast<char>(0xF0 | (cp >> 18)));
            out.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        }
    }
    return out;
}

// ---------------------------------------------------------------------------
// static helpers
// ---------------------------------------------------------------------------

std::string PMXFileReader::readText(std::istream& s, bool utf16)
{
    int32_t len = 0;
    s.read(reinterpret_cast<char*>(&len), 4);
    if (!s || len < 0) return {};
    if (len == 0) return {};

    std::vector<uint8_t> buf(static_cast<size_t>(len));
    s.read(reinterpret_cast<char*>(buf.data()), len);
    if (!s) return {};

    if (!utf16) {
        return std::string(reinterpret_cast<const char*>(buf.data()),
                           static_cast<size_t>(len));
    }

    // UTF-16LE → UTF-8
    if (len % 2 != 0) return {};
    return utf16leToUtf8(buf.data(), len);
}

int32_t PMXFileReader::readSignedIndex(std::istream& s, int indexSize)
{
    switch (indexSize) {
        case 1: { int8_t  v = 0; s.read(reinterpret_cast<char*>(&v), 1); return v; }
        case 2: { int16_t v = 0; s.read(reinterpret_cast<char*>(&v), 2); return v; }
        case 4: { int32_t v = 0; s.read(reinterpret_cast<char*>(&v), 4); return v; }
        default: return -1;
    }
}

int32_t PMXFileReader::readUnsignedIndex(std::istream& s, int indexSize)
{
    switch (indexSize) {
        case 1: { uint8_t  v = 0; s.read(reinterpret_cast<char*>(&v), 1);
                  return static_cast<int32_t>(v); }
        case 2: { uint16_t v = 0; s.read(reinterpret_cast<char*>(&v), 2);
                  return static_cast<int32_t>(v); }
        case 4: { int32_t  v = 0; s.read(reinterpret_cast<char*>(&v), 4); return v; }
        default: return 0;
    }
}

// ---------------------------------------------------------------------------
// public API
// ---------------------------------------------------------------------------

bool PMXFileReader::read(const std::filesystem::path& path)
{
    std::ifstream ifs(path, std::ios::binary);
    if (!ifs) return false;
    return readFrom(ifs);
}

bool PMXFileReader::read(std::istream& stream)
{
    return readFrom(stream);
}

// ---------------------------------------------------------------------------
// core parser
// ---------------------------------------------------------------------------

bool PMXFileReader::readFrom(std::istream& s)
{
    pmx_   = PMXFile{};
    stats_ = PMXParseStats{};

    // --- header ---
    char magic[4];
    s.read(magic, 4);
    if (!s || std::strncmp(magic, "PMX ", 4) != 0) return false;

    s.read(reinterpret_cast<char*>(&pmx_.version), 4);
    if (!s) return false;

    uint8_t globalsSize = 0;
    s.read(reinterpret_cast<char*>(&globalsSize), 1);
    if (!s || globalsSize < 8) return false;

    uint8_t gBuf[32] = {};
    s.read(reinterpret_cast<char*>(gBuf), globalsSize);
    if (!s) return false;

    pmx_.globals.encoding          = gBuf[0];
    pmx_.globals.additionalUVs     = gBuf[1];
    pmx_.globals.vertexIndexSize   = gBuf[2];
    pmx_.globals.textureIndexSize  = gBuf[3];
    pmx_.globals.materialIndexSize = gBuf[4];
    pmx_.globals.boneIndexSize     = gBuf[5];
    pmx_.globals.morphIndexSize    = gBuf[6];
    pmx_.globals.rigidBodyIndexSize= gBuf[7];

    const bool utf16 = (pmx_.globals.encoding == 0);
    const PMXGlobals& g = pmx_.globals;

    // --- model info ---
    pmx_.modelNameJP = readText(s, utf16);
    pmx_.modelNameEN = readText(s, utf16);
    pmx_.commentJP   = readText(s, utf16);
    pmx_.commentEN   = readText(s, utf16);
    if (!s) return false;

    // --- vertices ---
    int32_t vertCount = 0;
    s.read(reinterpret_cast<char*>(&vertCount), 4);
    if (!s || vertCount < 0) { stats_.failedAt = "vertices:count"; return false; }
    stats_.vertCount = vertCount;

    pmx_.vertices.resize(static_cast<size_t>(vertCount));
    int vertIdx__ = 0;
    for (auto& v : pmx_.vertices) {
        s.read(reinterpret_cast<char*>(v.position), 12);
        s.read(reinterpret_cast<char*>(v.normal),   12);
        s.read(reinterpret_cast<char*>(v.uv),        8);

        for (int k = 0; k < g.additionalUVs; ++k) {
            std::array<float, 4> auv{};
            s.read(reinterpret_cast<char*>(auv.data()), 16);
            v.additionalUV.push_back(auv);
        }

        s.read(reinterpret_cast<char*>(&v.weightType), 1);
        if (!s) { char buf[64]; std::snprintf(buf,sizeof(buf),"vertices[%d]:weightType",vertIdx__); stats_.failedAt=buf; return false; }

        // Initialise to "no influence"
        v.boneIndices[0] = v.boneIndices[1] = v.boneIndices[2] = v.boneIndices[3] = -1;
        v.boneWeights[0] = v.boneWeights[1] = v.boneWeights[2] = v.boneWeights[3] = 0.f;
        std::memset(v.sdefC, 0, sizeof(v.sdefC));
        std::memset(v.sdefR0, 0, sizeof(v.sdefR0));
        std::memset(v.sdefR1, 0, sizeof(v.sdefR1));

        switch (v.weightType) {
            case 0: // BDEF1
                v.boneIndices[0] = readSignedIndex(s, g.boneIndexSize);
                v.boneWeights[0] = 1.f;
                break;
            case 1: // BDEF2
                v.boneIndices[0] = readSignedIndex(s, g.boneIndexSize);
                v.boneIndices[1] = readSignedIndex(s, g.boneIndexSize);
                s.read(reinterpret_cast<char*>(&v.boneWeights[0]), 4);
                v.boneWeights[1] = 1.f - v.boneWeights[0];
                break;
            case 2: // BDEF4
                v.boneIndices[0] = readSignedIndex(s, g.boneIndexSize);
                v.boneIndices[1] = readSignedIndex(s, g.boneIndexSize);
                v.boneIndices[2] = readSignedIndex(s, g.boneIndexSize);
                v.boneIndices[3] = readSignedIndex(s, g.boneIndexSize);
                s.read(reinterpret_cast<char*>(v.boneWeights), 16);
                break;
            case 3: // SDEF — treat as BDEF2 for skinning; store SDEF params for reference
                v.boneIndices[0] = readSignedIndex(s, g.boneIndexSize);
                v.boneIndices[1] = readSignedIndex(s, g.boneIndexSize);
                s.read(reinterpret_cast<char*>(&v.boneWeights[0]), 4);
                v.boneWeights[1] = 1.f - v.boneWeights[0];
                s.read(reinterpret_cast<char*>(v.sdefC),  12);
                s.read(reinterpret_cast<char*>(v.sdefR0), 12);
                s.read(reinterpret_cast<char*>(v.sdefR1), 12);
                break;
            default: {
                char buf[64]; std::snprintf(buf,sizeof(buf),"vertices[%d]:unknownWeightType=%d",vertIdx__,(int)v.weightType);
                stats_.failedAt = buf; return false;
            }
        }

        s.read(reinterpret_cast<char*>(&v.edgeFactor), 4);
        if (!s) { char buf[64]; std::snprintf(buf,sizeof(buf),"vertices[%d]:edgeFactor",vertIdx__); stats_.failedAt=buf; return false; }
        ++vertIdx__;
    }

    // --- face indices ---
    int32_t idxCount = 0;
    s.read(reinterpret_cast<char*>(&idxCount), 4);
    if (!s || idxCount < 0) { stats_.failedAt = "indices:count"; return false; }
    stats_.idxCount = idxCount;

    pmx_.indices.resize(static_cast<size_t>(idxCount));
    for (auto& idx : pmx_.indices)
        idx = readUnsignedIndex(s, g.vertexIndexSize);
    if (!s) { stats_.failedAt = "indices:data"; return false; }

    // --- textures ---
    int32_t texCount = 0;
    s.read(reinterpret_cast<char*>(&texCount), 4);
    if (!s || texCount < 0) { stats_.failedAt = "textures:count"; return false; }
    stats_.texCount = texCount;

    pmx_.textures.resize(static_cast<size_t>(texCount));
    for (auto& tex : pmx_.textures)
        tex = readText(s, utf16);
    if (!s) { stats_.failedAt = "textures:data"; return false; }

    // --- materials ---
    int32_t matCount = 0;
    s.read(reinterpret_cast<char*>(&matCount), 4);
    if (!s || matCount < 0) { stats_.failedAt = "materials:count"; return false; }
    stats_.matCount = matCount;

    pmx_.materials.resize(static_cast<size_t>(matCount));
    int matIdx__ = 0;
    for (auto& m : pmx_.materials) {
        m.name   = readText(s, utf16);
        m.nameEN = readText(s, utf16);
        s.read(reinterpret_cast<char*>(m.diffuse),      16);
        s.read(reinterpret_cast<char*>(m.specular),     12);
        s.read(reinterpret_cast<char*>(&m.specularity),  4);
        s.read(reinterpret_cast<char*>(m.ambient),      12);
        s.read(reinterpret_cast<char*>(&m.drawFlags),    1);
        s.read(reinterpret_cast<char*>(m.edgeColor),    16);
        s.read(reinterpret_cast<char*>(&m.edgeSize),     4);
        m.textureIndex        = readSignedIndex(s, g.textureIndexSize);
        m.sphereTextureIndex  = readSignedIndex(s, g.textureIndexSize);
        s.read(reinterpret_cast<char*>(&m.sphereMode),      1);
        s.read(reinterpret_cast<char*>(&m.toonSharingFlag), 1);
        if (m.toonSharingFlag == 0) {
            m.toonTextureIndex = readSignedIndex(s, g.textureIndexSize);
        } else {
            uint8_t toon = 0;
            s.read(reinterpret_cast<char*>(&toon), 1);
            m.toonTextureIndex = static_cast<int32_t>(toon);
        }
        m.comment   = readText(s, utf16);
        s.read(reinterpret_cast<char*>(&m.faceCount), 4);
        if (!s) { char buf[64]; std::snprintf(buf,sizeof(buf),"materials[%d]",matIdx__); stats_.failedAt=buf; return false; }
        ++matIdx__;
    }

    // --- bones ---
    int32_t boneCount = 0;
    s.read(reinterpret_cast<char*>(&boneCount), 4);
    if (!s || boneCount < 0) { stats_.failedAt = "bones:count"; return false; }
    stats_.boneCount = boneCount;
    { auto p = s.tellg(); stats_.boneSectionStart = (p == std::streampos(-1)) ? -1 : (int64_t)p; }

    pmx_.bones.resize(static_cast<size_t>(boneCount));
    int boneIdx__ = 0;
    for (auto& b : pmx_.bones) {
        // Helper macro: check stream and record failure location
#define BONE_CHECK(tag) \
    if (!s) { char buf[96]; \
               std::snprintf(buf,sizeof(buf),"bones[%d]:%s(flags=0x%04X)",boneIdx__,(tag),(int)b.flags); \
               stats_.failedAt=buf; return false; }

        {
            auto gpos = s.tellg();
            stats_.lastBoneStreamPos = (gpos == std::streampos(-1)) ? -1 : (int64_t)gpos;
        }
        b.name   = readText(s, utf16); BONE_CHECK("name")
        b.nameEN = readText(s, utf16); BONE_CHECK("nameEN")
        s.read(reinterpret_cast<char*>(b.position), 12); BONE_CHECK("position")
        b.parentBoneIndex = readSignedIndex(s, g.boneIndexSize); BONE_CHECK("parentBone")
        s.read(reinterpret_cast<char*>(&b.transformLayer), 4); BONE_CHECK("transformLayer")
        s.read(reinterpret_cast<char*>(&b.flags), 2); BONE_CHECK("flags")

        // tail
        if (b.flags & 0x0001) {
            b.tailBoneIndex = readSignedIndex(s, g.boneIndexSize);
            std::memset(b.tailPosition, 0, sizeof(b.tailPosition));
        } else {
            b.tailBoneIndex = -1;
            s.read(reinterpret_cast<char*>(b.tailPosition), 12);
        }
        BONE_CHECK("tail")

        // grant / inherit
        b.inheritBoneIndex = -1;
        b.inheritInfluence = 0.f;
        if ((b.flags & 0x0100) || (b.flags & 0x0080)) {
            b.inheritBoneIndex = readSignedIndex(s, g.boneIndexSize);
            s.read(reinterpret_cast<char*>(&b.inheritInfluence), 4);
            BONE_CHECK("inherit")
        }

        // fixed axis
        std::memset(b.fixedAxis, 0, sizeof(b.fixedAxis));
        if (b.flags & 0x0200) {
            s.read(reinterpret_cast<char*>(b.fixedAxis), 12);
            BONE_CHECK("fixedAxis")
        }

        // local axis
        // Note: blender_mmd_tools exports only localAxisX (12 bytes);
        // the PMX 2.0 spec says localAxisX + localAxisZ (24 bytes).
        // We read only X and derive Z via cross product to remain compatible
        // with both sources.
        std::memset(b.localAxisX, 0, sizeof(b.localAxisX));
        std::memset(b.localAxisZ, 0, sizeof(b.localAxisZ));
        if (b.flags & 0x0400) {
            s.read(reinterpret_cast<char*>(b.localAxisX), 12);
            BONE_CHECK("localAxisX")
            // Peek: if the next 4 bytes as an int32 are in [0, 4096] and even,
            // blender_mmd_tools omitted localAxisZ → skip reading it.
            // Otherwise (e.g. PMXE-exported files) read the full Z axis.
            {
                int32_t probe = 0;
                s.read(reinterpret_cast<char*>(&probe), 4);
                if (!s) { stats_.failedAt = "localAxis:probe"; return false; }
                const bool likelyStringLen = (probe >= 0 && probe <= 4096 && (probe & 1) == 0);
                if (likelyStringLen) {
                    // Put the 4 bytes back by seeking -4.
                    s.seekg(-4, std::ios::cur);
                } else {
                    // First 4 bytes of localAxisZ already read as probe; read remaining 20.
                    b.localAxisZ[0] = *reinterpret_cast<float*>(&probe);
                    s.read(reinterpret_cast<char*>(&b.localAxisZ[1]), 8);
                    BONE_CHECK("localAxisZ")
                }
            }
        }

        // external parent transform
        b.externalParentKey = 0;
        if (b.flags & 0x1000) {
            s.read(reinterpret_cast<char*>(&b.externalParentKey), 4);
            BONE_CHECK("externalParent")
        }

        // IK
        b.ikTargetBoneIndex = -1;
        b.ikLoopCount       = 0;
        b.ikAngleLimit      = 0.f;
        if (b.flags & 0x0020) {
            b.ikTargetBoneIndex = readSignedIndex(s, g.boneIndexSize);
            s.read(reinterpret_cast<char*>(&b.ikLoopCount),  4);
            s.read(reinterpret_cast<char*>(&b.ikAngleLimit), 4);
            int32_t ikLinkCount = 0;
            s.read(reinterpret_cast<char*>(&ikLinkCount), 4);
            if (!s || ikLinkCount < 0) {
                char buf[96]; std::snprintf(buf,sizeof(buf),"bones[%d]:IK:linkCount=%d",boneIdx__,ikLinkCount);
                stats_.failedAt=buf; return false;
            }
            b.ikLinks.resize(static_cast<size_t>(ikLinkCount));
            for (int il__ = 0; il__ < ikLinkCount; ++il__) {
                auto& link = b.ikLinks[static_cast<size_t>(il__)];
                link.boneIndex     = readSignedIndex(s, g.boneIndexSize);
                uint8_t hasLimit   = 0;
                s.read(reinterpret_cast<char*>(&hasLimit), 1);
                link.hasAngleLimit = (hasLimit != 0);
                std::memset(link.angleMin, 0, sizeof(link.angleMin));
                std::memset(link.angleMax, 0, sizeof(link.angleMax));
                if (link.hasAngleLimit) {
                    s.read(reinterpret_cast<char*>(link.angleMin), 12);
                    s.read(reinterpret_cast<char*>(link.angleMax), 12);
                }
                if (!s) {
                    char buf[96]; std::snprintf(buf,sizeof(buf),"bones[%d]:IK:link[%d]",boneIdx__,il__);
                    stats_.failedAt=buf; return false;
                }
            }
            BONE_CHECK("IK")
        }

#undef BONE_CHECK
        ++boneIdx__;
    }

    // --- morphs ---
    int32_t morphCount = 0;
    s.read(reinterpret_cast<char*>(&morphCount), 4);
    if (!s || morphCount < 0) { stats_.failedAt = "morphs:count"; return false; }
    stats_.morphCount = morphCount;

    pmx_.morphs.resize(static_cast<size_t>(morphCount));
    int morphIdx__ = 0;
    for (auto& morph : pmx_.morphs) {
        morph.name     = readText(s, utf16);
        morph.nameEN   = readText(s, utf16);
        s.read(reinterpret_cast<char*>(&morph.category),  1);
        s.read(reinterpret_cast<char*>(&morph.morphType), 1);

        int32_t elemCount = 0;
        s.read(reinterpret_cast<char*>(&elemCount), 4);
        if (!s || elemCount < 0) { char buf[64]; std::snprintf(buf,sizeof(buf),"morphs[%d]:elemCount",morphIdx__); stats_.failedAt=buf; return false; }

        if (morph.morphType == 1) {
            // vertex morph
            morph.vertexMorphs.resize(static_cast<size_t>(elemCount));
            for (auto& vm : morph.vertexMorphs) {
                vm.vertexIndex = readUnsignedIndex(s, g.vertexIndexSize);
                s.read(reinterpret_cast<char*>(vm.offset), 12);
            }
        } else {
            // skip unsupported morph types
            int elemSize = 0;
            switch (morph.morphType) {
                case 0:  elemSize = g.morphIndexSize  + 4;   break; // group
                case 2:  elemSize = g.boneIndexSize   + 28;  break; // bone
                case 3: case 4: case 5: case 6: case 7:
                         elemSize = g.vertexIndexSize + 16;  break; // UV
                case 8:  elemSize = g.materialIndexSize + 113; break;// material
                case 9:  elemSize = g.morphIndexSize  + 4;   break; // flip (2.1)
                case 10: elemSize = g.rigidBodyIndexSize + 25; break;// impulse (2.1)
                default: {
                    char buf[64]; std::snprintf(buf,sizeof(buf),"morphs[%d]:unknownType=%d",morphIdx__,(int)morph.morphType);
                    stats_.failedAt = buf; return false;
                }
            }
            if (!skipBytes(s, static_cast<std::streamsize>(elemCount) * elemSize)) {
                char buf[64]; std::snprintf(buf,sizeof(buf),"morphs[%d]:skip(type=%d,n=%d,sz=%d)",morphIdx__,(int)morph.morphType,elemCount,elemSize);
                stats_.failedAt = buf; return false;
            }
        }

        if (!s) { char buf[64]; std::snprintf(buf,sizeof(buf),"morphs[%d]:after",morphIdx__); stats_.failedAt=buf; return false; }
        ++morphIdx__;
    }

    stats_.success = true;
    return !s.bad();
}

    } // namespace File
} // namespace Phantom
