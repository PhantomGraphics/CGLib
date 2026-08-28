#include "gtest/gtest.h"

#include "CGLib/Animation/Animation/BVHConverter.h"
#include "CGLib/File/File/BVHFile.h"

using namespace Phantom::Animation;
using Phantom::File::BVHFile;
using Phantom::File::BVHJoint;

namespace {

// Hips (ROOT, 6 channels: pos + Zrotation Xrotation Yrotation)
//   -> Spine (JOINT, 3 channels: Zrotation Xrotation Yrotation)
//        -> End Site
BVHFile makeSampleBVH()
{
    BVHFile bvh;

    BVHJoint hips;
    hips.name = "Hips";
    hips.parentIndex = -1;
    hips.offset = { 0.0, 0.0, 0.0 };
    hips.channelNames = { "Xposition", "Yposition", "Zposition",
                           "Zrotation", "Xrotation", "Yrotation" };
    hips.childIndices = { 1 };
    bvh.joints.push_back(hips);

    BVHJoint spine;
    spine.name = "Spine";
    spine.parentIndex = 0;
    spine.offset = { 0.0, 5.0, 0.0 };
    spine.channelNames = { "Zrotation", "Xrotation", "Yrotation" };
    spine.childIndices = { 2 };
    bvh.joints.push_back(spine);

    BVHJoint endSite;
    endSite.name = "End Site";
    endSite.parentIndex = 1;
    endSite.offset = { 0.0, 3.0, 0.0 };
    endSite.isEndSite = true;
    bvh.joints.push_back(endSite);

    bvh.numFrames = 3;
    bvh.frameTime = 1.0 / 30.0;
    // 9 channels/frame: Hips(6) + Spine(3)
    bvh.motion = {
        { 0.0, 0.0, 0.0,  0.0, 0.0, 0.0,  0.0, 0.0, 0.0 },
        { 1.0, 2.0, 3.0, 10.0, 0.0, 0.0, 20.0, 0.0, 0.0 },
        { 2.0, 4.0, 6.0, 20.0, 0.0, 0.0, 40.0, 0.0, 0.0 },
    };

    return bvh;
}

} // namespace

TEST(BVHConverterTest, ConvertEmpty)
{
    BVHFile bvh;
    Skeleton skeleton;
    AnimationClip clip;
    BVHConverter conv;
    EXPECT_TRUE(conv.convert(bvh, skeleton, clip));
    EXPECT_EQ(0u, skeleton.bones.size());
    EXPECT_EQ(0u, clip.channels.size());
}

TEST(BVHConverterTest, SkeletonHierarchy)
{
    const auto bvh = makeSampleBVH();
    Skeleton skeleton;
    AnimationClip clip;
    BVHConverter conv;
    ASSERT_TRUE(conv.convert(bvh, skeleton, clip));

    // End Site is not a bone: only Hips and Spine become bones.
    ASSERT_EQ(2u, skeleton.bones.size());
    EXPECT_EQ("Hips", skeleton.bones[0].name);
    EXPECT_EQ(-1, skeleton.bones[0].parentIndex);
    EXPECT_EQ("Spine", skeleton.bones[1].name);
    EXPECT_EQ(0, skeleton.bones[1].parentIndex);
    EXPECT_FLOAT_EQ(5.0f, skeleton.bones[1].localPosition.y);
}

TEST(BVHConverterTest, AnimationClipChannels)
{
    const auto bvh = makeSampleBVH();
    Skeleton skeleton;
    AnimationClip clip;
    BVHConverter conv;
    ASSERT_TRUE(conv.convert(bvh, skeleton, clip));

    ASSERT_EQ(2u, clip.channels.size());

    // Hips: root, has both position and rotation channels.
    const auto& hipsChannel = clip.channels[0];
    EXPECT_EQ(0, hipsChannel.boneIndex);
    EXPECT_EQ(3u, hipsChannel.positionKeys.size());
    EXPECT_EQ(3u, hipsChannel.rotationKeys.size());
    EXPECT_FLOAT_EQ(1.0f, hipsChannel.positionKeys[1].value.x);
    EXPECT_FLOAT_EQ(3.0f, hipsChannel.positionKeys[1].value.z);

    // Spine: rotation-only.
    const auto& spineChannel = clip.channels[1];
    EXPECT_EQ(1, spineChannel.boneIndex);
    EXPECT_EQ(0u, spineChannel.positionKeys.size());
    EXPECT_EQ(3u, spineChannel.rotationKeys.size());
}

TEST(BVHConverterTest, DurationAndTicksPerSecond)
{
    const auto bvh = makeSampleBVH();
    Skeleton skeleton;
    AnimationClip clip;
    BVHConverter conv;
    ASSERT_TRUE(conv.convert(bvh, skeleton, clip));

    EXPECT_FLOAT_EQ(30.0f, clip.ticksPerSecond);
    // 3 frames -> last frame is frame index 2 -> 2 * (1/30) seconds.
    EXPECT_FLOAT_EQ(2.0f / 30.0f, clip.duration);
}
