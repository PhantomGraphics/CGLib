#include "gtest/gtest.h"
#include "../Space/Octree.h"

using namespace Phantom::Math;
using namespace Phantom::Space;

namespace {

	// テスト用の ITreeItem 実装
	class TestItem : public ITreeItem
	{
	public:
		explicit TestItem(const Box3df& b) : box(b) {}
		Box3df getBox() override { return box; }
	private:
		Box3df box;
	};
}

TEST(OctreeTest, TestCreateChildren)
{
	const Box3df box(Vector3df(0.0f, 0.0f, 0.0f), Vector3df(10.0f, 10.0f, 10.0f));
	Octree tree(box);
	tree.createChildren();

	const auto& children = tree.getChildren();

	const Box3df expected0(Vector3df(0.0f, 0.0f, 0.0f), Vector3df(5.0f, 5.0f, 5.0f));
	EXPECT_TRUE(expected0.isSame( children[0]->getBox(), 1.0e-6f));

	const Box3df expected1(Vector3df(0.0f, 0.0f, 5.0f), Vector3df(5.0f, 5.0f, 10.0f));
	EXPECT_TRUE(expected1.isSame(children[1]->getBox(), 1.0e-6f));
}

TEST(OctreeTest, AddItemBeforeCreateChildrenMovesWhenCreated)
{
	const Box3df box(Vector3df(0.0f,0.0f,0.0f), Vector3df(10.0f,10.0f,10.0f));
	Octree tree(box);

	TestItem item(Box3df(Vector3df(1.0f,1.0f,1.0f), Vector3df(2.0f,2.0f,2.0f)));
	tree.add(&item);

	// 子を作る前はルートにアイテムが入る
	EXPECT_EQ(tree.getItemsCount(), 1u);

	// 子を作ると、可能なら子に降ろされる
	tree.createChildren();
	const auto& children = tree.getChildren();
	EXPECT_EQ(tree.getItemsCount(), 0u);
	EXPECT_EQ(children[0]->getItemsCount(), 1u); // (1,1,1) は children[0] に属する
}

TEST(OctreeTest, AddItemToChild)
{
	const Box3df box(Vector3df(0.0f,0.0f,0.0f), Vector3df(10.0f,10.0f,10.0f));
	Octree tree(box);
	tree.createChildren();
	const auto& children = tree.getChildren();

	TestItem item(Box3df(Vector3df(1.0f,1.0f,1.0f), Vector3df(2.0f,2.0f,2.0f)));
	tree.add(&item);

	// ルートには残らず、子に格納される
	EXPECT_EQ(tree.getItemsCount(), 0u);
	EXPECT_EQ(children[0]->getItemsCount(), 1u);
}

TEST(OctreeTest, AddItemSpanningChildrenRemainsInParent)
{
	const Box3df box(Vector3df(0.0f,0.0f,0.0f), Vector3df(10.0f,10.0f,10.0f));
	Octree tree(box);
	tree.createChildren();
	const auto& children = tree.getChildren();

	// 中心をまたぐ小箱 -> どの子にも完全には入らないのでルートに残る
	TestItem spanning(Box3df(Vector3df(4.5f,4.5f,4.5f), Vector3df(5.5f,5.5f,5.5f)));
	tree.add(&spanning);

	EXPECT_EQ(tree.getItemsCount(), 1u);
	for (int i = 0; i < 8; ++i) {
		if (children[i]) {
			EXPECT_EQ(children[i]->getItemsCount(), 0u);
		}
	}
}

TEST(OctreeTest, AddItemOutsideIgnored)
{
	const Box3df box(Vector3df(0.0f,0.0f,0.0f), Vector3df(10.0f,10.0f,10.0f));
	Octree tree(box);

	TestItem outside(Box3df(Vector3df(20.0f,20.0f,20.0f), Vector3df(21.0f,21.0f,21.0f)));
	tree.add(&outside);

	// 領域外は無視される
	EXPECT_EQ(tree.getItemsCount(), 0u);
}

TEST(OctreeTest, FindSingleItemInside)
{
	Box3df root(Vector3df(0.0f, 0.0f, 0.0f), Vector3df(8.0f, 8.0f, 8.0f));
	Octree tree(root);

	auto item = std::make_unique<TestItem>(Box3df(Vector3df(1.0f, 1.0f, 1.0f), Vector3df(2.0f, 2.0f, 2.0f)));
	TestItem* p = item.get();
	tree.add(p);

	// 検索点がアイテムの内部、半径0 で見つかること
	auto res = tree.findItems(Vector3df(1.5f, 1.5f, 1.5f), 0.0f);
	ASSERT_EQ(1u, res.size());
	EXPECT_EQ(p, res.front());
}

TEST(OctreeTest, NotFoundOutsideRoot)
{
	Box3df root(Vector3df(0.0f, 0.0f, 0.0f), Vector3df(4.0f, 4.0f, 4.0f));
	Octree tree(root);

	auto item = std::make_unique<TestItem>(Box3df(Vector3df(5.0f, 5.0f, 5.0f), Vector3df(6.0f, 6.0f, 6.0f)));
	TestItem* p = item.get();
	tree.add(p);

	// 検索球がルート領域と交差しない -> 見つからない
	auto res = tree.findItems(Vector3df(0.0f, 0.0f, 0.0f), 1.0f);
	EXPECT_TRUE(res.empty());
}

TEST(OctreeTest, FindItemsAcrossChildren)
{
	Box3df root(Vector3df(0.0f, 0.0f, 0.0f), Vector3df(8.0f, 8.0f, 8.0f));
	Octree tree(root);

	std::vector<std::unique_ptr<TestItem>> items;
	items.emplace_back(std::make_unique<TestItem>(Box3df(Vector3df(1, 1, 1), Vector3df(2, 2, 2)))); // child A
	items.emplace_back(std::make_unique<TestItem>(Box3df(Vector3df(6, 6, 6), Vector3df(7, 7, 7)))); // child B
	items.emplace_back(std::make_unique<TestItem>(Box3df(Vector3df(3.5f, 3.5f, 3.5f), Vector3df(4.5f, 4.5f, 4.5f)))); // center

	for (auto& it : items) tree.add(it.get());

	// 中央付近を半径1で検索 -> 中央のアイテムは必ず見つかる。離れたものは見つからない。
	auto res = tree.findItems(Vector3df(4.0f, 4.0f, 4.0f), 1.0f);
	EXPECT_EQ(1, res.size());
	EXPECT_EQ(res.front(), items[2].get());
	//EXPECT_TRUE(containsPtr(res, items[2].get()));
	//EXPECT_FALSE(containsPtr(res, items[0].get()));
	//EXPECT_FALSE(containsPtr(res, items[1].get()));
}

TEST(OctreeTest, TouchingSphereBoundaryIsFound)
{
	Box3df root(Vector3df(0, 0, 0), Vector3df(8, 8, 8));
	Octree tree(root);

	// ボックスは x = 3..4、検索点は x = 2、半径 = 1 -> 接触する（境界上）
	auto item = std::make_unique<TestItem>(Box3df(Vector3df(3.0f, 0.0f, 0.0f), Vector3df(4.0f, 1.0f, 1.0f)));
	TestItem* p = item.get();
	tree.add(p);

	auto res = tree.findItems(Vector3df(2.0f, 0.5f, 0.5f), 1.0f);
	EXPECT_EQ(1, res.size());

	//EXPECT_TRUE(containsPtr(res, p));
}


TEST(OctreeBoxFindTest, FindSingleItemIntersect)
{
	Box3df root(Vector3df(0.0f, 0.0f, 0.0f), Vector3df(8.0f, 8.0f, 8.0f));
	Octree tree(root);

	auto item = std::make_unique<TestItem>(Box3df(Vector3df(1.0f, 1.0f, 1.0f), Vector3df(2.0f, 2.0f, 2.0f)));
	TestItem* p = item.get();
	tree.add(p);

	// 検索ボックスがアイテムと重なる -> 見つかる
	Box3df searchBox(Vector3df(1.5f, 1.5f, 1.5f), Vector3df(1.6f, 1.6f, 1.6f));
	auto res = tree.findItems(searchBox);
	ASSERT_EQ(1u, res.size());
	EXPECT_EQ(p, res.front());
}

TEST(OctreeBoxFindTest, NotFoundWhenNoIntersectionWithRoot)
{
	Box3df root(Vector3df(0.0f, 0.0f, 0.0f), Vector3df(4.0f, 4.0f, 4.0f));
	Octree tree(root);

	// ルート外のアイテムは add で無視される
	auto item = std::make_unique<TestItem>(Box3df(Vector3df(5.0f, 5.0f, 5.0f), Vector3df(6.0f, 6.0f, 6.0f)));
	TestItem* p = item.get();
	tree.add(p);

	Box3df searchBox(Vector3df(0.0f, 0.0f, 0.0f), Vector3df(1.0f, 1.0f, 1.0f));
	auto res = tree.findItems(searchBox);
	EXPECT_TRUE(res.empty());
}

TEST(OctreeBoxFindTest, FindItemsAcrossChildren)
{
	Box3df root(Vector3df(0.0f, 0.0f, 0.0f), Vector3df(8.0f, 8.0f, 8.0f));
	Octree tree(root);

	std::vector<std::unique_ptr<TestItem>> items;
	items.emplace_back(std::make_unique<TestItem>(Box3df(Vector3df(1, 1, 1), Vector3df(2, 2, 2)))); // child A
	items.emplace_back(std::make_unique<TestItem>(Box3df(Vector3df(6, 6, 6), Vector3df(7, 7, 7)))); // child B
	items.emplace_back(std::make_unique<TestItem>(Box3df(Vector3df(3.5f, 3.5f, 3.5f), Vector3df(4.5f, 4.5f, 4.5f)))); // center

	for (auto& it : items) tree.add(it.get());

	// 中央近傍を検索 -> 中央のアイテムだけヒットする
	Box3df searchBox(Vector3df(3.9f, 3.9f, 3.9f), Vector3df(4.1f, 4.1f, 4.1f));
	auto res = tree.findItems(searchBox);
	EXPECT_EQ(res.size(), 1);
//	EXPECT_TRUE(containsPtr(res, items[2].get()));
//	EXPECT_FALSE(containsPtr(res, items[0].get()));
//	EXPECT_FALSE(containsPtr(res, items[1].get()));
}

/*
TEST(OctreeBoxFindTest, TouchingBoundaryIsConsideredIntersection)
{
	Box3df root(Vector3df(0, 0, 0), Vector3df(8, 8, 8));
	Octree tree(root);

	// アイテムの最小 x = 3、検索ボックスの最大 x = 3 -> 境界接触
	auto item = std::make_unique<TestItem>(Box3df(Vector3df(3.0f, 0.0f, 0.0f), Vector3df(4.0f, 1.0f, 1.0f)));
	TestItem* p = item.get();
	tree.add(p);

	Box3df searchBox(Vector3df(2.0f, 0.0f, 0.0f), Vector3df(3.0f, 1.0f, 1.0f)); // x=3 で接触
	auto res = tree.findItems(searchBox);
	EXPECT_TRUE(containsPtr(res, p));
}

TEST(OctreeBoxFindTest, LargeBoxCoversMultipleItems)
{
	Box3df root(Vector3df(0, 0, 0), Vector3df(8, 8, 8));
	Octree tree(root);

	std::vector<std::unique_ptr<TestItem>> items;
	items.emplace_back(std::make_unique<TestItem>(Box3df(Vector3df(1, 1, 1), Vector3df(2, 2, 2))));
	items.emplace_back(std::make_unique<TestItem>(Box3df(Vector3df(2.5f, 2.5f, 2.5f), Vector3df(3.5f, 3.5f, 3.5f))));
	items.emplace_back(std::make_unique<TestItem>(Box3df(Vector3df(5, 5, 5), Vector3df(6, 6, 6))));
	for (auto& it : items) tree.add(it.get());

	// 大きな検索ボックスで複数ヒット
	Box3df searchBox(Vector3df(1.0f, 1.0f, 1.0f), Vector3df(4.0f, 4.0f, 4.0f));
	auto res = tree.findItems(searchBox);
	EXPECT_TRUE(containsPtr(res, items[0].get()));
	EXPECT_TRUE(containsPtr(res, items[1].get()));
	EXPECT_FALSE(containsPtr(res, items[2].get()));
}
*/