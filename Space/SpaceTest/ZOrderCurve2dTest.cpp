#include "gtest/gtest.h"

#include "../Space/ZOrderCurve2d.h"

using namespace Phantom::Space;

TEST(ZOrderCurve2dTest, EncodeOrigin)
{
    ZOrderCurve2d curve;
    EXPECT_EQ(0u, curve.encode({ 0u, 0u }));
}

TEST(ZOrderCurve2dTest, EncodeAxisX)
{
    ZOrderCurve2d curve;
    EXPECT_EQ(1u, curve.encode({ 1u, 0u }));
    EXPECT_EQ(4u, curve.encode({ 2u, 0u }));
}

TEST(ZOrderCurve2dTest, EncodeAxisY)
{
    ZOrderCurve2d curve;
    EXPECT_EQ(2u, curve.encode({ 0u, 1u }));
    EXPECT_EQ(8u, curve.encode({ 0u, 2u }));
}

TEST(ZOrderCurve2dTest, EncodeDiagonal)
{
    ZOrderCurve2d curve;
    EXPECT_EQ(3u, curve.encode({ 1u, 1u }));
    EXPECT_EQ(12u, curve.encode({ 2u, 2u }));
}

TEST(ZOrderCurve2dTest, DecodeOrigin)
{
    ZOrderCurve2d curve;
    const auto actual = curve.decode(0u);
    EXPECT_EQ(0u, actual[0]);
    EXPECT_EQ(0u, actual[1]);
}

TEST(ZOrderCurve2dTest, DecodeAxisX)
{
    ZOrderCurve2d curve;
    const auto actual = curve.decode(1u);
    EXPECT_EQ(1u, actual[0]);
    EXPECT_EQ(0u, actual[1]);
}

TEST(ZOrderCurve2dTest, DecodeAxisY)
{
    ZOrderCurve2d curve;
    const auto actual = curve.decode(2u);
    EXPECT_EQ(0u, actual[0]);
    EXPECT_EQ(1u, actual[1]);
}

TEST(ZOrderCurve2dTest, EncodeDecodeRoundtrip)
{
    ZOrderCurve2d curve;
    const std::array<unsigned int, 2> index = { 3u, 5u };
    const auto encoded = curve.encode(index);
    const auto decoded = curve.decode(encoded);
    EXPECT_EQ(index[0], decoded[0]);
    EXPECT_EQ(index[1], decoded[1]);
}
