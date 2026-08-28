#include "KDTree.h"

#include <algorithm>
#include <limits>
#include <cmath>

using namespace Phantom::Math;
using namespace Phantom::Space;

void KDTree::addPoint(const Math::Vector3df& position)
{
	positions.push_back(position);
}

void KDTree::clear()
{
	positions.clear();
	nodes.clear();
	rootIndex = -1;
}

int KDTree::buildRecursive(std::vector<int>& indices, int begin, int end, int depth)
{
	if (begin >= end) return -1;
	const int len = end - begin;
	const int axis = depth % 3;
	const int mid = begin + len / 2;

	// nth_element selects the median
	std::nth_element(indices.begin() + begin, indices.begin() + mid, indices.begin() + end,
		[&](int a, int b) {
			return positions[a][axis] < positions[b][axis];
		});

	const int nodeIndex = static_cast<int>(nodes.size());
	nodes.push_back(Node{});
	nodes[nodeIndex].axis = axis;
	nodes[nodeIndex].index = indices[mid];

	const int leftIndex = buildRecursive(indices, begin, mid, depth + 1);
	const int rightIndex = buildRecursive(indices, mid + 1, end, depth + 1);
	// Re-index rather than hold a reference across the recursive calls above: nodes.reserve()
	// in build() means no reallocation actually happens, but indexing stays correct either way.
	nodes[nodeIndex].left = leftIndex;
	nodes[nodeIndex].right = rightIndex;

	return nodeIndex;
}

void KDTree::build(const Math::Vector3dfVector& newPositions)
{
	positions = newPositions;
	build();
}

void KDTree::build()
{
	nodes.clear();
	rootIndex = -1;

	if (positions.empty()) {
		return;
	}

	nodes.reserve(positions.size());

	std::vector<int> indices(positions.size());
	for (size_t i = 0; i < positions.size(); ++i) indices[i] = static_cast<int>(i);

	rootIndex = buildRecursive(indices, 0, static_cast<int>(indices.size()), 0);
}

int KDTree::findNearestIndex(const Math::Vector3df& query) const
{
	if (rootIndex < 0) return -1;
	int bestIndex = -1;
	float bestDist2 = std::numeric_limits<float>::infinity();
	nearestRecursive(rootIndex, query, bestIndex, bestDist2);
	return bestIndex;
}

const Vector3df* KDTree::findNearest(const Math::Vector3df& query) const
{
	const int idx = findNearestIndex(query);
	if (idx < 0) {
		return nullptr;
	}
	return &positions[idx];
}

void KDTree::nearestRecursive(int nodeIndex, const Math::Vector3df& query, int& bestIndex, float& bestDist2) const
{
	if (nodeIndex < 0) return;
	const Node& node = nodes[nodeIndex];

	const Math::Vector3df& p = positions[node.index];
	const float d2 = getDistanceSquared(p, query);
	if (d2 < bestDist2) {
		bestDist2 = d2;
		bestIndex = node.index;
	}

	const int axis = node.axis;
	const float diff = query[axis] - p[axis];

	const int first = diff <= 0.0f ? node.left : node.right;
	const int second = diff <= 0.0f ? node.right : node.left;

	if (first >= 0) nearestRecursive(first, query, bestIndex, bestDist2);

	// prune based on distance to the splitting plane
	const float diff2 = diff * diff;
	if (second >= 0 && diff2 < bestDist2) {
		nearestRecursive(second, query, bestIndex, bestDist2);
	}
}

std::vector<int> KDTree::findKNearestIndices(const Math::Vector3df& query, const size_t k) const
{
	std::vector<int> result;
	if (rootIndex < 0 || k == 0) return result;

	const auto less = [](const Candidate& a, const Candidate& b) { return a.dist2 < b.dist2; };

	std::vector<Candidate> heap;
	heap.reserve(k);
	kNearestRecursive(rootIndex, query, k, heap);

	// heap is a max-heap (w.r.t. dist2) of the k best candidates found so far;
	// sort_heap turns it into an ascending-by-dist2 range, i.e. nearest first.
	std::sort_heap(heap.begin(), heap.end(), less);

	result.reserve(heap.size());
	for (const auto& c : heap) result.push_back(c.index);
	return result;
}

void KDTree::kNearestRecursive(int nodeIndex, const Math::Vector3df& query, size_t k, std::vector<Candidate>& heap) const
{
	if (nodeIndex < 0) return;
	const Node& node = nodes[nodeIndex];

	const auto less = [](const Candidate& a, const Candidate& b) { return a.dist2 < b.dist2; };

	const Math::Vector3df& p = positions[node.index];
	const float d2 = getDistanceSquared(p, query);

	if (heap.size() < k) {
		heap.push_back({ d2, node.index });
		std::push_heap(heap.begin(), heap.end(), less);
	}
	else if (d2 < heap.front().dist2) {
		std::pop_heap(heap.begin(), heap.end(), less);
		heap.back() = { d2, node.index };
		std::push_heap(heap.begin(), heap.end(), less);
	}

	const int axis = node.axis;
	const float diff = query[axis] - p[axis];

	const int first = diff <= 0.0f ? node.left : node.right;
	const int second = diff <= 0.0f ? node.right : node.left;

	if (first >= 0) kNearestRecursive(first, query, k, heap);

	// prune based on distance to the splitting plane: only recurse into the second side if
	// fewer than k candidates have been found yet, or it could still beat the current worst
	const float diff2 = diff * diff;
	if (second >= 0 && (heap.size() < k || diff2 < heap.front().dist2)) {
		kNearestRecursive(second, query, k, heap);
	}
}

std::vector<int> KDTree::findWithinRadius(const Math::Vector3df& query, const float radius) const
{
	std::vector<int> result;
	if (rootIndex < 0) return result;
	const float radius2 = radius * radius;
	radiusRecursive(rootIndex, query, radius2, result);
	return result;
}

void KDTree::radiusRecursive(int nodeIndex, const Math::Vector3df& query, float radius2, std::vector<int>& result) const
{
	if (nodeIndex < 0) return;
	const Node& node = nodes[nodeIndex];

	const Math::Vector3df& p = positions[node.index];
	const float d2 = getDistanceSquared(p, query);
	if (d2 <= radius2) result.push_back(node.index);

	const int axis = node.axis;
	const float diff = query[axis] - p[axis];
	const float diff2 = diff * diff;

	if (diff <= 0.0f) {
		if (node.left >= 0) radiusRecursive(node.left, query, radius2, result);
		if (node.right >= 0 && diff2 <= radius2) radiusRecursive(node.right, query, radius2, result);
	}
	else {
		if (node.right >= 0) radiusRecursive(node.right, query, radius2, result);
		if (node.left >= 0 && diff2 <= radius2) radiusRecursive(node.left, query, radius2, result);
	}
}
