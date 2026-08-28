#include "pch.h"

#include "../Volume/SparseVolumeTree/SparseVolume.h"
#include "../Volume/SparseVolumeTree/Interpolator.h"
#include "../Volume/SparseVolumeTree/ValueAccessor.h"

using namespace Phantom::Math;
using namespace Phantom::Volume;

// ---- Coord ------------------------------------------------------------------

TEST(CoordTest, EqualityAndHash) {
    Coord a(1, 2, 3);
    Coord b(1, 2, 3);
    Coord c(4, 5, 6);

    EXPECT_EQ(a, b);
    EXPECT_NE(a, c);

    Coord::Hash h;
    EXPECT_EQ(h(a), h(b));
}

TEST(CoordTest, BitOps) {
    Coord c(0b1010, 0b1100, 0b0111);
    Coord shifted = c >> 1;
    EXPECT_EQ(shifted.x, 0b101);
    EXPECT_EQ(shifted.y, 0b110);
    EXPECT_EQ(shifted.z, 0b011);

    Coord masked = c & 0b111;
    EXPECT_EQ(masked.x, 0b010);
    EXPECT_EQ(masked.y, 0b100);
    EXPECT_EQ(masked.z, 0b111);
}

// ---- LeafNode ---------------------------------------------------------------

TEST(LeafNodeTest, SetAndGet) {
    LeafNode<float> leaf(0.0f);
    EXPECT_EQ(leaf.getActiveCount(), 0);
    EXPECT_TRUE(leaf.isEmpty());

    int offset = LeafNode<float>::coordToOffset(Coord(3, 5, 7));
    leaf.setValue(offset, 1.5f);
    EXPECT_FLOAT_EQ(leaf.getValue(offset), 1.5f);
    EXPECT_TRUE(leaf.isActive(offset));
    EXPECT_EQ(leaf.getActiveCount(), 1);
    EXPECT_FALSE(leaf.isEmpty());
}

TEST(LeafNodeTest, BackgroundForInactive) {
    LeafNode<float> leaf(99.0f);
    int offset = LeafNode<float>::coordToOffset(Coord(0, 0, 0));
    // 未設定スロットはバックグラウンド値を返す
    EXPECT_FLOAT_EQ(leaf.getValue(offset), 99.0f);
    EXPECT_FALSE(leaf.isActive(offset));
}

TEST(LeafNodeTest, CoordToOffset) {
    EXPECT_EQ(LeafNode<float>::coordToOffset(Coord(0, 0, 0)), 0);
    // x=1 -> 1 * 8 * 8 = 64
    EXPECT_EQ(LeafNode<float>::coordToOffset(Coord(1, 0, 0)), 64);
    // y=1 -> 1 * 8 = 8
    EXPECT_EQ(LeafNode<float>::coordToOffset(Coord(0, 1, 0)), 8);
    // z=1 -> 1
    EXPECT_EQ(LeafNode<float>::coordToOffset(Coord(0, 0, 1)), 1);
    // 座標が DIM (8) の倍数でも低 3 ビットで評価されるので 0
    EXPECT_EQ(LeafNode<float>::coordToOffset(Coord(8, 0, 0)), 0);
}

TEST(LeafNodeTest, ForEachActive) {
    LeafNode<float> leaf(0.0f);
    Coord origin(0, 0, 0);
    leaf.setValue(LeafNode<float>::coordToOffset(Coord(1, 2, 3)), 7.0f);
    leaf.setValue(LeafNode<float>::coordToOffset(Coord(4, 5, 6)), 3.0f);

    int count = 0;
    float sum = 0.0f;
    leaf.forEachActive(origin, [&](const Coord&, float v) {
        ++count;
        sum += v;
    });
    EXPECT_EQ(count, 2);
    EXPECT_FLOAT_EQ(sum, 10.0f);
}

// ---- SparseVolume -----------------------------------------------------------

TEST(SparseVolumeTreeTest, SetAndGet) {
    SparseVolumef sv;
    sv.setValue(Coord(0, 0, 0), 1.0f);
    sv.setValue(Coord(100, 200, 300), 2.5f);

    EXPECT_FLOAT_EQ(sv.getValue(Coord(0, 0, 0)),       1.0f);
    EXPECT_FLOAT_EQ(sv.getValue(Coord(100, 200, 300)),  2.5f);
    EXPECT_FLOAT_EQ(sv.getValue(Coord(1, 2, 3)),        0.0f);
}

TEST(SparseVolumeTreeTest, Background) {
    SparseVolumef sv(-1.0f);
    EXPECT_FLOAT_EQ(sv.getBackground(),                -1.0f);
    EXPECT_FLOAT_EQ(sv.getValue(Coord(99, 99, 99)),    -1.0f);
}

TEST(SparseVolumeTreeTest, ActiveVoxelCount) {
    SparseVolumef sv;
    EXPECT_EQ(sv.getActiveVoxelCount(), 0);

    sv.setValue(Coord(0, 0, 0), 1.0f);
    sv.setValue(Coord(1, 0, 0), 2.0f);
    sv.setValue(Coord(0, 1, 0), 3.0f);
    EXPECT_EQ(sv.getActiveVoxelCount(), 3);
}

TEST(SparseVolumeTreeTest, ForEachActive) {
    SparseVolumef sv;
    sv.setValue(Coord(10, 20, 30), 5.0f);
    sv.setValue(Coord(11, 20, 30), 6.0f);

    float sum   = 0.0f;
    int   count = 0;
    sv.forEachActive([&](const Coord&, const Vector3df&, float v) {
        sum += v;
        ++count;
    });
    EXPECT_EQ(count, 2);
    EXPECT_FLOAT_EQ(sum, 11.0f);
}

TEST(SparseVolumeTreeTest, VoxelSizeAndWorldCoord) {
    SparseVolumef sv;
    sv.setVoxelSize(0.5f);
    EXPECT_FLOAT_EQ(sv.getVoxelSize(), 0.5f);

    Vector3df world = sv.indexToWorld(Coord(4, 6, 8));
    EXPECT_FLOAT_EQ(world.x, 2.0f);
    EXPECT_FLOAT_EQ(world.y, 3.0f);
    EXPECT_FLOAT_EQ(world.z, 4.0f);

    Coord idx = sv.worldToIndex(Vector3df(2.0f, 3.0f, 4.0f));
    EXPECT_EQ(idx.x, 4);
    EXPECT_EQ(idx.y, 6);
    EXPECT_EQ(idx.z, 8);
}

TEST(SparseVolumeTreeTest, Clear) {
    SparseVolumef sv;
    sv.setValue(Coord(1, 2, 3), 9.0f);
    sv.clear();
    EXPECT_EQ(sv.getActiveVoxelCount(), 0);
    EXPECT_FLOAT_EQ(sv.getValue(Coord(1, 2, 3)), 0.0f);
}

TEST(SparseVolumeTreeTest, NegativeCoords) {
    SparseVolumef sv;
    sv.setValue(Coord(-10, -20, -30), 42.0f);
    EXPECT_FLOAT_EQ(sv.getValue(Coord(-10, -20, -30)), 42.0f);
    EXPECT_FLOAT_EQ(sv.getValue(Coord(10, 20, 30)),     0.0f);
}

TEST(SparseVolumeTreeTest, BoundingBox) {
    SparseVolumef sv;
    sv.setVoxelSize(1.0f);
    sv.setValue(Coord(0, 0, 0),  1.0f);
    sv.setValue(Coord(10, 5, 3), 2.0f);

    auto box = sv.getBoundingBox();
    EXPECT_FLOAT_EQ(box.getMin().x,  0.0f);
    EXPECT_FLOAT_EQ(box.getMin().y,  0.0f);
    EXPECT_FLOAT_EQ(box.getMin().z,  0.0f);
    EXPECT_FLOAT_EQ(box.getMax().x, 10.0f);
    EXPECT_FLOAT_EQ(box.getMax().y,  5.0f);
    EXPECT_FLOAT_EQ(box.getMax().z,  3.0f);
}

TEST(SparseVolumeTreeTest, InterpolatorTrilinearCenter) {
    SparseVolumef sv;
    sv.setVoxelSize(1.0f);

    for (int i = 0; i <= 1; ++i) {
        for (int j = 0; j <= 1; ++j) {
            for (int k = 0; k <= 1; ++k) {
                sv.setValue(Coord(i, j, k), static_cast<float>(i + j + k));
            }
        }
    }

    TrilinearInterpolator<float> interp(sv);
    const float v = interp.getValue(Vector3df(0.5f, 0.5f, 0.5f));
    EXPECT_NEAR(v, 1.5f, 1e-6f);
}

TEST(SparseVolumeTreeTest, InterpolatorGradientLinearField) {
    SparseVolumef sv;
    sv.setVoxelSize(1.0f);

    for (int i = -1; i <= 3; ++i) {
        for (int j = -1; j <= 3; ++j) {
            for (int k = -1; k <= 3; ++k) {
                sv.setValue(Coord(i, j, k), static_cast<float>(i + j + k));
            }
        }
    }

    TrilinearInterpolator<float> interp(sv);
    const auto g = interp.getGradient(Vector3df(0.5f, 0.5f, 0.5f));
    EXPECT_NEAR(g.x, 1.0f, 1e-6f);
    EXPECT_NEAR(g.y, 1.0f, 1e-6f);
    EXPECT_NEAR(g.z, 1.0f, 1e-6f);
}

// ---- ValueAccessor ----------------------------------------------------------

TEST(ValueAccessorTest, SetAndGet) {
    SparseVolumef sv;
    ValueAccessor<float> acc(sv);

    acc.setValue(Coord(1, 2, 3), 10.0f);
    EXPECT_FLOAT_EQ(acc.getValue(Coord(1, 2, 3)), 10.0f);
}

TEST(ValueAccessorTest, CacheHitSameLeaf) {
    SparseVolumef sv;
    ValueAccessor<float> acc(sv);

    // 同じリーフ (8^3 ブロック) 内の連続アクセスはキャッシュヒット
    for (int z = 0; z < 8; ++z) {
        acc.setValue(Coord(0, 0, z), static_cast<float>(z));
    }
    for (int z = 0; z < 8; ++z) {
        EXPECT_FLOAT_EQ(acc.getValue(Coord(0, 0, z)), static_cast<float>(z));
    }
}

TEST(ValueAccessorTest, BackgroundForMissing) {
    SparseVolumef sv(99.0f);
    ValueAccessor<float> acc(sv);
    EXPECT_FLOAT_EQ(acc.getValue(Coord(500, 500, 500)), 99.0f);
}

TEST(ValueAccessorTest, ClearCache) {
    SparseVolumef sv;
    ValueAccessor<float> acc(sv);
    acc.setValue(Coord(0, 0, 0), 1.0f);
    acc.clearCache();
    // キャッシュクリア後もツリー経由で正しい値を返す
    EXPECT_FLOAT_EQ(acc.getValue(Coord(0, 0, 0)), 1.0f);
}

TEST(ValueAccessorTest, LargeScatterWrite) {
    SparseVolumef sv;
    ValueAccessor<float> acc(sv);

    // 異なるリーフへの書き込み (キャッシュミスが発生する)
    acc.setValue(Coord(0,   0,   0),   1.0f);
    acc.setValue(Coord(100, 0,   0),   2.0f);
    acc.setValue(Coord(0,   100, 0),   3.0f);
    acc.setValue(Coord(0,   0,   100), 4.0f);

    EXPECT_FLOAT_EQ(sv.getValue(Coord(0,   0,   0)),   1.0f);
    EXPECT_FLOAT_EQ(sv.getValue(Coord(100, 0,   0)),   2.0f);
    EXPECT_FLOAT_EQ(sv.getValue(Coord(0,   100, 0)),   3.0f);
    EXPECT_FLOAT_EQ(sv.getValue(Coord(0,   0,   100)), 4.0f);
    EXPECT_EQ(sv.getActiveVoxelCount(), 4);
}

// ---- LeafNode: setActive deactivation ---------------------------------------

TEST(LeafNodeTest, SetActiveDeactivation) {
    LeafNode<float> leaf(99.0f);
    const int offset = LeafNode<float>::coordToOffset(Coord(2, 3, 4));

    leaf.setValue(offset, 5.0f);
    EXPECT_TRUE(leaf.isActive(offset));
    EXPECT_FLOAT_EQ(leaf.getValue(offset), 5.0f);
    EXPECT_EQ(leaf.getActiveCount(), 1);

    leaf.setActive(offset, false);
    EXPECT_FALSE(leaf.isActive(offset));
    EXPECT_FLOAT_EQ(leaf.getValue(offset), 99.0f);  // 背景値を返す
    EXPECT_EQ(leaf.getActiveCount(), 0);
    EXPECT_TRUE(leaf.isEmpty());
}

// ---- SparseVolume: isActive -------------------------------------------------

TEST(SparseVolumeTreeTest, IsActive) {
    SparseVolumef sv(0.0f);
    EXPECT_FALSE(sv.isActive(Coord(5, 5, 5)));

    sv.setValue(Coord(5, 5, 5), 1.0f);
    EXPECT_TRUE(sv.isActive(Coord(5, 5, 5)));
}

// isActive は getValue != getBackground() で実装されているため、背景値と同じ値を
// setValue してもアクティブと見なされない。一方 getActiveVoxelCount はビットマスクを
// 参照するので 1 を返す。この仕様の相違を文書化するテスト。
TEST(SparseVolumeTreeTest, IsActiveReturnsFalseForBackgroundValue) {
    SparseVolumef sv(-1.0f);
    sv.setValue(Coord(0, 0, 0), -1.0f);
    EXPECT_FALSE(sv.isActive(Coord(0, 0, 0)));
    EXPECT_EQ(sv.getActiveVoxelCount(), 1);
}

TEST(SparseVolumeTreeTest, OverwriteSameCoord) {
    SparseVolumef sv;
    sv.setValue(Coord(3, 3, 3), 1.0f);
    sv.setValue(Coord(3, 3, 3), 2.0f);

    EXPECT_FLOAT_EQ(sv.getValue(Coord(3, 3, 3)), 2.0f);
    EXPECT_EQ(sv.getActiveVoxelCount(), 1);  // 2 にならない
}

TEST(SparseVolumeTreeTest, ForEachActiveVerifiesCoords) {
    SparseVolumef sv;
    sv.setVoxelSize(1.0f);
    sv.setValue(Coord(7, 13, 21), 42.0f);

    Coord   foundIdx(-999, -999, -999);
    Vector3df foundWorld(-1.0f, -1.0f, -1.0f);
    float   foundVal = -1.0f;
    sv.forEachActive([&](const Coord& c, const Vector3df& w, float v) {
        foundIdx   = c;
        foundWorld = w;
        foundVal   = v;
    });

    EXPECT_EQ(foundIdx.x, 7);
    EXPECT_EQ(foundIdx.y, 13);
    EXPECT_EQ(foundIdx.z, 21);
    EXPECT_FLOAT_EQ(foundWorld.x, 7.0f);
    EXPECT_FLOAT_EQ(foundWorld.y, 13.0f);
    EXPECT_FLOAT_EQ(foundWorld.z, 21.0f);
    EXPECT_FLOAT_EQ(foundVal, 42.0f);
}

TEST(SparseVolumeTreeTest, BoundingBoxEmptyVolume) {
    SparseVolumef sv;
    const auto bb = sv.getBoundingBox();  // クラッシュしないこと
    EXPECT_EQ(sv.getActiveVoxelCount(), 0);
    (void)bb;
}

TEST(SparseVolumeTreeTest, InterpolatorOffCenter) {
    // f(x,y,z) = x のみ線形: getValue(0.25,0,0) ≈ 0.25、getValue(0.75,0.5,0.5) ≈ 0.75
    SparseVolumef sv;
    sv.setVoxelSize(1.0f);
    for (int i = 0; i <= 1; ++i)
        for (int j = 0; j <= 1; ++j)
            for (int k = 0; k <= 1; ++k)
                sv.setValue(Coord(i, j, k), static_cast<float>(i));

    TrilinearInterpolator<float> interp(sv);
    EXPECT_NEAR(interp.getValue(Vector3df(0.25f, 0.0f, 0.0f)), 0.25f, 1e-6f);
    EXPECT_NEAR(interp.getValue(Vector3df(0.75f, 0.5f, 0.5f)), 0.75f, 1e-6f);
}
