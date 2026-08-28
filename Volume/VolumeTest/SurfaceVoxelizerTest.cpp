#include "pch.h"

#include "../Volume/SurfaceVoxelizer.h"

#include "../../../CGLib/Math/Triangle3d.h"

#include <array>
#include <cmath>

using namespace Phantom::Math;
using namespace Phantom::Volume;

namespace {

// Unit box, half-extent 0.5, centered at the origin, as 12 triangles (same
// construction as LevelSetTest.ClosedBoxMesh_InteriorAndExteriorPointsHaveCorrectSignAndMagnitude).
std::vector<Triangle3df> makeUnitBoxMesh()
{
	const Vector3df h(0.5f, 0.5f, 0.5f);
	const Vector3df c[8] = {
		{ -h.x, -h.y, -h.z }, {  h.x, -h.y, -h.z }, {  h.x,  h.y, -h.z }, { -h.x,  h.y, -h.z },
		{ -h.x, -h.y,  h.z }, {  h.x, -h.y,  h.z }, {  h.x,  h.y,  h.z }, { -h.x,  h.y,  h.z },
	};
	auto tri = [](const Vector3df& a, const Vector3df& b, const Vector3df& cc) {
		return Triangle3df(std::array<Vector3df, 3>{ a, b, cc });
	};
	return {
		tri(c[0], c[1], c[2]), tri(c[0], c[2], c[3]),
		tri(c[4], c[6], c[5]), tri(c[4], c[7], c[6]),
		tri(c[0], c[5], c[1]), tri(c[0], c[4], c[5]),
		tri(c[3], c[2], c[6]), tri(c[3], c[6], c[7]),
		tri(c[0], c[3], c[7]), tri(c[0], c[7], c[4]),
		tri(c[1], c[5], c[6]), tri(c[1], c[6], c[2]),
	};
}

} // namespace

TEST(SurfaceVoxelizerTest, EmptyTrianglesYieldsNoPoints)
{
	const auto points = SurfaceVoxelizer::voxelizeSurface({}, 0.1f, 1.0f);
	EXPECT_TRUE(points.empty());
}

TEST(SurfaceVoxelizerTest, BoxSurfaceProducesNonEmptyPointCloud)
{
	const auto points = SurfaceVoxelizer::voxelizeSurface(makeUnitBoxMesh(), 0.1f, 1.0f);
	EXPECT_GT(points.size(), 0u);
}

TEST(SurfaceVoxelizerTest, BoxSurfacePointsLieNearBoxFaces)
{
	const float voxelSize = 0.1f;
	const auto points = SurfaceVoxelizer::voxelizeSurface(makeUnitBoxMesh(), voxelSize, 1.0f);
	ASSERT_GT(points.size(), 0u);

	// Every returned point should lie close to the surface of the unit box
	// (half-extent 0.5): the max-norm distance to the box boundary should be
	// within a couple of voxels of 0.
	for (const auto& p : points) {
		const float dx = std::fabs(std::fabs(p.x) - 0.5f);
		const float dy = std::fabs(std::fabs(p.y) - 0.5f);
		const float dz = std::fabs(std::fabs(p.z) - 0.5f);
		const float distToFace = std::min(dx, std::min(dy, dz));
		EXPECT_LE(distToFace, 2.0f * voxelSize)
			<< "Surface point (" << p.x << "," << p.y << "," << p.z << ") is too far from any box face";
	}
}

TEST(SurfaceVoxelizerTest, DeepInteriorIsExcludedFromSurfaceBand)
{
	// A large box: every point strictly inside is far from *some* face along
	// its own axis, but "distance to the nearest face plane" (not distance to
	// the center -- a corner is far from the center yet right on two faces)
	// must stay small for a thin surface band (bandWidthInVoxels = 1).
	const Vector3df h(5.0f, 5.0f, 5.0f);
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

	const float voxelSize = 1.0f;
	const auto points = SurfaceVoxelizer::voxelizeSurface(triangles, voxelSize, 1.0f);
	ASSERT_GT(points.size(), 0u);

	for (const auto& p : points) {
		const float dx = std::fabs(std::fabs(p.x) - h.x);
		const float dy = std::fabs(std::fabs(p.y) - h.y);
		const float dz = std::fabs(std::fabs(p.z) - h.z);
		const float distToNearestFace = std::min(dx, std::min(dy, dz));
		EXPECT_LE(distToNearestFace, 2.0f * voxelSize)
			<< "Should not contain deep-interior points far from every face plane";
	}
}

TEST(SurfaceVoxelizerTest, WiderBandProducesMorePointsThanNarrowBand)
{
	const auto narrow = SurfaceVoxelizer::voxelizeSurface(makeUnitBoxMesh(), 0.1f, 0.5f);
	const auto wide   = SurfaceVoxelizer::voxelizeSurface(makeUnitBoxMesh(), 0.1f, 3.0f);
	EXPECT_GT(wide.size(), narrow.size());
}

TEST(SurfaceVoxelizerTest, NonPositiveVoxelSizeIsClampedAndStillProducesPoints)
{
	const auto points = SurfaceVoxelizer::voxelizeSurface(makeUnitBoxMesh(), 0.0f, 1.0f);
	EXPECT_GT(points.size(), 0u);
}
