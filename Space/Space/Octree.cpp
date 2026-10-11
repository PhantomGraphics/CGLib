#include "Octree.h"
#include "BoxQueries.h"

#include <algorithm>

using namespace Phantom::Math;
using namespace Phantom::Space;

bool Octree::add(ITreeItem* item)
{
	if (item == nullptr) {
		return false;
	}
	constexpr float tol = 1.0e-12f;
	const auto itemBox = item->getBox();

	// A box with NaN/Inf corners cannot be placed (comparisons are all false), so reject it up front.
	if (!Validation::isFinite(itemBox.getMin()) || !Validation::isFinite(itemBox.getMax())) {
		return false;
	}

	// アイテムがこのノードの領域と交差しなければ無視する
	if (!box.intersects(itemBox)) {
		return false;
	}

	// 葉ノードならまずこのノードに格納できるか確認
	if (children[0] == nullptr) {
		// 完全にこのボックスに含まれるならこのノードに格納
		if (box.contains(itemBox, tol)) {
			items.push_back(item);
			return true;
		}
		// そうでなければ子を作成して配置を試みる
		createChildren();
	}

	// 子ノードのうち「完全に含む」ものがあればその子へ再帰的に追加
	for (int i = 0; i < 8; ++i) {
		const auto& child = children[i];
		if (child == nullptr) continue;
		if (child->getBox().contains(itemBox, tol)) {
			return child->add(item);
		}
	}

	// どの子にも完全に含まれない（複数子にまたがるなど）場合はこのノードに保持する
	items.push_back(item);
	return true;
}

std::size_t Octree::countItems() const
{
	std::size_t count = items.size();
	for (const auto& child : children) {
		if (child != nullptr) {
			count += child->countItems();
		}
	}
	return count;
}

void Octree::createChildren()
{
	// 既に子がある場合は何もしない
	if (children[0] != nullptr) return;

	int index = 0;
	const auto length = box.getLength() * 0.5f;
	for (int i = 0; i < 2; ++i) {
		for (int j = 0; j < 2; ++j) {
			for (int k = 0; k < 2; ++k) {
				const auto v1 = box.getPosition(i * 0.5f, j * 0.5f, k * 0.5f);
				const auto v2 = v1 + length;
				Box3df childBox(v1, v2);
				auto tree = std::make_unique<Octree>(childBox);
				children[index] = std::move(tree);
				index++;
			}
		}
	}

	// 既にこのノードにある items を可能なら子に降ろす
	constexpr float tol = 1.0e-12f;
	for (auto it = items.begin(); it != items.end(); ) {
		ITreeItem* item = *it;
		const auto ibox = item->getBox();
		bool moved = false;
		for (int i = 0; i < 8; ++i) {
			auto& child = children[i];
			if (child && child->getBox().contains(ibox, tol)) {
				// 子が完全に含むなら子へ移動
				child->add(item);
				it = items.erase(it);
				moved = true;
				break;
			}
		}
		if (!moved) {
			++it;
		}
	}
}

std::list<ITreeItem*> Octree::findItems(const Math::Vector3df& position, const float searchRadius) const
{
	std::list<ITreeItem*> result;
	constexpr float tol = 1.0e-12f;
	const float r = searchRadius;
	if (!(r >= 0.0f) || !Validation::isFinite(position)) {
		return result;
	}
	const float r2 = r * r;

	auto boxIntersectsSphere = [&](const Box3df& b) -> bool {
		return BoxQueries::squaredDistanceToBox(position, b) <= (r2 + tol);
	};

	// このノードのボックス自体が検索球と交差しなければ探索しない
	if (!boxIntersectsSphere(box)) {
		return result;
	}

	// このノードにあるアイテムを検査
	for (auto item : items) {
		const auto ibox = item->getBox();
		if (boxIntersectsSphere(ibox)) {
			result.push_back(item);
		}
	}

	// 子ノードを再帰探索（子のボックスが球と交差する場合のみ）
	for (const auto& child : children) {
		if (child == nullptr) continue;
		if (boxIntersectsSphere(child->getBox())) {
			auto childItems = child->findItems(position, searchRadius);
			result.splice(result.end(), childItems);
		}
	}

	return result;
}


std::list<ITreeItem*> Octree::findItems(const Box3df& box) const
{
	std::list<ITreeItem*> result;

	// ノード領域と検索ボックスが交差しなければ早期リターン
	if (!this->box.intersects(box)) {
		return result;
	}

	// このノードに保持しているアイテムをチェック
	for (auto item : items) {
		const auto ibox = item->getBox();
		if (ibox.intersects(box)) {
			result.push_back(item);
		}
	}

	// 子ノードを再帰検索（子ボックスと検索ボックスが交差する場合のみ）
	for (const auto& child : children) {
		if (child == nullptr) continue;
		if (child->getBox().intersects(box)) {
			auto childItems = child->findItems(box);
			result.splice(result.end(), childItems);
		}
	}

	return result;
}
