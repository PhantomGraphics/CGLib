#include "pch.h"

#include "../Volume/MCSurfaceBuilder.h"
#include "../Volume/MCCell.h"
#include "../Volume/SparseVolumeTree/SparseVolume.h"

using namespace Phantom::Math;
using namespace Phantom::Volume;

static MCCell makeUnitCubeCell(const std::array<float, 8>& values)
{
	std::array<MCCell::Vertex, 8> v;
	v[0].position = Vector3df(0.0f, 0.0f, 0.0f); v[0].value = values[0];
	v[1].position = Vector3df(1.0f, 0.0f, 0.0f); v[1].value = values[1];
	v[2].position = Vector3df(1.0f, 1.0f, 0.0f); v[2].value = values[2];
	v[3].position = Vector3df(0.0f, 1.0f, 0.0f); v[3].value = values[3];
	v[4].position = Vector3df(0.0f, 0.0f, 1.0f); v[4].value = values[4];
	v[5].position = Vector3df(1.0f, 0.0f, 1.0f); v[5].value = values[5];
	v[6].position = Vector3df(1.0f, 1.0f, 1.0f); v[6].value = values[6];
	v[7].position = Vector3df(0.0f, 1.0f, 1.0f); v[7].value = values[7];
	return MCCell(v);
}

TEST(MCSurfaceBuilderTest, NoIntersectionProducesNoTriangles)
{
	// All vertices are smaller (or larger) than isolevel -> no triangles generated
	const std::array<float, 8> vals = { -1.f, -1.f, -1.f, -1.f, -1.f, -1.f, -1.f, -1.f };
	const auto cell = makeUnitCubeCell(vals);

	MCSurfaceBuilder builder;
	const int n = builder.march(cell, 0.0f);

	EXPECT_EQ(n, 0) << "Should return 0 when there is no intersection.";
	EXPECT_EQ(builder.getTriangles().size(), 0u) << "Should not generate triangles when there is no intersection.";
}

TEST(MCSurfaceBuilderTest, SingleVertexInsideProducesTriangles)
{
	// Only vertex 0 is inside isolevel (should generate triangles in standard MC table)
	const std::array<float, 8> vals = { -1.f, 1.f, 1.f, 1.f, 1.f, 1.f, 1.f, 1.f };
	const auto cell = makeUnitCubeCell(vals);

	MCSurfaceBuilder builder;
	const int n = builder.march(cell, 0.0f);

	// march return value should match triangles size
	EXPECT_EQ(static_cast<size_t>(n), builder.getTriangles().size());
	// At least 1 triangle should be generated
	EXPECT_GT(n, 0) << "Should generate at least 1 triangle when a single vertex is inside.";

	// Verify generated triangle vertices are within the cube bounds
	for (const auto& tri : builder.getTriangles()) {
		for (int i = 0; i < 3; ++i) {
			const auto& p = tri.getVertices()[i];
			EXPECT_GE(p.x, 0.0f);
			EXPECT_LE(p.x, 1.0f);
			EXPECT_GE(p.y, 0.0f);
			EXPECT_LE(p.y, 1.0f);
			EXPECT_GE(p.z, 0.0f);
			EXPECT_LE(p.z, 1.0f);
		}
	}
}

TEST(MCSurfaceBuilderTest, HorizontalPlaneCutProducesQuadAsTwoTriangles)
{
	// Lower 4 vertices are inside (-1), upper 4 are outside (+1)
	// This should produce a horizontal plane cut at z = 0.5.
	// Standard MC triangulation splits the quad into 2 triangles.
	const std::array<float, 8> vals = { -1.f, -1.f, -1.f, -1.f, 1.f, 1.f, 1.f, 1.f };
	const auto cell = makeUnitCubeCell(vals);

	MCSurfaceBuilder builder;
	const int n = builder.march(cell, 0.0f);
	const auto tris = builder.getTriangles();

	EXPECT_EQ(static_cast<size_t>(n), tris.size());
	EXPECT_EQ(n, 2) << "Horizontal plane cut should produce 2 triangles.";

	// Verify all vertices of generated triangles have z close to 0.5
	for (const auto& tri : tris) {
		for (int i = 0; i < 3; ++i) {
			const auto& p = tri.getVertices()[i];
			EXPECT_NEAR(p.z, 0.5f, 1.0e-6f) << "Cut plane should be close to z = 0.5.";
			EXPECT_GE(p.x, 0.0f);
			EXPECT_LE(p.x, 1.0f);
			EXPECT_GE(p.y, 0.0f);
			EXPECT_LE(p.y, 1.0f);
		}
	}
}

TEST(MCSurfaceBuilderTest, SparseVolumeBuildTest)
{
	SparseVolumef sparse(0.0f);
	sparse.setVoxelSize(1.0f);

	sparse.setValue(Coord(0, 0, 0), 1.0f);

	MCSurfaceBuilder builder;
	builder.build(sparse, 0.5f);

	const auto tris = builder.getTriangles();
	EXPECT_GT(tris.size(), 0u);
}

TEST(MCSurfaceBuilderTest, AllVerticesOutsideProducesNoTriangles)
{
	// All vertices above isolevel (outside) -> config 0x00 -> no triangles
	const std::array<float, 8> vals = { 1.f, 1.f, 1.f, 1.f, 1.f, 1.f, 1.f, 1.f };
	const auto cell = makeUnitCubeCell(vals);

	MCSurfaceBuilder builder;
	const int n = builder.march(cell, 0.0f);
	EXPECT_EQ(n, 0);
	EXPECT_EQ(builder.getTriangles().size(), 0u);
}

TEST(MCSurfaceBuilderTest, ComplementaryVertexOutsideProducesTriangles)
{
	// Vertex 0 outside (+1), all others inside (-1) — complement of SingleVertexInside
	const std::array<float, 8> vals = { 1.f, -1.f, -1.f, -1.f, -1.f, -1.f, -1.f, -1.f };
	const auto cell = makeUnitCubeCell(vals);

	MCSurfaceBuilder builder;
	const int n = builder.march(cell, 0.0f);
	EXPECT_EQ(static_cast<size_t>(n), builder.getTriangles().size());
	EXPECT_GT(n, 0);

	// By MC symmetry, triangle count equals SingleVertexInside (config 0x01 == config 0xFE)
	const std::array<float, 8> valsOrig = { -1.f, 1.f, 1.f, 1.f, 1.f, 1.f, 1.f, 1.f };
	MCSurfaceBuilder builderOrig;
	builderOrig.march(makeUnitCubeCell(valsOrig), 0.0f);
	EXPECT_EQ(n, static_cast<int>(builderOrig.getTriangles().size()));

	for (const auto& tri : builder.getTriangles()) {
		for (int i = 0; i < 3; ++i) {
			const auto& p = tri.getVertices()[i];
			EXPECT_GE(p.x, 0.0f); EXPECT_LE(p.x, 1.0f);
			EXPECT_GE(p.y, 0.0f); EXPECT_LE(p.y, 1.0f);
			EXPECT_GE(p.z, 0.0f); EXPECT_LE(p.z, 1.0f);
		}
	}
}

TEST(MCSurfaceBuilderTest, VerticalPlaneCutProducesTwoTriangles)
{
	// Upper 4 vertices inside (-1), lower 4 outside (+1) — complement of HorizontalPlaneCut
	const std::array<float, 8> vals = { 1.f, 1.f, 1.f, 1.f, -1.f, -1.f, -1.f, -1.f };
	const auto cell = makeUnitCubeCell(vals);

	MCSurfaceBuilder builder;
	const int n = builder.march(cell, 0.0f);
	const auto tris = builder.getTriangles();

	EXPECT_EQ(static_cast<size_t>(n), tris.size());
	EXPECT_EQ(n, 2) << "Complement of horizontal cut should also produce 2 triangles";

	for (const auto& tri : tris) {
		for (int i = 0; i < 3; ++i) {
			const auto& p = tri.getVertices()[i];
			EXPECT_NEAR(p.z, 0.5f, 1.0e-6f) << "Cut plane should be at z = 0.5";
		}
	}
}

TEST(MCSurfaceBuilderTest, MultipleMarches)
{
	// Two separate cells marched on the same builder — triangles accumulate
	const std::array<float, 8> vals1 = { -1.f, 1.f, 1.f, 1.f, 1.f, 1.f, 1.f, 1.f };
	const std::array<float, 8> vals2 = { -1.f, -1.f, -1.f, -1.f, 1.f, 1.f, 1.f, 1.f };

	MCSurfaceBuilder builder;
	const int n1 = builder.march(makeUnitCubeCell(vals1), 0.0f);
	const int n2 = builder.march(makeUnitCubeCell(vals2), 0.0f);

	EXPECT_GT(n1, 0);
	EXPECT_GT(n2, 0);
	EXPECT_EQ(static_cast<int>(builder.getTriangles().size()), n1 + n2);
}