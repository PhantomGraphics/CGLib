#include "Octree.h"

#include <algorithm>

using namespace Phantom::Math;
using namespace Phantom::Space;

void Octree::add(ITreeItem* item)
{
	constexpr float tol = 1.0e-12f;
	const auto itemBox = item->getBox();

	// アイテムがこのノードの領域と交差しなければ無視する
	if (!box.intersects(itemBox)) {
		return;
	}

	// 葉ノードならまずこのノードに格納できるか確認
	if (children[0] == nullptr) {
		// 完全にこのボックスに含まれるならこのノードに格納
		if (box.contains(itemBox, tol)) {
			items.push_back(item);
			return;
		}
		// そうでなければ子を作成して配置を試みる
		createChildren();
	}

	// 子ノードのうち「完全に含む」ものがあればその子へ再帰的に追加
	for (int i = 0; i < 8; ++i) {
		const auto& child = children[i];
		if (child == nullptr) continue;
		if (child->getBox().contains(itemBox, tol)) {
			child->add(item);
			return;
		}
	}

	// どの子にも完全に含まれない（複数子にまたがるなど）場合はこのノードに保持する
	items.push_back(item);
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
	const float r2 = r * r;

	auto boxIntersectsSphere = [&](const Box3df& b) -> bool {
		const auto bmin = b.getMin();
		const auto bmax = b.getMax();
		// 最近接点を箱の内部にクランプ
		float cx = position.x;
		if (cx < bmin.x) cx = bmin.x;
		if (cx > bmax.x) cx = bmax.x;
		float cy = position.y;
		if (cy < bmin.y) cy = bmin.y;
		if (cy > bmax.y) cy = bmax.y;
		float cz = position.z;
		if (cz < bmin.z) cz = bmin.z;
		if (cz > bmax.z) cz = bmax.z;
		const float dx = position.x - cx;
		const float dy = position.y - cy;
		const float dz = position.z - cz;
		const float dist2 = dx * dx + dy * dy + dz * dz;
		return dist2 <= (r2 + tol);
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
