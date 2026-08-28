#include "gtest/gtest.h"

#include "../Space/BVH.h"
#include <algorithm>
#include <vector>

using namespace Phantom::Math;
using namespace Phantom::Space;

namespace {

// ヘルパー: pair が vector に含まれるか
bool containsPair(const std::vector<std::pair<int,int>>& pairs, int a, int b) {
    return std::any_of(pairs.begin(), pairs.end(), [&](const std::pair<int,int>& p) {
        return p.first == a && p.second == b;
    });
}

bool containsObjectId(const std::vector<BVHObject*>& objs, int id) {
    return std::any_of(objs.begin(), objs.end(), [&](const BVHObject* o) {
        return o != nullptr && o->id == id;
    });
}

} // namespace

TEST(BVHTest, QueryOverlapsSimple)
{
    std::vector<BVHObject*> objs;
    objs.push_back(new BVHObject(0, Box3df(Vector3df(0.0f,0.0f,0.0f), Vector3df(1.0f,1.0f,1.0f))));
    objs.push_back(new BVHObject(1, Box3df(Vector3df(0.5f,0.5f,0.5f), Vector3df(1.5f,1.5f,1.5f))));
    objs.push_back(new BVHObject(2, Box3df(Vector3df(2.0f,2.0f,2.0f), Vector3df(3.0f,3.0f,3.0f))));

    BVH bvh(objs, /*leafMax=*/2);

    Box3df query(Vector3df(0.8f,0.8f,0.8f), Vector3df(1.2f,1.2f,1.2f));
    const auto hits = bvh.queryOverlaps(query);

    // id 0 と 1 がヒットするはず
    EXPECT_EQ(2u, hits.size());
    //EXPECT_TRUE(std::find(hits.begin(), hits.end(), 0) != hits.end());
    //EXPECT_TRUE(std::find(hits.begin(), hits.end(), 1) != hits.end());

    for(auto obj : objs) {
        delete obj;
	}
}

TEST(BVHTest, FindAllPairsBasic)
{
    std::vector<BVHObject*> objs;
    objs.push_back(new BVHObject(0, Box3df(Vector3df(0.0f,0.0f,0.0f), Vector3df(1.0f,1.0f,1.0f)) ));
    objs.push_back(new BVHObject(1, Box3df(Vector3df(0.5f,0.5f,0.5f), Vector3df(1.5f,1.5f,1.5f)) ));
    objs.push_back(new BVHObject(2, Box3df(Vector3df(2.0f,2.0f,2.0f), Vector3df(3.0f,3.0f,3.0f)) ));

    BVH bvh(objs, /*leafMax=*/1);

    auto pairs = bvh.findAllPairs();

    // 唯一の重なりペアは (0,1)
    EXPECT_TRUE(containsPair(pairs, 0, 1));
    EXPECT_EQ(1u, pairs.size());

    for (auto obj : objs) {
        delete obj;
    }

}

TEST(BVHTest, QueryRay_Hit)
{
    std::vector<BVHObject*> objs;
    objs.push_back(new BVHObject(0, Box3df(Vector3df(1.0f, -1.0f, -1.0f), Vector3df(2.0f, 1.0f, 1.0f))));
    objs.push_back(new BVHObject(1, Box3df(Vector3df(5.0f, 5.0f, 5.0f), Vector3df(6.0f, 6.0f, 6.0f))));

    BVH bvh(objs, /*leafMax=*/1);

    auto hits = bvh.queryRay(Vector3df(0.0f, 0.0f, 0.0f), Vector3df(1.0f, 0.1f, 0.1f));
    EXPECT_EQ(1u, hits.size());
    EXPECT_TRUE(containsObjectId(hits, 0));

    for (auto obj : objs) {
        delete obj;
    }
}

TEST(BVHTest, QueryRay_Miss)
{
    std::vector<BVHObject*> objs;
    objs.push_back(new BVHObject(0, Box3df(Vector3df(1.0f, 1.0f, 1.0f), Vector3df(2.0f, 2.0f, 2.0f))));

    BVH bvh(objs, /*leafMax=*/1);

    auto hits = bvh.queryRay(Vector3df(0.0f, 0.0f, 0.0f), Vector3df(-1.0f, -1.0f, -1.0f));
    EXPECT_TRUE(hits.empty());

    for (auto obj : objs) {
        delete obj;
    }
}

TEST(BVHTest, QueryRay_Refit_AfterMove)
{
    std::vector<BVHObject*> objs;
    objs.push_back(new BVHObject(0, Box3df(Vector3df(5.0f, 2.0f, -0.5f), Vector3df(6.0f, 3.0f, 0.5f))));

    BVH bvh(objs, /*leafMax=*/1);

    auto before = bvh.queryRay(Vector3df(0.0f, 0.0f, 0.0f), Vector3df(1.0f, 0.0f, 0.0f));
    EXPECT_TRUE(before.empty());

    objs[0]->box = Box3df(Vector3df(1.0f, -0.5f, -0.5f), Vector3df(2.0f, 0.5f, 0.5f));
    bvh.refit();

    auto after = bvh.queryRay(Vector3df(0.0f, 0.0f, 0.0f), Vector3df(1.0f, 0.0f, 0.0f));
    EXPECT_EQ(1u, after.size());
    EXPECT_TRUE(containsObjectId(after, 0));

    for (auto obj : objs) {
        delete obj;
    }
}

TEST(BVHTest, LeafMaxAffectsNodeCount)
{
    // 8 個の非重複オブジェクト（それぞれ別の位置)
    std::vector<BVHObject*> objs;
    for (int i = 0; i < 8; ++i) {
        float x = static_cast<float>(i * 10);
        objs.push_back(new BVHObject( i, Box3df(Vector3df(x, 0.0f, 0.0f), Vector3df(x + 1.0f, 1.0f, 1.0f)) ));
    }

    // leafMax を 8 にすると葉はルート1つだけ => ノード数は 1
    BVH bvhOneLeaf(objs, /*leafMax=*/8);
    EXPECT_EQ(1, bvhOneLeaf.nodeCount());

    // leafMax を 1 にすると複数ノードになる
    BVH bvhManyLeaves(objs, /*leafMax=*/1);
    EXPECT_GT(bvhManyLeaves.nodeCount(), 1);

    for (auto obj : objs) {
        delete obj;
    }

}

TEST(BVHTest, TouchingBoundaryIsIntersect)
{
    std::vector<BVHObject*> objs;
    // オブジェクトは x=[1,2]
    objs.push_back(new BVHObject( 0, Box3df(Vector3df(1.0f,0.0f,0.0f), Vector3df(2.0f,1.0f,1.0f)) ));

    BVH bvh(objs, /*leafMax=*/1);

    // クエリは x=[2,3] で境界 x=2 に接する -> intersects は true のはず
    Box3df query(Vector3df(2.0f,0.0f,0.0f), Vector3df(3.0f,1.0f,1.0f));
    auto hits = bvh.queryOverlaps(query);

    EXPECT_EQ(1u, hits.size());
    EXPECT_EQ(0, hits.front()->id);

    for (auto obj : objs) {
        delete obj;
    }

}