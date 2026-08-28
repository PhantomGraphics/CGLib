#include "gtest/gtest.h"

#include <sstream>
#include "../File/BVHFileReader.h"

using namespace Phantom::File;

namespace {

std::stringstream getSampleBVH()
{
    std::stringstream stream;
    stream
        << "HIERARCHY" << std::endl
        << "ROOT Hips" << std::endl
        << "{" << std::endl
        << "	OFFSET 0.0 0.0 0.0" << std::endl
        << "	CHANNELS 6 Xposition Yposition Zposition Zrotation Xrotation Yrotation" << std::endl
        << "	JOINT Spine" << std::endl
        << "	{" << std::endl
        << "		OFFSET 0.0 5.0 0.0" << std::endl
        << "		CHANNELS 3 Zrotation Xrotation Yrotation" << std::endl
        << "		End Site" << std::endl
        << "		{" << std::endl
        << "			OFFSET 0.0 3.0 0.0" << std::endl
        << "		}" << std::endl
        << "	}" << std::endl
        << "}" << std::endl
        << "MOTION" << std::endl
        << "Frames: 2" << std::endl
        << "Frame Time: 0.0333333" << std::endl
        << "0.0 0.0 0.0 0.0 0.0 0.0 0.0 0.0 0.0" << std::endl
        << "1.0 2.0 3.0 10.0 20.0 30.0 5.0 6.0 7.0" << std::endl;
    return stream;
}

} // namespace

TEST(BVHFileReaderTest, ReadNonexistentFile)
{
    BVHFileReader reader;
    EXPECT_FALSE(reader.read("__nonexistent_file__.bvh"));
}

TEST(BVHFileReaderTest, ReadEmptyStream)
{
    std::stringstream ss;
    BVHFileReader reader;
    EXPECT_FALSE(reader.read(ss));
}

TEST(BVHFileReaderTest, ReadInvalidKeyword)
{
    std::stringstream ss;
    ss << "NOT_HIERARCHY" << std::endl;
    BVHFileReader reader;
    EXPECT_FALSE(reader.read(ss));
}

TEST(BVHFileReaderTest, ReadHierarchy)
{
    auto ss = getSampleBVH();
    BVHFileReader reader;
    ASSERT_TRUE(reader.read(ss));

    const auto& bvh = reader.getBVH();
    ASSERT_EQ(3u, bvh.joints.size());

    const auto& hips = bvh.joints[0];
    EXPECT_EQ("Hips", hips.name);
    EXPECT_EQ(-1, hips.parentIndex);
    EXPECT_EQ(6u, hips.channelNames.size());
    ASSERT_EQ(1u, hips.childIndices.size());
    EXPECT_EQ(1, hips.childIndices[0]);

    const auto& spine = bvh.joints[1];
    EXPECT_EQ("Spine", spine.name);
    EXPECT_EQ(0, spine.parentIndex);
    EXPECT_EQ(3u, spine.channelNames.size());
    EXPECT_DOUBLE_EQ(5.0, spine.offset.y);
    ASSERT_EQ(1u, spine.childIndices.size());
    EXPECT_EQ(2, spine.childIndices[0]);

    const auto& endSite = bvh.joints[2];
    EXPECT_TRUE(endSite.isEndSite);
    EXPECT_EQ(1, endSite.parentIndex);
    EXPECT_TRUE(endSite.channelNames.empty());
    EXPECT_DOUBLE_EQ(3.0, endSite.offset.y);
}

TEST(BVHFileReaderTest, ReadMotion)
{
    auto ss = getSampleBVH();
    BVHFileReader reader;
    ASSERT_TRUE(reader.read(ss));

    const auto& bvh = reader.getBVH();
    EXPECT_EQ(2, bvh.numFrames);
    EXPECT_DOUBLE_EQ(0.0333333, bvh.frameTime);
    ASSERT_EQ(2u, bvh.motion.size());
    ASSERT_EQ(9u, bvh.motion[0].size()); // Hips: 6 channels, Spine: 3 channels
    EXPECT_DOUBLE_EQ(1.0, bvh.motion[1][0]);
    EXPECT_DOUBLE_EQ(7.0, bvh.motion[1][8]);
}

TEST(BVHFileReaderTest, MotionChannelCountMismatchFails)
{
    std::stringstream ss;
    ss << "HIERARCHY" << std::endl
       << "ROOT Hips" << std::endl
       << "{" << std::endl
       << "	OFFSET 0.0 0.0 0.0" << std::endl
       << "	CHANNELS 3 Xposition Yposition Zposition" << std::endl
       << "}" << std::endl
       << "MOTION" << std::endl
       << "Frames: 1" << std::endl
       << "Frame Time: 0.033333" << std::endl
       << "0.0 0.0" << std::endl; // only 2 values, 3 expected
    BVHFileReader reader;
    EXPECT_FALSE(reader.read(ss));
}
