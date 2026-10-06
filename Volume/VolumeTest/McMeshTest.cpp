#include <gtest/gtest.h>

#include "McMesh.h"

using namespace VolumeView;
using Phantom::Math::Triangle3df;
using Phantom::Math::Vector3df;

TEST(McMeshTest, EmptyInputGivesEmptyMesh)
{
    const auto mesh = trianglesToPolygonMesh("m", {}, {1.f, 1.f, 1.f, 1.f});
    EXPECT_EQ(mesh.name, "m");
    EXPECT_TRUE(mesh.positions.empty());
    EXPECT_TRUE(mesh.colors.empty());
    EXPECT_TRUE(mesh.indices.empty());
}

TEST(McMeshTest, TrianglesBecomeUnsharedVerticesWithSequentialIndices)
{
    const std::vector<Triangle3df> tris = {
        Triangle3df({Vector3df(0, 0, 0), Vector3df(1, 0, 0), Vector3df(0, 1, 0)}),
        Triangle3df({Vector3df(0, 0, 1), Vector3df(1, 0, 1), Vector3df(0, 1, 1)}),
    };
    const auto mesh = trianglesToPolygonMesh("t", tris, {0.1f, 0.2f, 0.3f, 0.4f});

    ASSERT_EQ(mesh.positions.size(), 18u);
    ASSERT_EQ(mesh.colors.size(), 24u);
    ASSERT_EQ(mesh.indices.size(), 6u);
    for (uint32_t i = 0; i < 6; ++i) EXPECT_EQ(mesh.indices[i], i);
    EXPECT_FLOAT_EQ(mesh.positions[3], 1.f);   // vertex 1 x
    EXPECT_FLOAT_EQ(mesh.positions[17], 1.f);  // vertex 5 z
    for (size_t v = 0; v < 6; ++v) {
        EXPECT_FLOAT_EQ(mesh.colors[v * 4 + 0], 0.1f);
        EXPECT_FLOAT_EQ(mesh.colors[v * 4 + 3], 0.4f);
    }
}
