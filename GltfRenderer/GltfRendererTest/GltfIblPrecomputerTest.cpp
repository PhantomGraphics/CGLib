#include "gtest/gtest.h"

#include "../IBL/GltfIBLPrecomputer.h"

using Phantom::Gltf::GltfIBLPrecomputer;

TEST(GltfIblPrecomputerTest, PrefilterRoughnessIsLinearZeroToOne)
{
    EXPECT_FLOAT_EQ(0.0f,  GltfIBLPrecomputer::prefilterRoughness(0, 5));
    EXPECT_FLOAT_EQ(0.25f, GltfIBLPrecomputer::prefilterRoughness(1, 5));
    EXPECT_FLOAT_EQ(0.5f,  GltfIBLPrecomputer::prefilterRoughness(2, 5));
    EXPECT_FLOAT_EQ(1.0f,  GltfIBLPrecomputer::prefilterRoughness(4, 5));
}

TEST(GltfIblPrecomputerTest, SingleMipChainDoesNotDivideByZero)
{
    EXPECT_FLOAT_EQ(0.0f, GltfIBLPrecomputer::prefilterRoughness(0, 1));
    EXPECT_FLOAT_EQ(0.0f, GltfIBLPrecomputer::prefilterRoughness(0, 0));
}
