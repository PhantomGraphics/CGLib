#include "gtest/gtest.h"

#include "../Space/ZOrderCurve3d.h"

using namespace Phantom::Space;

TEST(ZOrderCurve3dTest, EncodeOrigin)
{
    EXPECT_EQ(0u, ZOrderCurve3d::encode({ 0u, 0u, 0u }));
}

TEST(ZOrderCurve3dTest, EncodeAxisX)
{
    EXPECT_EQ(1u, ZOrderCurve3d::encode({ 1u, 0u, 0u }));
    EXPECT_EQ(8u, ZOrderCurve3d::encode({ 2u, 0u, 0u }));
}

TEST(ZOrderCurve3dTest, EncodeAxisY)
{
    EXPECT_EQ(2u, ZOrderCurve3d::encode({ 0u, 1u, 0u }));
    EXPECT_EQ(16u, ZOrderCurve3d::encode({ 0u, 2u, 0u }));
}

TEST(ZOrderCurve3dTest, EncodeAxisZ)
{
    EXPECT_EQ(4u, ZOrderCurve3d::encode({ 0u, 0u, 1u }));
    EXPECT_EQ(32u, ZOrderCurve3d::encode({ 0u, 0u, 2u }));
}

TEST(ZOrderCurve3dTest, EncodeAllAxes)
{
    EXPECT_EQ(7u, ZOrderCurve3d::encode({ 1u, 1u, 1u }));
}

TEST(ZOrderCurve3dTest, DecodeOrigin)
{
    const auto actual = ZOrderCurve3d::decode(0u);
    EXPECT_EQ(0u, actual[0]);
    EXPECT_EQ(0u, actual[1]);
    EXPECT_EQ(0u, actual[2]);
}

TEST(ZOrderCurve3dTest, DecodeAxisX)
{
    const auto actual = ZOrderCurve3d::decode(1u);
    EXPECT_EQ(1u, actual[0]);
    EXPECT_EQ(0u, actual[1]);
    EXPECT_EQ(0u, actual[2]);
}

TEST(ZOrderCurve3dTest, DecodeAxisY)
{
    const auto actual = ZOrderCurve3d::decode(2u);
    EXPECT_EQ(0u, actual[0]);
    EXPECT_EQ(1u, actual[1]);
    EXPECT_EQ(0u, actual[2]);
}

TEST(ZOrderCurve3dTest, DecodeAxisZ)
{
    const auto actual = ZOrderCurve3d::decode(4u);
    EXPECT_EQ(0u, actual[0]);
    EXPECT_EQ(0u, actual[1]);
    EXPECT_EQ(1u, actual[2]);
}

TEST(ZOrderCurve3dTest, EncodeDecodeRoundtrip)
{
    const std::array<unsigned int, 3> index = { 3u, 5u, 2u };
    const auto encoded = ZOrderCurve3d::encode(index);
    const auto decoded = ZOrderCurve3d::decode(encoded);
    EXPECT_EQ(index[0], decoded[0]);
    EXPECT_EQ(index[1], decoded[1]);
    EXPECT_EQ(index[2], decoded[2]);
}

TEST(ZOrderCurve3dTest, GetParentSameCell)
{
    EXPECT_EQ(0u, ZOrderCurve3d::getParent(0u, 0u));
}

TEST(ZOrderCurve3dTest, GetParentAdjacentCells)
{
    EXPECT_EQ(1u, ZOrderCurve3d::getParent(0u, 1u));
    EXPECT_EQ(1u, ZOrderCurve3d::getParent(0u, 7u));
}
