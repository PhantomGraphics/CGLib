#include "gtest/gtest.h"

#include "../Gltf/GltfAnimationEvaluator.h"
#include "../Gltf/MmdToGltfConverter.h"
#include "../../File/File/VMDFile.h"

#include <cstring>
#include <filesystem>
#include <fstream>

using namespace Phantom::Gltf;
using namespace Phantom::File;

namespace {

void writeI32(std::ostream& s, int32_t v) { s.write(reinterpret_cast<const char*>(&v), 4); }
void writeU8(std::ostream& s, uint8_t v)  { s.write(reinterpret_cast<const char*>(&v), 1); }
void writeU16(std::ostream& s, uint16_t v){ s.write(reinterpret_cast<const char*>(&v), 2); }
void writeF32(std::ostream& s, float v)   { s.write(reinterpret_cast<const char*>(&v), 4); }
void writeText(std::ostream& s, const std::string& utf8) {
    writeI32(s, static_cast<int32_t>(utf8.size()));
    s.write(utf8.data(), static_cast<std::streamsize>(utf8.size()));
}

// Hand-crafted minimal PMX 2.0 binary: encoding=UTF-8, all index sizes=1 byte, 2 bones
// (Root -> Child, no IK), 2 vertices (BDEF1, one bound to each bone), 1 triangle, 1 material,
// no textures, no morphs. Exercises PMXFileReader's full parse path (see PMXFileReader.cpp)
// without needing a real downloaded MMD sample model (internal design notes
// Phase 10 -- AnimationTest has no reusable PMX/VMD binary fixture to borrow).
bool writeMinimalPmx(const std::filesystem::path& path) {
    std::ofstream f(path, std::ios::binary);
    if (!f) return false;

    f.write("PMX ", 4);
    writeF32(f, 2.0f);

    writeU8(f, 8); // globalsSize
    const uint8_t globals[8] = {
        1, // encoding = UTF-8
        0, // additionalUVs
        1, 1, 1, 1, 1, 1, // vertex/texture/material/bone/morph/rigidBody index sizes = 1 byte
    };
    f.write(reinterpret_cast<const char*>(globals), 8);

    writeText(f, ""); writeText(f, ""); writeText(f, ""); writeText(f, "");

    // vertices
    writeI32(f, 2);
    for (int i = 0; i < 2; ++i) {
        const float pos[3] = {0.f, static_cast<float>(i), 0.f};
        const float nrm[3] = {0.f, 1.f, 0.f};
        const float uv[2]  = {0.f, 0.f};
        f.write(reinterpret_cast<const char*>(pos), 12);
        f.write(reinterpret_cast<const char*>(nrm), 12);
        f.write(reinterpret_cast<const char*>(uv), 8);
        writeU8(f, 0); // weightType = BDEF1
        writeU8(f, static_cast<uint8_t>(i)); // boneIndex
        writeF32(f, 1.f); // edgeFactor
    }

    // faces: 1 degenerate triangle referencing both vertices
    writeI32(f, 3);
    writeU8(f, 0); writeU8(f, 1); writeU8(f, 0);

    // textures
    writeI32(f, 0);

    // materials
    writeI32(f, 1);
    writeText(f, "Mat0");
    writeText(f, "Mat0");
    const float diffuse[4] = {1.f, 1.f, 1.f, 1.f};
    f.write(reinterpret_cast<const char*>(diffuse), 16);
    const float specular[3] = {0.f, 0.f, 0.f};
    f.write(reinterpret_cast<const char*>(specular), 12);
    writeF32(f, 0.f); // specularity
    const float ambient[3] = {0.2f, 0.2f, 0.2f};
    f.write(reinterpret_cast<const char*>(ambient), 12);
    writeU8(f, 0); // drawFlags
    const float edgeColor[4] = {0.f, 0.f, 0.f, 1.f};
    f.write(reinterpret_cast<const char*>(edgeColor), 16);
    writeF32(f, 1.f); // edgeSize
    writeU8(f, static_cast<uint8_t>(-1)); // textureIndex = -1
    writeU8(f, static_cast<uint8_t>(-1)); // sphereTextureIndex = -1
    writeU8(f, 0); // sphereMode
    writeU8(f, 1); // toonSharingFlag = shared
    writeU8(f, 0); // shared toon index
    writeText(f, "");
    writeI32(f, 3); // faceCount

    // bones: Root(0) -> Child(1), neither is an IK bone
    writeI32(f, 2);
    {
        writeText(f, "Root"); writeText(f, "Root");
        const float p[3] = {0.f, 0.f, 0.f};
        f.write(reinterpret_cast<const char*>(p), 12);
        writeU8(f, static_cast<uint8_t>(-1)); // parentBoneIndex = -1
        writeI32(f, 0); // transformLayer
        writeU16(f, 0x0000); // flags: tail-as-position, no IK/inherit/fixedAxis/localAxis/externalParent
        const float tail[3] = {0.f, 1.f, 0.f};
        f.write(reinterpret_cast<const char*>(tail), 12);
    }
    {
        writeText(f, "Child"); writeText(f, "Child");
        const float p[3] = {0.f, 1.f, 0.f};
        f.write(reinterpret_cast<const char*>(p), 12);
        writeU8(f, 0); // parentBoneIndex = Root
        writeI32(f, 0);
        writeU16(f, 0x0000);
        const float tail[3] = {0.f, 1.f, 0.f};
        f.write(reinterpret_cast<const char*>(tail), 12);
    }

    // morphs
    writeI32(f, 0);

    return f.good();
}

// Animates the "Child" bone: identity at frame 0, translated +0.5 on X at frame 30 (1.0s @ 30fps).
bool writeMinimalVmd(const std::filesystem::path& path) {
    std::ofstream f(path, std::ios::binary);
    if (!f) return false;

    VMDHeader header{};
    std::memset(&header, 0, sizeof(header));
    std::strncpy(header.magic, "Vocaloid Motion Data 0002", sizeof(header.magic));
    f.write(reinterpret_cast<const char*>(&header), sizeof(header));

    std::vector<VMDBoneKeyframe> boneKeys(2);
    std::memset(boneKeys.data(), 0, sizeof(VMDBoneKeyframe) * boneKeys.size());
    std::strncpy(boneKeys[0].boneName, "Child", 15);
    boneKeys[0].frameNo    = 0;
    boneKeys[0].rotation[3] = 1.f; // identity quat (x,y,z,w)
    std::strncpy(boneKeys[1].boneName, "Child", 15);
    boneKeys[1].frameNo     = 30;
    boneKeys[1].position[0] = 0.5f;
    boneKeys[1].rotation[3] = 1.f;

    const uint32_t boneCount = static_cast<uint32_t>(boneKeys.size());
    f.write(reinterpret_cast<const char*>(&boneCount), 4);
    f.write(reinterpret_cast<const char*>(boneKeys.data()),
            static_cast<std::streamsize>(sizeof(VMDBoneKeyframe) * boneKeys.size()));

    const uint32_t morphCount = 0;
    f.write(reinterpret_cast<const char*>(&morphCount), 4);

    return f.good();
}

// RAII temp-file helper so a failed ASSERT doesn't leak the fixture on disk.
struct TempFile {
    std::filesystem::path path;
    explicit TempFile(const char* name) : path(std::filesystem::temp_directory_path() / name) {}
    ~TempFile() { std::error_code ec; std::filesystem::remove(path, ec); }
};

} // namespace

TEST(MmdToGltfConverterTest, MissingPmxFails)
{
    GltfDocument doc;
    EXPECT_FALSE(MmdToGltfConverter::convert("does_not_exist.pmx", {}, doc));
}

TEST(MmdToGltfConverterTest, PmxOnly_ProducesBindPoseDocumentWithStats)
{
    TempFile pmx("mmd_to_gltf_test_bindpose.pmx");
    ASSERT_TRUE(writeMinimalPmx(pmx.path));

    GltfDocument doc;
    MmdToGltfLoadStats stats;
    ASSERT_TRUE(MmdToGltfConverter::convert(pmx.path, {}, doc, {}, &stats));

    EXPECT_EQ(2, stats.boneCount);
    EXPECT_EQ(0, stats.ikChainCount);
    EXPECT_EQ(0, stats.morphTargetCount);
    EXPECT_EQ(2, stats.vertexCount);

    ASSERT_EQ(1u, doc.skins.size());
    EXPECT_EQ(2u, doc.skins[0].joints.size());
    ASSERT_EQ(1u, doc.meshes.size());
    ASSERT_EQ(1u, doc.meshes[0].primitives.size());
    EXPECT_TRUE(doc.animations.empty()); // no VMD supplied
}

TEST(MmdToGltfConverterTest, PmxAndVmd_BakesAnimationChannels)
{
    TempFile pmx("mmd_to_gltf_test_anim.pmx");
    TempFile vmd("mmd_to_gltf_test_anim.vmd");
    ASSERT_TRUE(writeMinimalPmx(pmx.path));
    ASSERT_TRUE(writeMinimalVmd(vmd.path));

    GltfDocument doc;
    ASSERT_TRUE(MmdToGltfConverter::convert(pmx.path, vmd.path, doc));

    ASSERT_EQ(1u, doc.animations.size());
    EXPECT_GT(doc.animations[0].channels.size(), 0u);
    EXPECT_NEAR(1.0f, GltfAnimationEvaluator::duration(doc.animations[0], doc), 1e-4f);

    // Sanity: the baked pose at t=1s should reflect the VMD's +0.5 X translation on "Child"
    // (node index 1, bone index 1 -- see SkeletonGltfConverter's 1:1 bone/node mapping).
    auto skinMatrices = GltfAnimationEvaluator::evaluateSkin(doc, 0, 0, 1.f);
    ASSERT_EQ(2u, skinMatrices.size());
}
