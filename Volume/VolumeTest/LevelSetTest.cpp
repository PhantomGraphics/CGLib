#include "pch.h"

#include "../Volume/LevelSet.h"
#include "../Volume/SparseVolumeTree/SparseVolume.h"

#include "../../../CGLib/Math/Box3d.h"
#include "../../../CGLib/Math/Triangle3d.h"

#include <array>
#include <cmath>

using namespace Phantom::Math;
using namespace Phantom::Volume;

TEST(LevelSetTest, CreatesNodesAndSetsValues)
{
	const Triangle3df tri({ Vector3df(0.0f, 0.0f, 0.0f), Vector3df(1.0f, 0.0f, 0.0f), Vector3df(0.0f, 1.0f, 0.0f) });
	const std::vector<Triangle3df> triangles = { tri };

	SparseVolumef volume(0.0f);
	volume.setVoxelSize(0.25f);

	LevelSet ls;
	ls.setSignedDistance(triangles, volume);

	EXPECT_GT(volume.getActiveVoxelCount(), 0) << "�m�[�h����������Ă��܂���B";

	const auto idx = volume.worldToIndex(Vector3df(0.2f, 0.2f, 0.0f));
	EXPECT_NEAR(volume.getValue(idx), 0.0f, 1.0e-4f) << "Surface point should have SDF near 0.";
}

TEST(LevelSetTest, BoxSdfInsideNegativeOutsidePositive)
{
    SparseVolumef vol(1e6f);
    vol.setVoxelSize(1.0f);
    LevelSet ls;
    // Box [-5,5]^3 with thickness 3
    ls.setSignedDistance(Box3df(Vector3df(-5,-5,-5), Vector3df(5,5,5)), vol, 3.0);
    ASSERT_GT(vol.getActiveVoxelCount(), 0);

    // Point 1 inside the box, 1 voxel from surface: SDF should be -1
    const Coord insideIdx = vol.worldToIndex(Vector3df(-4.f, 0.f, 0.f));
    EXPECT_LT(vol.getValue(insideIdx), 0.0f) << "Inside box: SDF should be negative";
    EXPECT_NEAR(vol.getValue(insideIdx), -1.0f, 0.6f);

    // Point 1 outside the box, 1 voxel from surface: SDF should be +1
    const Coord outsideIdx = vol.worldToIndex(Vector3df(6.f, 0.f, 0.f));
    EXPECT_GT(vol.getValue(outsideIdx), 0.0f) << "Outside box: SDF should be positive";
    EXPECT_NEAR(vol.getValue(outsideIdx), 1.0f, 0.6f);

    // Point on the surface: SDF should be near 0
    const Coord surfaceIdx = vol.worldToIndex(Vector3df(5.f, 0.f, 0.f));
    EXPECT_NEAR(vol.getValue(surfaceIdx), 0.0f, 1.1f);
}

TEST(LevelSetTest, BoxSdfNarrowBandDoesNotContainDeepInterior)
{
    SparseVolumef vol(1e6f);
    vol.setVoxelSize(1.0f);
    LevelSet ls;
    ls.setSignedDistance(Box3df(Vector3df(-5,-5,-5), Vector3df(5,5,5)), vol, 3.0);

    // Deep interior (distance > 3 from surface): should NOT be set → background
    const Coord deep = vol.worldToIndex(Vector3df(0.f, 0.f, 0.f));
    EXPECT_FLOAT_EQ(vol.getValue(deep), vol.getBackground())
        << "Deep interior voxel should remain background in narrow-band SDF";
}

TEST(LevelSetTest, TriangleMeshSdfNearZeroOnSurface)
{
    // XZ-plane triangle; points on the plane should have SDF near 0
    const Triangle3df tri({
        Vector3df(0.0f, 0.0f, 0.0f),
        Vector3df(2.0f, 0.0f, 0.0f),
        Vector3df(0.0f, 0.0f, 2.0f)
    });

    SparseVolumef vol(1e6f);
    vol.setVoxelSize(0.5f);
    LevelSet ls;
    const std::vector<Triangle3df> tris1 = { tri };
    ls.setSignedDistance(tris1, vol);
    ASSERT_GT(vol.getActiveVoxelCount(), 0);

    // Centroid of the triangle in world space should have SDF close to 0
    const Vector3df centroid(2.0f/3.0f, 0.0f, 2.0f/3.0f);
    const Coord cIdx = vol.worldToIndex(centroid);
    EXPECT_NEAR(vol.getValue(cIdx), 0.0f, 0.5f)
        << "Point on the triangle surface should have SDF near 0";
}

// Regression test for a bug where every triangle-mesh SDF value collapsed to
// exactly 0 (see LevelSet.cpp's fix comment): setSignedDistance() used to pass
// the running best-so-far distance as DistanceCalculator::calculate()'s
// snap-to-zero *tolerance* argument, so the first triangle (tolerance=infinity)
// always satisfied "dist <= tolerance" and returned 0, permanently corrupting
// the result. The existing tests above never caught this because they only
// assert points *on* the surface are near 0 -- which a stuck-at-0 bug also
// satisfies trivially. This test checks points strictly inside/outside a
// closed mesh get the correct nonzero magnitude and sign.
TEST(LevelSetTest, ClosedBoxMesh_InteriorAndExteriorPointsHaveCorrectSignAndMagnitude)
{
    // Unit box, half-extent 0.5, centered at the origin, as 12 triangles.
    const Vector3df h(0.5f, 0.5f, 0.5f);
    const Vector3df c[8] = {
        { -h.x, -h.y, -h.z }, {  h.x, -h.y, -h.z }, {  h.x,  h.y, -h.z }, { -h.x,  h.y, -h.z },
        { -h.x, -h.y,  h.z }, {  h.x, -h.y,  h.z }, {  h.x,  h.y,  h.z }, { -h.x,  h.y,  h.z },
    };
    auto tri = [](const Vector3df& a, const Vector3df& b, const Vector3df& cc) {
        return Triangle3df(std::array<Vector3df, 3>{ a, b, cc });
    };
    const std::vector<Triangle3df> triangles = {
        tri(c[0], c[1], c[2]), tri(c[0], c[2], c[3]),
        tri(c[4], c[6], c[5]), tri(c[4], c[7], c[6]),
        tri(c[0], c[5], c[1]), tri(c[0], c[4], c[5]),
        tri(c[3], c[2], c[6]), tri(c[3], c[6], c[7]),
        tri(c[0], c[3], c[7]), tri(c[0], c[7], c[4]),
        tri(c[1], c[5], c[6]), tri(c[1], c[6], c[2]),
    };

    SparseVolumef vol(1e6f);
    vol.setVoxelSize(0.1f);
    LevelSet ls;
    ls.setSignedDistance(triangles, vol);

    // Center: 0.5 inside every face -> -0.5, not 0.
    const float center = vol.getValue(vol.worldToIndex(Vector3df(0.f, 0.f, 0.f)));
    EXPECT_NEAR(center, -0.5f, 1.0e-4f);

    // Off-axis interior point, 0.2 from the nearest (+X) face.
    const float interior = vol.getValue(vol.worldToIndex(Vector3df(0.3f, 0.2f, -0.1f)));
    EXPECT_NEAR(interior, -0.2f, 1.0e-4f);

    // Just outside the +X face, still within the padded active band.
    const float exterior = vol.getValue(vol.worldToIndex(Vector3df(0.6f, 0.f, 0.f)));
    EXPECT_NEAR(exterior, 0.1f, 1.0e-4f);
}

TEST(LevelSetTest, EmptyTrianglesCreatesNoVoxels)
{
    SparseVolumef vol(0.0f);
    vol.setVoxelSize(0.25f);
    LevelSet ls;
    ls.setSignedDistance(std::vector<Triangle3df>{}, vol);
    EXPECT_EQ(vol.getActiveVoxelCount(), 0);
}

TEST(LevelSetTest, BoxSdfZeroThicknessCreatesNoVoxels)
{
    SparseVolumef vol(1e6f);
    vol.setVoxelSize(1.0f);
    LevelSet ls;
    ls.setSignedDistance(Box3df(Vector3df(-5,-5,-5), Vector3df(5,5,5)), vol, 0.0);
    EXPECT_EQ(vol.getActiveVoxelCount(), 0);
}

TEST(LevelSetTest, TriangleMeshBothSidesHaveSameDistance)
{
    // Flat triangle on the XZ-plane at y=0
    const Triangle3df tri({
        Vector3df(-1.0f, 0.0f, -1.0f),
        Vector3df( 1.0f, 0.0f, -1.0f),
        Vector3df( 0.0f, 0.0f,  1.0f)
    });

    SparseVolumef vol(1e6f);
    vol.setVoxelSize(0.25f);
    LevelSet ls;
    const std::vector<Triangle3df> tris2 = { tri };
    ls.setSignedDistance(tris2, vol);

    // A point directly above the centroid
    const Vector3df above(0.0f,  0.25f, 0.0f);
    const Vector3df below(0.0f, -0.25f, 0.0f);

    const Coord aIdx = vol.worldToIndex(above);
    const Coord bIdx = vol.worldToIndex(below);

    const float distAbove = std::fabs(vol.getValue(aIdx));
    const float distBelow = std::fabs(vol.getValue(bIdx));

    // Both should be roughly equidistant from the surface
    EXPECT_NEAR(distAbove, distBelow, 0.1f)
        << "Both sides of a flat mesh should have equal unsigned distance";
}
