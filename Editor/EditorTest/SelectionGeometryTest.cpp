#include <gtest/gtest.h>
#include "../Selection/SelectionGeometry.h"
#include <array>
#include <vector>

using namespace Phantom::Editor;
using Phantom::Math::Matrix4df;
using Phantom::Math::Vector3df;

namespace {
const std::array<uint32_t, 3> triangle = {0, 1, 2};
const std::array<Vector3df, 3> positions = {
    Vector3df(-0.8f, -0.8f, 0.5f), Vector3df(0.8f, -0.8f, 0.5f), Vector3df(0.f, 0.8f, 0.5f)};
const SelectionRectangle window = {100.f, 100.f, 5.f, 5.f, 95.f, 95.f};
const SelectionRectangle crossing = {100.f, 100.f, 55.f, 45.f, 45.f, 55.f};
}

TEST(SelectionGeometryTest, DirectionDistinguishesContainmentAndTriangleIntersection) {
    EXPECT_TRUE(meshSelectedInRectangle(positions, triangle, Matrix4df(1.f), window));
    EXPECT_FALSE(meshSelectedInRectangle(positions, triangle, Matrix4df(1.f),
        {100.f, 100.f, 45.f, 45.f, 55.f, 55.f}));
    EXPECT_TRUE(meshSelectedInRectangle(positions, triangle, Matrix4df(1.f), crossing));
    EXPECT_FALSE(meshSelectedInRectangle(positions, triangle, Matrix4df(1.f),
        {100.f, 100.f, 99.f, 98.f, 98.f, 99.f}));
}

TEST(SelectionGeometryTest, ClipsNearFarAndSidePlanesBeforeCrossingTest) {
    auto mesh = positions;
    mesh[0].z = -0.5f;
    EXPECT_FALSE(meshSelectedInRectangle(mesh, triangle, Matrix4df(1.f), window));
    EXPECT_TRUE(meshSelectedInRectangle(mesh, triangle, Matrix4df(1.f), crossing));
    mesh = positions;
    mesh[0].z = 1.5f;
    EXPECT_FALSE(meshSelectedInRectangle(mesh, triangle, Matrix4df(1.f), window));
    EXPECT_TRUE(meshSelectedInRectangle(mesh, triangle, Matrix4df(1.f), crossing));
    mesh = positions;
    mesh[0].x = -2.f;
    EXPECT_FALSE(meshSelectedInRectangle(mesh, triangle, Matrix4df(1.f), window));
    EXPECT_TRUE(meshSelectedInRectangle(mesh, triangle, Matrix4df(1.f), crossing));
    for (float z : {-0.1f, 1.1f}) {
        mesh = positions;
        for (auto& p : mesh) p.z = z;
        EXPECT_FALSE(meshSelectedInRectangle(mesh, triangle, Matrix4df(1.f), crossing));
    }
}

TEST(SelectionGeometryTest, AppliesMvpAndPreservesScreenYConvention) {
    const std::array<Vector3df, 3> mesh = {
        Vector3df(-0.1f, -0.1f, 0.5f), Vector3df(0.1f, -0.1f, 0.5f), Vector3df(0.f, 0.1f, 0.5f)};
    Matrix4df transform(1.f);
    transform[3][0] = 0.4f;
    transform[3][1] = -0.4f;
    const SelectionRectangle atScreen70_30 = {100.f, 100.f, 64.f, 24.f, 76.f, 36.f};
    EXPECT_TRUE(meshSelectedInRectangle(mesh, triangle, transform, atScreen70_30));
    EXPECT_FALSE(meshSelectedInRectangle(mesh, triangle, Matrix4df(1.f), atScreen70_30));
    transform[1][1] = -1.f;
    transform[3][1] = 0.4f;
    EXPECT_TRUE(meshSelectedInRectangle(mesh, triangle, transform,
        {100.f, 100.f, 64.f, 64.f, 76.f, 76.f}));
}

TEST(SelectionGeometryTest, DividesByPositiveWAndRejectsBehindCamera) {
    Matrix4df transform(1.f);
    transform[3][3] = 2.f;
    EXPECT_TRUE(meshSelectedInRectangle(positions, triangle, transform,
        {100.f, 100.f, 29.f, 29.f, 71.f, 71.f}));
    transform[3][3] = -1.f;
    EXPECT_FALSE(meshSelectedInRectangle(positions, triangle, transform, window));
    EXPECT_FALSE(meshSelectedInRectangle(positions, triangle, transform, crossing));
}

TEST(SelectionGeometryTest, BoundaryTouchIsIncluded) {
    const std::array<Vector3df, 3> mesh = {
        Vector3df(0.f, 0.f, 0.f), Vector3df(0.2f, 0.f, 1.f), Vector3df(0.f, 0.2f, 0.5f)};
    EXPECT_TRUE(meshSelectedInRectangle(mesh, triangle, Matrix4df(1.f),
        {100.f, 100.f, 50.f, 50.f, 61.f, 61.f}));
    EXPECT_TRUE(meshSelectedInRectangle(mesh, triangle, Matrix4df(1.f),
        {100.f, 100.f, 50.f, 40.f, 40.f, 50.f}));
}

TEST(SelectionGeometryTest, EmptyDegenerateAndInvalidTrianglesDoNotCross) {
    EXPECT_FALSE(meshSelectedInRectangle({}, triangle, Matrix4df(1.f), crossing));
    EXPECT_FALSE(meshSelectedInRectangle(positions, {}, Matrix4df(1.f), crossing));
    for (auto rect : {SelectionRectangle{0.f, 100.f, 55.f, 45.f, 45.f, 55.f},
        SelectionRectangle{100.f, -1.f, 55.f, 45.f, 45.f, 55.f},
        SelectionRectangle{100.f, 100.f, 45.f, 45.f, 45.f, 55.f},
        SelectionRectangle{100.f, 100.f, 55.f, 45.f, 45.f, 45.f}})
        EXPECT_FALSE(meshSelectedInRectangle(positions, triangle, Matrix4df(1.f), rect));
    const std::array<uint32_t, 5> invalid = {0, 1, 99, 0, 1};
    EXPECT_FALSE(meshSelectedInRectangle(positions, invalid, Matrix4df(1.f), crossing));
    const std::array<uint32_t, 6> mixed = {0, 1, 99, 0, 1, 2};
    EXPECT_TRUE(meshSelectedInRectangle(positions, mixed, Matrix4df(1.f), crossing));
}
