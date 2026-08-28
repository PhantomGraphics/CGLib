#include "gtest/gtest.h"
#include "../File/VMDFileReader.h"

#include <cstring>
#include <sstream>
#include <vector>

using namespace Phantom::File;

namespace {

std::string buildMinimalVMD(int boneFrames = 0, int morphFrames = 0)
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

    // Header: 30 bytes magic + 20 bytes model name
    pushBytes("Vocaloid Motion Data 0002", 30);
    pushBytes("TestModel", 20);

    // Bone keyframes
    pushT(static_cast<uint32_t>(boneFrames));
    for (int i = 0; i < boneFrames; ++i) {
        VMDBoneKeyframe kf{};
        std::snprintf(kf.boneName, sizeof(kf.boneName), "Center");
        kf.frameNo     = static_cast<uint32_t>(i * 30);
        kf.position[0] = static_cast<float>(i);
        kf.rotation[3] = 1.f; // identity quat w=1
        // linear interpolation (default: straight line)
        for (int j = 0; j < 64; ++j)
            kf.interpolation[j] = (j < 4 || (j >= 8 && j < 12)) ? 20 : 107;
        data.insert(data.end(), reinterpret_cast<uint8_t*>(&kf),
                    reinterpret_cast<uint8_t*>(&kf) + sizeof(kf));
    }

    // Morph keyframes
    pushT(static_cast<uint32_t>(morphFrames));
    for (int i = 0; i < morphFrames; ++i) {
        VMDMorphKeyframe kf{};
        std::snprintf(kf.morphName, sizeof(kf.morphName), "Smile");
        kf.frameNo = static_cast<uint32_t>(i * 15);
        kf.weight  = 0.5f;
        data.insert(data.end(), reinterpret_cast<uint8_t*>(&kf),
                    reinterpret_cast<uint8_t*>(&kf) + sizeof(kf));
    }

    return std::string(data.begin(), data.end());
}

} // namespace

TEST(VMDFileReaderTest, ReadEmpty)
{
    std::string vmd = buildMinimalVMD();
    std::istringstream ss(vmd);
    VMDFileReader reader;
    EXPECT_TRUE(reader.read(ss));
    EXPECT_EQ(0u, reader.getVMD().boneKeyframes.size());
    EXPECT_EQ(0u, reader.getVMD().morphKeyframes.size());
}

TEST(VMDFileReaderTest, ReadOneBoneKeyframe)
{
    std::string vmd = buildMinimalVMD(1);
    std::istringstream ss(vmd);
    VMDFileReader reader;
    EXPECT_TRUE(reader.read(ss));
    ASSERT_EQ(1u, reader.getVMD().boneKeyframes.size());
    EXPECT_EQ(0u, reader.getVMD().boneKeyframes[0].frameNo);
    EXPECT_FLOAT_EQ(0.f, reader.getVMD().boneKeyframes[0].position[0]);
}

TEST(VMDFileReaderTest, ReadMultipleBoneKeyframes)
{
    std::string vmd = buildMinimalVMD(3, 2);
    std::istringstream ss(vmd);
    VMDFileReader reader;
    EXPECT_TRUE(reader.read(ss));
    EXPECT_EQ(3u, reader.getVMD().boneKeyframes.size());
    EXPECT_EQ(2u, reader.getVMD().morphKeyframes.size());
    EXPECT_EQ(30u,  reader.getVMD().boneKeyframes[1].frameNo);
    EXPECT_EQ(60u,  reader.getVMD().boneKeyframes[2].frameNo);
    EXPECT_FLOAT_EQ(1.f, reader.getVMD().boneKeyframes[1].position[0]);
    EXPECT_FLOAT_EQ(0.5f, reader.getVMD().morphKeyframes[0].weight);
}

TEST(VMDFileReaderTest, InvalidMagicFails)
{
    std::string bad(50, '\0');
    bad[0] = 'B'; bad[1] = 'A'; bad[2] = 'D';
    std::istringstream ss(bad);
    VMDFileReader reader;
    EXPECT_FALSE(reader.read(ss));
}

TEST(VMDFileReaderTest, EmptyStreamFails)
{
    std::istringstream ss("");
    VMDFileReader reader;
    EXPECT_FALSE(reader.read(ss));
}
