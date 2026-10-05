#include "gtest/gtest.h"

#include <cmath>
#include <limits>

#include "../GeometryNode/GeometryOps.h"

using namespace Phantom::GeometryNode;

namespace {

Vec3 sub(const Vec3& a, const Vec3& b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
Vec3 cross(const Vec3& a, const Vec3& b) { return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x}; }
float dot(const Vec3& a, const Vec3& b) { return a.x * b.x + a.y * b.y + a.z * b.z; }

// Every triangle's geometric normal must agree with the declared vertex normals.
bool windingMatchesNormals(const Mesh& m) {
    for (size_t t = 0; t + 2 < m.indices.size(); t += 3) {
        const Vec3& a = m.positions[m.indices[t]];
        const Vec3& b = m.positions[m.indices[t + 1]];
        const Vec3& c = m.positions[m.indices[t + 2]];
        const Vec3 n = cross(sub(b, a), sub(c, a));
        if (dot(n, m.normals[m.indices[t]]) <= 0.0f) return false;
    }
    return true;
}

GeometryPtr box(Vec3 size = {1, 1, 1}) {
    Mesh m;
    EXPECT_EQ(makeBox(size, Limits{}, m), OpStatus::Ok);
    return std::make_shared<const Mesh>(std::move(m));
}

}  // namespace

TEST(GeometryOps, BoxTopologyBoundsAndWinding) {
    Mesh m;
    ASSERT_EQ(makeBox({2, 4, 6}, Limits{}, m), OpStatus::Ok);
    EXPECT_EQ(m.vertexCount(), 24u);
    EXPECT_EQ(m.indices.size(), 36u);
    EXPECT_EQ(validateMesh(m), OpStatus::Ok);
    const Bounds b = computeBounds(m);
    ASSERT_TRUE(b.valid);
    EXPECT_EQ(b.min, (Vec3{-1, -2, -3}));
    EXPECT_EQ(b.max, (Vec3{1, 2, 3}));
    EXPECT_TRUE(windingMatchesNormals(m));
}

TEST(GeometryOps, BoxRejectsBadSize) {
    Mesh m;
    EXPECT_EQ(makeBox({-1, 1, 1}, Limits{}, m), OpStatus::InvalidArgument);
    EXPECT_EQ(makeBox({std::numeric_limits<float>::quiet_NaN(), 1, 1}, Limits{}, m), OpStatus::InvalidArgument);
    EXPECT_TRUE(m.positions.empty());  // untouched on failure
}

TEST(GeometryOps, GridTopologyAndUpFacingWinding) {
    Mesh m;
    ASSERT_EQ(makeGrid(4, 2, 5, 3, Limits{}, m), OpStatus::Ok);
    EXPECT_EQ(m.vertexCount(), 15u);
    EXPECT_EQ(m.triangleCount(), 16u);
    EXPECT_EQ(validateMesh(m), OpStatus::Ok);
    const Bounds b = computeBounds(m);
    EXPECT_EQ(b.min, (Vec3{-2, 0, -1}));
    EXPECT_EQ(b.max, (Vec3{2, 0, 1}));
    EXPECT_TRUE(windingMatchesNormals(m));
}

TEST(GeometryOps, GridRejectsTooFewVerticesAndHugeGrids) {
    Mesh m;
    EXPECT_EQ(makeGrid(1, 1, 1, 3, Limits{}, m), OpStatus::InvalidArgument);
    // 2^31 - 1 squared would overflow 32-bit math; it must be refused before any allocation.
    EXPECT_EQ(makeGrid(1, 1, std::numeric_limits<int32_t>::max(), std::numeric_limits<int32_t>::max(), Limits{}, m),
              OpStatus::LimitExceeded);
    Limits tight;
    tight.maxVertices = 8;
    EXPECT_EQ(makeGrid(1, 1, 3, 3, tight, m), OpStatus::LimitExceeded);
}

TEST(GeometryOps, TransformTranslateScaleRotate) {
    const GeometryPtr src = box({2, 2, 2});
    Mesh m;
    ASSERT_EQ(transformMesh(*src, {10, 0, 0}, {0, 0, 0}, {2, 1, 1}, m), OpStatus::Ok);
    Bounds b = computeBounds(m);
    EXPECT_EQ(b.min, (Vec3{8, -1, -1}));
    EXPECT_EQ(b.max, (Vec3{12, 1, 1}));

    // 90 degrees about Z maps +X to +Y.
    Mesh r;
    ASSERT_EQ(transformMesh(*src, {0, 0, 0}, {0, 0, 90}, {3, 1, 1}, r), OpStatus::Ok);
    b = computeBounds(r);
    EXPECT_NEAR(b.max.x, 1.0f, 1e-5f);
    EXPECT_NEAR(b.max.y, 3.0f, 1e-5f);
    EXPECT_TRUE(windingMatchesNormals(r));
}

TEST(GeometryOps, NonUniformScaleKeepsNormalsPerpendicular) {
    const GeometryPtr src = box({2, 2, 2});
    Mesh m;
    ASSERT_EQ(transformMesh(*src, {0, 0, 0}, {20, 35, 50}, {1, 5, 0.25f}, m), OpStatus::Ok);
    for (const Vec3& n : m.normals) EXPECT_NEAR(dot(n, n), 1.0f, 1e-5f);
    // A face normal must stay perpendicular to its triangle edges.
    for (size_t t = 0; t + 2 < m.indices.size(); t += 3) {
        const Vec3& a = m.positions[m.indices[t]];
        const Vec3 e1 = sub(m.positions[m.indices[t + 1]], a);
        const Vec3 e2 = sub(m.positions[m.indices[t + 2]], a);
        const Vec3& n = m.normals[m.indices[t]];
        EXPECT_NEAR(dot(n, e1), 0.0f, 1e-4f);
        EXPECT_NEAR(dot(n, e2), 0.0f, 1e-4f);
    }
    EXPECT_TRUE(windingMatchesNormals(m));
}

TEST(GeometryOps, NegativeScaleFlipsWindingSoFacesStillPointOutward) {
    const GeometryPtr src = box({2, 2, 2});
    Mesh m;
    ASSERT_EQ(transformMesh(*src, {0, 0, 0}, {0, 0, 0}, {-1, 1, 1}, m), OpStatus::Ok);
    EXPECT_TRUE(windingMatchesNormals(m));
    // The +X face (normal +X) is now the -X face: its normal must have become -X.
    bool sawMinusX = false;
    for (const Vec3& n : m.normals) sawMinusX = sawMinusX || (n.x < -0.99f);
    EXPECT_TRUE(sawMinusX);
    // And every normal must point away from the centre.
    for (size_t i = 0; i < m.positions.size(); ++i) EXPECT_GT(dot(m.normals[i], m.positions[i]), 0.0f);
}

TEST(GeometryOps, SingularTransformIsRejected) {
    const GeometryPtr src = box();
    Mesh m;
    EXPECT_EQ(transformMesh(*src, {0, 0, 0}, {0, 0, 0}, {1, 0, 1}, m), OpStatus::SingularTransform);
    EXPECT_EQ(transformMesh(*src, {std::numeric_limits<float>::infinity(), 0, 0}, {0, 0, 0}, {1, 1, 1}, m),
              OpStatus::InvalidArgument);
}

TEST(GeometryOps, TransformOverflowToInfinityIsRejected) {
    const GeometryPtr src = box({1e30f, 1, 1});
    Mesh m;
    EXPECT_EQ(transformMesh(*src, {0, 0, 0}, {0, 0, 0}, {1e30f, 1, 1}, m), OpStatus::InvalidMesh);
}

TEST(GeometryOps, JoinOffsetsIndicesAndFillsMissingAttributes) {
    Mesh tri;  // no normals, no uvs
    tri.positions = {{0, 0, 0}, {0, 0, 1}, {1, 0, 0}};
    tri.indices = {0, 1, 2};
    const GeometryPtr a = std::make_shared<const Mesh>(tri);
    const GeometryPtr b = box();

    Mesh out;
    ASSERT_EQ(joinMeshes({a, b}, Limits{}, out), OpStatus::Ok);
    EXPECT_EQ(out.vertexCount(), 27u);
    EXPECT_EQ(out.indices.size(), 39u);
    EXPECT_EQ(out.indices[3], 3u);  // first box index is offset by the triangle's 3 vertices
    ASSERT_EQ(out.normals.size(), 27u);
    ASSERT_EQ(out.uvs.size(), 27u);
    EXPECT_NEAR(out.normals[0].y, 1.0f, 1e-6f);  // computed: the triangle faces +Y
    EXPECT_EQ(out.uvs[0], (Vec2{0, 0}));         // missing UVs -> (0,0)
    EXPECT_EQ(validateMesh(out), OpStatus::Ok);
}

TEST(GeometryOps, JoinOfNothingIsEmptyAndLimitsApply) {
    Mesh out;
    ASSERT_EQ(joinMeshes({}, Limits{}, out), OpStatus::Ok);
    EXPECT_EQ(out.vertexCount(), 0u);
    EXPECT_FALSE(computeBounds(out).valid);

    Limits tight;
    tight.maxVertices = 30;
    const GeometryPtr b = box();
    EXPECT_EQ(joinMeshes({b, b}, tight, out), OpStatus::LimitExceeded);
}

TEST(GeometryOps, ValidateMeshCatchesBadIndexAndAttributes) {
    Mesh m;
    m.positions = {{0, 0, 0}, {1, 0, 0}, {0, 1, 0}};
    m.indices = {0, 1, 3};
    EXPECT_EQ(validateMesh(m), OpStatus::InvalidMesh);
    m.indices = {0, 1};
    EXPECT_EQ(validateMesh(m), OpStatus::InvalidMesh);
    m.indices = {0, 1, 2};
    m.normals = {{0, 0, 1}};
    EXPECT_EQ(validateMesh(m), OpStatus::InvalidMesh);
    m.normals.clear();
    m.positions[1].x = std::numeric_limits<float>::quiet_NaN();
    EXPECT_EQ(validateMesh(m), OpStatus::InvalidMesh);
}
