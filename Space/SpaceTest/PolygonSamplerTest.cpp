#include "pch.h"
#include "pch.h"

#include "../Space/PolygonSampler.h"

using namespace Phantom::Math;
using namespace Phantom::Space;

namespace {
std::vector<Vector3df> createCubeVertices(float size)
{
    return {
        {0.0f, 0.0f, 0.0f},
        {size, 0.0f, 0.0f},
        {size, size, 0.0f},
        {0.0f, size, 0.0f},
        {0.0f, 0.0f, size},
        {size, 0.0f, size},
        {size, size, size},
        {0.0f, size, size}
    };
}

std::vector<Face> createCubeFaces()
{
    return {
        {0, 1, 2}, {0, 2, 3},
        {4, 7, 6}, {4, 6, 5},
        {0, 4, 5}, {0, 5, 1},
        {3, 2, 6}, {3, 6, 7},
        {0, 3, 7}, {0, 7, 4},
        {1, 5, 6}, {1, 6, 2}
    };
}
}

TEST(PolygonSamplerTest, Sample_InsideMesh)
{
    const auto cubeVertices = createCubeVertices(10.0f);
    const auto cubeFaces = createCubeFaces();
    PolygonSampler sampler(cubeVertices, cubeFaces);

    const auto generated = sampler.generateUniformGrid(2.0);
    ASSERT_FALSE(generated.empty());

    for (const auto& p : generated) {
        EXPECT_GE(p.x, 0.0f);
        EXPECT_LE(p.x, 10.0f);
        EXPECT_GE(p.y, 0.0f);
        EXPECT_LE(p.y, 10.0f);
        EXPECT_GE(p.z, 0.0f);
        EXPECT_LE(p.z, 10.0f);
    }
}

TEST(PolygonSamplerTest, Sample_SingleTriangle)
{
    const std::vector<Vector3df> vertices = {
        {0.0f, 0.0f, 0.0f},
        {2.0f, 0.0f, 0.0f},
        {0.0f, 2.0f, 0.0f}
    };
    const std::vector<Face> faces = {
        {0, 1, 2}
    };

    PolygonSampler sampler(vertices, faces);
    const auto generated = sampler.generateUniformGrid(1.0);

    for (const auto& p : generated) {
        EXPECT_GE(p.x, 0.0f);
        EXPECT_LE(p.x, 2.0f);
        EXPECT_GE(p.y, 0.0f);
        EXPECT_LE(p.y, 2.0f);
        EXPECT_NEAR(p.z, 0.0f, 1.0e-6f);
    }
}

TEST(PolygonSamplerTest, Sample_ZeroVolumeMesh)
{
    const std::vector<Vector3df> vertices = {
        {1.0f, 1.0f, 1.0f},
        {1.0f, 1.0f, 1.0f},
        {1.0f, 1.0f, 1.0f}
    };
    const std::vector<Face> faces = {
        {0, 1, 2}
    };

    PolygonSampler sampler(vertices, faces);
    const auto generated = sampler.generateUniformGrid(1.0);
    EXPECT_TRUE(generated.empty());
}
