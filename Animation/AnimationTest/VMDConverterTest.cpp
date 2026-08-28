#include "gtest/gtest.h"

#include "CGLib/Animation/Animation/VMDConverter.h"
#include "CGLib/File/File/VMDFile.h"

#include <cstring>
#include <map>

using namespace Phantom::Animation;
using Phantom::File::VMDFile;
using Phantom::File::VMDBoneKeyframe;

namespace {

VMDBoneKeyframe makeKF(const char* name, uint32_t frame,
                        float px = 0.f, float py = 0.f, float pz = 0.f,
                        float qx = 0.f, float qy = 0.f, float qz = 0.f, float qw = 1.f)
{
    VMDBoneKeyframe kf{};
    std::strncpy(kf.boneName, name, 14);
    kf.frameNo      = frame;
    kf.position[0]  = px; kf.position[1] = py; kf.position[2] = pz;
    kf.rotation[0]  = qx; kf.rotation[1] = qy; kf.rotation[2] = qz; kf.rotation[3] = qw;
    return kf;
}

} // namespace

TEST(VMDConverterTest, ConvertEmpty)
{
    VMDFile vmd;
    std::map<std::string,int> nameIdx;
    nameIdx["Hips"] = 0;

    AnimationClip clip;
    VMDConverter conv;
    EXPECT_TRUE(conv.convert(vmd, nameIdx, clip));
    EXPECT_EQ(0u, clip.channels.size());
    EXPECT_FLOAT_EQ(0.f, clip.duration);
}

TEST(VMDConverterTest, ConvertSingleBoneKeyframe)
{
    VMDFile vmd;
    vmd.boneKeyframes.push_back(makeKF("Hips", 0));
    vmd.boneKeyframes.push_back(makeKF("Hips", 30));

    std::map<std::string,int> nameIdx;
    nameIdx["Hips"] = 0;

    AnimationClip clip;
    VMDConverter conv;
    EXPECT_TRUE(conv.convert(vmd, nameIdx, clip));
    ASSERT_EQ(1u, clip.channels.size());
    EXPECT_EQ(0, clip.channels[0].boneIndex);
    EXPECT_EQ(2u, clip.channels[0].positionKeys.size());
    EXPECT_FLOAT_EQ(1.0f, clip.duration); // frame 30 → 1.0s
}

TEST(VMDConverterTest, UnknownBoneSkipped)
{
    VMDFile vmd;
    vmd.boneKeyframes.push_back(makeKF("UnknownBone", 0));

    std::map<std::string,int> nameIdx;
    nameIdx["Hips"] = 0;

    AnimationClip clip;
    VMDConverter conv;
    EXPECT_TRUE(conv.convert(vmd, nameIdx, clip));
    EXPECT_EQ(0u, clip.channels.size());
}

TEST(VMDConverterTest, ZAxisFlipped)
{
    VMDFile vmd;
    vmd.boneKeyframes.push_back(makeKF("Hips", 0, 0.f, 0.f, 5.f));

    std::map<std::string,int> nameIdx;
    nameIdx["Hips"] = 0;

    AnimationClip clip;
    VMDConverter conv;
    EXPECT_TRUE(conv.convert(vmd, nameIdx, clip));
    ASSERT_EQ(1u, clip.channels.size());
    // Z=5 in MMD left-hand → Z=-5 in right-hand
    EXPECT_FLOAT_EQ(-5.f, clip.channels[0].positionKeys[0].value.z);
}

TEST(VMDConverterTest, MultipleBones)
{
    VMDFile vmd;
    vmd.boneKeyframes.push_back(makeKF("Hips",  0));
    vmd.boneKeyframes.push_back(makeKF("Spine", 0));
    vmd.boneKeyframes.push_back(makeKF("Hips",  30));

    std::map<std::string,int> nameIdx;
    nameIdx["Hips"]  = 0;
    nameIdx["Spine"] = 1;

    AnimationClip clip;
    VMDConverter conv;
    EXPECT_TRUE(conv.convert(vmd, nameIdx, clip));
    EXPECT_EQ(2u, clip.channels.size());
    EXPECT_FLOAT_EQ(1.0f, clip.duration);
}

TEST(VMDConverterTest, DurationFromMaxFrame)
{
    VMDFile vmd;
    vmd.boneKeyframes.push_back(makeKF("Hips",   0));
    vmd.boneKeyframes.push_back(makeKF("Hips",  60));
    vmd.boneKeyframes.push_back(makeKF("Hips", 150));

    std::map<std::string,int> nameIdx;
    nameIdx["Hips"] = 0;

    AnimationClip clip;
    VMDConverter conv;
    EXPECT_TRUE(conv.convert(vmd, nameIdx, clip));
    EXPECT_FLOAT_EQ(5.0f, clip.duration); // frame 150 → 5.0s
}
