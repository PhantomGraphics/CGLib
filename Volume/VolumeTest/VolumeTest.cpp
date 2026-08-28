#include "pch.h"

#include "../Volume/LevelSet.h"
#include "../Volume/SparseVolumeTree/SparseVolume.h"
#include "../Volume/SparseVolumeTree/Interpolator.h"

#include "../../../CGLib/Math/Box3d.h"
#include "../../../CGLib/Math/Triangle3d.h"
#include "../../../CGLib/Math/Vector3d.h"

#include <algorithm>
#include <climits>
#include <cmath>
#include <memory>

using namespace Phantom::Math;
using namespace Phantom::Volume;

// ---------------------------------------------------------------------------
// SparseVolume CSG operations (same math as SVCombineView)
// ---------------------------------------------------------------------------

static std::unique_ptr<SparseVolumef> makeBoxSdf(
    const Box3df& box, float cell, float thickness)
{
    auto vol = std::make_unique<SparseVolumef>(1e6f);
    vol->setVoxelSize(cell);
    LevelSet ls;
    ls.setSignedDistance(box, *vol, static_cast<double>(thickness));
    return vol;
}

TEST(CSGTest, UnionContainsBothInteriors)
{
    // Box A: [-5,5]^3, Box B: shifted to [2,12] on x-axis
    const float cell = 1.0f, thick = 3.0f;
    auto volA = makeBoxSdf(Box3df(Vector3df(-5,-5,-5), Vector3df(5,5,5)), cell, thick);
    auto volB = makeBoxSdf(Box3df(Vector3df( 2,-5,-5), Vector3df(12,5,5)), cell, thick);

    TrilinearInterpolator<float> interpA(*volA);
    TrilinearInterpolator<float> interpB(*volB);

    // Union = min(sdfA, sdfB)
    // Point inside A only: (-3, 0, 0)
    const Vector3df insideA(-3.f, 0.f, 0.f);
    const float unionA = std::min(interpA.getValue(insideA), interpB.getValue(insideA));
    EXPECT_LT(unionA, 0.0f) << "Union should include interior of A";

    // Point inside B only: (10, 0, 0)
    const Vector3df insideB(10.f, 0.f, 0.f);
    const float unionB = std::min(interpA.getValue(insideB), interpB.getValue(insideB));
    EXPECT_LT(unionB, 0.0f) << "Union should include interior of B";
}

TEST(CSGTest, DifferenceExcludesB)
{
    const float cell = 1.0f, thick = 3.0f;
    auto volA = makeBoxSdf(Box3df(Vector3df(-5,-5,-5), Vector3df(5,5,5)), cell, thick);
    auto volB = makeBoxSdf(Box3df(Vector3df(-5,-5,-5), Vector3df(0, 5, 5)), cell, thick);

    TrilinearInterpolator<float> interpA(*volA);
    TrilinearInterpolator<float> interpB(*volB);

    // Difference A-B = max(sdfA, -sdfB)
    // Point inside both: (-3, 0, 0) → inside A, inside B → diff should be outside
    const Vector3df insideBoth(-3.f, 0.f, 0.f);
    const float diffVal = std::max(interpA.getValue(insideBoth), -interpB.getValue(insideBoth));
    EXPECT_GT(diffVal, 0.0f) << "Diff A-B: point inside B should be excluded";

    // Point inside A only: (3, 0, 0) → inside A, outside B → diff should include it
    const Vector3df insideAOnly(3.f, 0.f, 0.f);
    const float diffA = std::max(interpA.getValue(insideAOnly), -interpB.getValue(insideAOnly));
    EXPECT_LT(diffA, 0.0f) << "Diff A-B: point inside A but outside B should be included";
}

TEST(CSGTest, IntersectionOnlyWhereOverlap)
{
    const float cell = 1.0f, thick = 3.0f;
    auto volA = makeBoxSdf(Box3df(Vector3df(-5,-5,-5), Vector3df(5,5,5)), cell, thick);
    auto volB = makeBoxSdf(Box3df(Vector3df( 0,-5,-5), Vector3df(10,5,5)), cell, thick);

    TrilinearInterpolator<float> interpA(*volA);
    TrilinearInterpolator<float> interpB(*volB);

    // Intersection = max(sdfA, sdfB)
    // Point in overlap: (2, 0, 0) → inside both → intersection should be inside
    const Vector3df overlap(2.f, 0.f, 0.f);
    const float interVal = std::max(interpA.getValue(overlap), interpB.getValue(overlap));
    EXPECT_LT(interVal, 0.0f) << "Intersection: overlap region should be inside";

    // Point only inside A: (-3, 0, 0) → inside A, outside B → intersection should be outside
    const Vector3df onlyA(-3.f, 0.f, 0.f);
    const float interA = std::max(interpA.getValue(onlyA), interpB.getValue(onlyA));
    EXPECT_GT(interA, 0.0f) << "Intersection: A-only region should be excluded";
}

// ---------------------------------------------------------------------------
// Resample: trilinear interpolation preserves SDF values
// ---------------------------------------------------------------------------

TEST(ResampleTest, TrilinearPreservesValueAtCenter)
{
    SparseVolumef original(1e6f);
    original.setVoxelSize(1.0f);
    // 2x2x2 block with value 4.0 (trilinear center = 4.0)
    for (int i = 0; i <= 1; ++i)
        for (int j = 0; j <= 1; ++j)
            for (int k = 0; k <= 1; ++k)
                original.setValue(Coord(i, j, k), 4.0f);

    TrilinearInterpolator<float> interp(original);
    // Center of the unit cube at (0.5, 0.5, 0.5) should interpolate to 4.0
    EXPECT_NEAR(interp.getValue(Vector3df(0.5f, 0.5f, 0.5f)), 4.0f, 1e-5f);
}

TEST(ResampleTest, FinerResolutionMoreVoxels)
{
    // Create a sphere SDF at voxelSize=2, resample to voxelSize=1
    SparseVolumef coarse(1e6f);
    coarse.setVoxelSize(2.0f);
    const float radius = 10.0f, band = 3.0f * 2.0f;
    for (int i = -8; i <= 8; ++i)
        for (int j = -8; j <= 8; ++j)
            for (int k = -8; k <= 8; ++k) {
                const float d = std::sqrt(float(i*i+j*j+k*k)) * 2.0f - radius;
                if (std::fabs(d) <= band)
                    coarse.setValue(Coord(i, j, k), d);
            }

    // Resample to voxelSize=1
    SparseVolumef fine(1e6f);
    fine.setVoxelSize(1.0f);
    TrilinearInterpolator<float> interp(coarse);
    const auto bbox = coarse.getBoundingBox();
    const Coord iMin = fine.worldToIndex(bbox.getMin());
    const Coord iMax = fine.worldToIndex(bbox.getMax());
    for (int i = iMin.x - 1; i <= iMax.x + 1; ++i)
        for (int j = iMin.y - 1; j <= iMax.y + 1; ++j)
            for (int k = iMin.z - 1; k <= iMax.z + 1; ++k) {
                const Coord idx(i, j, k);
                const float v = interp.getValue(fine.indexToWorld(idx));
                if (std::fabs(v) < 1e5f) fine.setValue(idx, v);
            }

    // Fine grid should have more voxels than coarse (higher resolution)
    EXPECT_GT(fine.getActiveVoxelCount(), coarse.getActiveVoxelCount());
}

// ---------------------------------------------------------------------------
// LevelSet + MarchingCubes pipeline (dense volume path)
// ---------------------------------------------------------------------------

#include "../Volume/MCSurfaceBuilder.h"
#include "../Volume/Volume.h"

TEST(CSGTest, ChainedUnionThreeBoxes)
{
    // 3 つの非連続なボックスを Union し、それぞれの内部点がすべて含まれることを確認
    const float cell = 1.0f, thick = 3.0f;
    auto volA = makeBoxSdf(Box3df(Vector3df(-10,-5,-5), Vector3df(-2,5,5)), cell, thick);
    auto volB = makeBoxSdf(Box3df(Vector3df(  0,-5,-5), Vector3df( 8,5,5)), cell, thick);
    auto volC = makeBoxSdf(Box3df(Vector3df( 12,-5,-5), Vector3df(20,5,5)), cell, thick);

    TrilinearInterpolator<float> interpA(*volA);
    TrilinearInterpolator<float> interpB(*volB);
    TrilinearInterpolator<float> interpC(*volC);

    auto unionVal = [&](const Vector3df& p) {
        return std::min({interpA.getValue(p), interpB.getValue(p), interpC.getValue(p)});
    };

    // ボックス内部（表面から 1 ボクセル内側）の点はすべて Union に含まれる
    EXPECT_LT(unionVal(Vector3df(-3.f, 0.f, 0.f)), 0.0f) << "Union should include interior of A";
    EXPECT_LT(unionVal(Vector3df( 1.f, 0.f, 0.f)), 0.0f) << "Union should include interior of B";
    EXPECT_LT(unionVal(Vector3df(13.f, 0.f, 0.f)), 0.0f) << "Union should include interior of C";

    // ボックス間の隙間は Union の外側
    EXPECT_GT(unionVal(Vector3df(-1.f, 0.f, 0.f)), 0.0f) << "Gap between A and B should be outside";
}

TEST(ResampleTest, TrilinearPreservesGradientDirection)
{
    // f(x,y,z) = x のみ線形なフィールド: gradient = (1, 0, 0)
    SparseVolumef sv;
    sv.setVoxelSize(1.0f);
    for (int i = -1; i <= 3; ++i)
        for (int j = -1; j <= 3; ++j)
            for (int k = -1; k <= 3; ++k)
                sv.setValue(Coord(i, j, k), static_cast<float>(i));

    TrilinearInterpolator<float> interp(sv);
    const auto g = interp.getGradient(Vector3df(1.0f, 1.0f, 1.0f));
    EXPECT_NEAR(g.x, 1.0f, 1e-5f);
    EXPECT_NEAR(g.y, 0.0f, 1e-5f);
    EXPECT_NEAR(g.z, 0.0f, 1e-5f);
}

TEST(PipelineTest, LevelSetBoxToMCSurface)
{
    // 1. Create box SDF
    SparseVolumef sv(1e6f);
    sv.setVoxelSize(1.0f);
    LevelSet ls;
    ls.setSignedDistance(Box3df(Vector3df(-5,-5,-5), Vector3df(5,5,5)), sv, 3.0);
    ASSERT_GT(sv.getActiveVoxelCount(), 0);

    // 2. Convert sparse to dense (same logic as MCMeshView)
    Coord minC(INT_MAX, INT_MAX, INT_MAX), maxC(INT_MIN, INT_MIN, INT_MIN);
    sv.forEachActive([&](const Coord& c, const Vector3df&, float) {
        minC.x = std::min(minC.x, c.x); maxC.x = std::max(maxC.x, c.x);
        minC.y = std::min(minC.y, c.y); maxC.y = std::max(maxC.y, c.y);
        minC.z = std::min(minC.z, c.z); maxC.z = std::max(maxC.z, c.z);
    });

    const int ox = minC.x - 1, oy = minC.y - 1, oz = minC.z - 1;
    const int nx = maxC.x - minC.x + 3;
    const int ny = maxC.y - minC.y + 3;
    const int nz = maxC.z - minC.z + 3;
    const float vs = sv.getVoxelSize();
    const Vector3df bMin((ox - 0.5f) * vs, (oy - 0.5f) * vs, (oz - 0.5f) * vs);
    const Vector3df bMax(bMin.x + nx*vs, bMin.y + ny*vs, bMin.z + nz*vs);

    Phantom::Volume::Volumef dense(Box3df(bMin, bMax),
                                   { static_cast<size_t>(nx),
                                     static_cast<size_t>(ny),
                                     static_cast<size_t>(nz) });

    const float bg = sv.getBackground();
    for (int i = 0; i < nx; ++i)
        for (int j = 0; j < ny; ++j)
            for (int k = 0; k < nz; ++k)
                dense.setValue({ i, j, k }, bg);

    sv.forEachActive([&](const Coord& c, const Vector3df&, float val) {
        dense.setValue({ c.x - ox, c.y - oy, c.z - oz }, val);
    });

    // 3. Run Marching Cubes
    MCSurfaceBuilder mc;
    mc.build(dense, 0.0f);
    const auto& tris = mc.getTriangles();
    EXPECT_GT(tris.size(), 0u) << "Box SDF should produce surface triangles";

    // Surface area of a 10x10x10 box ≈ 600 sq units; each triangle ≈ 0.5 sq → ~1200+ triangles
    EXPECT_GT(static_cast<int>(tris.size()), 100);
}

TEST(PipelineTest, SphereLevelSetToMCSurface)
{
    // 球体 SDF を直接 SparseVolume に書き込み、Marching Cubes で表面を生成する
    SparseVolumef sv(1e6f);
    sv.setVoxelSize(1.0f);
    const float radius = 8.0f, band = 3.0f;
    for (int i = -12; i <= 12; ++i)
        for (int j = -12; j <= 12; ++j)
            for (int k = -12; k <= 12; ++k) {
                const float d = std::sqrt(static_cast<float>(i*i + j*j + k*k)) - radius;
                if (std::fabs(d) <= band)
                    sv.setValue(Coord(i, j, k), d);
            }
    ASSERT_GT(sv.getActiveVoxelCount(), 0);

    MCSurfaceBuilder mc;
    mc.build(sv, 0.0f);
    const auto& tris = mc.getTriangles();

    // 球面積 ≈ 4π r² ≈ 804; 各三角形 ≈ 0.5 → 100 枚以上
    EXPECT_GT(static_cast<int>(tris.size()), 100);
}
