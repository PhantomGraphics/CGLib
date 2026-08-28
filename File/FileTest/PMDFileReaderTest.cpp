#include "gtest/gtest.h"
#include "../File/PMDFileReader.h"

#include <cstring>
#include <sstream>
#include <vector>

using namespace Phantom::File;

namespace {

// Helper to build a minimal valid PMD binary in memory
std::string buildMinimalPMD(int vertCount = 0, int idxCount = 0,
                              int matCount = 0, int boneCount = 0)
{
    std::vector<uint8_t> data;
    auto pushT = [&](auto v) {
        const uint8_t* p = reinterpret_cast<const uint8_t*>(&v);
        for (size_t i = 0; i < sizeof(v); ++i) data.push_back(p[i]);
    };
    auto pushBytes = [&](const char* s, int n) {
        int slen = static_cast<int>(std::strlen(s));
        for (int i = 0; i < n; ++i) data.push_back(i < slen ? s[i] : 0);
    };

    // Header
    pushBytes("Pmd", 3);
    pushT(1.0f);
    pushBytes("TestModel", 20);
    pushBytes("", 256);

    // Vertices
    pushT(static_cast<uint32_t>(vertCount));
    for (int i = 0; i < vertCount; ++i) {
        PMDVertex v{};
        v.pos[0] = static_cast<float>(i) + 1.f;
        v.pos[1] = 2.f; v.pos[2] = 3.f;
        v.boneWeight = 100;
        data.insert(data.end(), reinterpret_cast<uint8_t*>(&v),
                    reinterpret_cast<uint8_t*>(&v) + sizeof(v));
    }

    // Indices
    pushT(static_cast<uint32_t>(idxCount));
    for (int i = 0; i < idxCount; ++i)
        pushT(static_cast<uint16_t>(i));

    // Materials
    pushT(static_cast<uint32_t>(matCount));
    for (int i = 0; i < matCount; ++i) {
        PMDMaterial m{};
        m.faceVertCount = 3;
        data.insert(data.end(), reinterpret_cast<uint8_t*>(&m),
                    reinterpret_cast<uint8_t*>(&m) + sizeof(m));
    }

    // Bones
    pushT(static_cast<uint16_t>(boneCount));
    for (int i = 0; i < boneCount; ++i) {
        PMDBone b{};
        std::snprintf(b.name, sizeof(b.name), "Bone%d", i);
        b.parentIndex = 0xFFFF;
        b.headPos[1]  = static_cast<float>(i) * 0.3f;
        data.insert(data.end(), reinterpret_cast<uint8_t*>(&b),
                    reinterpret_cast<uint8_t*>(&b) + sizeof(b));
    }

    // IK / Morph (required trailing sections)
    pushT(static_cast<uint16_t>(0));
    pushT(static_cast<uint16_t>(0));

    return std::string(data.begin(), data.end());
}

} // namespace

TEST(PMDFileReaderTest, ReadEmpty)
{
    std::string pmd = buildMinimalPMD();
    std::istringstream ss(pmd);
    PMDFileReader reader;
    EXPECT_TRUE(reader.read(ss));
    EXPECT_EQ(0u, reader.getPMD().vertices.size());
    EXPECT_EQ(0u, reader.getPMD().bones.size());
}

TEST(PMDFileReaderTest, ReadOneVertex)
{
    std::string pmd = buildMinimalPMD(1);
    std::istringstream ss(pmd);
    PMDFileReader reader;
    EXPECT_TRUE(reader.read(ss));
    ASSERT_EQ(1u, reader.getPMD().vertices.size());
    EXPECT_FLOAT_EQ(1.f, reader.getPMD().vertices[0].pos[0]);
    EXPECT_FLOAT_EQ(2.f, reader.getPMD().vertices[0].pos[1]);
    EXPECT_FLOAT_EQ(3.f, reader.getPMD().vertices[0].pos[2]);
    EXPECT_EQ(100, reader.getPMD().vertices[0].boneWeight);
}

TEST(PMDFileReaderTest, ReadBonesAndIndices)
{
    std::string pmd = buildMinimalPMD(3, 3, 1, 2);
    std::istringstream ss(pmd);
    PMDFileReader reader;
    EXPECT_TRUE(reader.read(ss));
    EXPECT_EQ(3u, reader.getPMD().vertices.size());
    EXPECT_EQ(3u, reader.getPMD().indices.size());
    EXPECT_EQ(1u, reader.getPMD().materials.size());
    EXPECT_EQ(2u, reader.getPMD().bones.size());
}

TEST(PMDFileReaderTest, InvalidMagicFails)
{
    std::string bad = "BAD\x00\x00\x00\x00";
    std::istringstream ss(bad);
    PMDFileReader reader;
    EXPECT_FALSE(reader.read(ss));
}

TEST(PMDFileReaderTest, EmptyStreamFails)
{
    std::istringstream ss("");
    PMDFileReader reader;
    EXPECT_FALSE(reader.read(ss));
}
