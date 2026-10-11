#pragma once

#include "CGLib/Util/UnCopyable.h"
#include "CGLib/Math/Vector3d.h"

#include "ITreeItem.h"
#include "SpaceValidation.h"

#include <vector>

namespace Phantom {
	namespace Space {

		/**
		 * @brief K-D Tree for efficient 3D nearest-neighbor and radius queries.
		 *
		 * Build the tree by calling addPoint() for each point (or build() directly with a
		 * Vector3dfVector), then calling build(). After build(), the tree supports:
		 * - Nearest-neighbor search (findNearest, findNearestIndex)
		 * - Range search within a given radius (findWithinRadius)
		 *
		 * Positions are stored by value in a Math::Vector3dfVector; the tree owns its own
		 * copy, so callers do not need to keep any source object alive after build().
		 *
		 * Internally, the tree cycles through X/Y/Z axes at successive depths
		 * and uses nth_element-based balanced partitioning (O(n log n) build).
		 *
		 * Lifecycle (staging vs built):
		 * - addPoint() only *stages* a point (appends to the position list). The queries see the points
		 *   present at the last build(); a point added afterwards is invisible until the next build().
		 *   Queries never read past what was built, so a stale tree is wrong-but-safe. isDirty() tells
		 *   whether staged points are waiting.
		 * - An index is a position in the staged list (insertion order). A non-finite point (NaN/Inf)
		 *   keeps its index but is left out of the tree -- sorting NaN coordinates would break the
		 *   partitioning -- and is counted by rejectedCount().
		 * - build() always rebuilds from scratch; clear() empties everything.
		 * - A query at a non-finite position returns nothing (-1 / nullptr / empty).
		 *
		 * @note This class is non-copyable.
		 */
		class KDTree : private UnCopyable
		{
		public:
			/// A k-NN candidate: squared distance and point index. Public only so that KNearestScratch
			/// can hold a heap of them.
			struct Candidate
			{
				float dist2 = 0.0f;
				int index = -1;
			};

			/**
			 * @brief Reusable heap storage for findKNearestIndices(query, k, out, scratch).
			 *
			 * One scratch per thread: queries that run concurrently must not share one. Contents are
			 * meaningless between calls; only the capacity is reused.
			 */
			struct KNearestScratch
			{
				std::vector<Candidate> heap;
			};

			KDTree() = default;
			~KDTree() = default;

			/**
			 * @brief Registers a point position to be included in the next build() call.
			 *
			 * Invalidates every pointer returned by findNearest() (the position list may reallocate).
			 * @param position The point position to add.
			 * @return false if the position is non-finite: it is still staged (keeping its index) but
			 *         build() will leave it out of the tree.
			 */
			bool addPoint(const Math::Vector3df& position);

			/**
			 * @brief Constructs the KD-Tree from all points added via addPoint().
			 *
			 * Must be called after all points have been added and before any queries.
			 * Rebuilds the tree from scratch each time it is invoked. Invalidates every pointer returned
			 * by findNearest().
			 */
			void build();

			/// Number of points in the built tree (finite points staged at the last build()).
			size_t size() const { return nodes.size(); }

			/// Number of staged points, including ones not built yet and non-finite ones.
			size_t pointCount() const { return positions.size(); }

			/// True if points were staged (addPoint) after the last build(), i.e. queries are stale.
			bool isDirty() const { return positions.size() != builtPointCount; }

			/// Non-finite points left out by the last build().
			size_t rejectedCount() const { return rejectedPointCount; }

			/**
			 * @brief Replaces the held positions with `positions` and builds the KD-Tree from
			 * them in one step. Equivalent to clear() + addPoint() for each entry + build(), but
			 * avoids the per-point call overhead when the caller already holds a flat vector.
			 *
			 * @param positions The point positions to index. Query results return indices into
			 * this same order.
			 */
			void build(const Math::Vector3dfVector& positions);

			/**
			 * @brief Removes all points and resets the tree.
			 */
			void clear();

			/**
			 * @brief Finds the nearest point to the given query position.
			 * @param query The query position in 3D space.
			 * @return Pointer to the closest position (into internal storage), or nullptr if the
			 * tree is empty. Valid until the next build()/clear() call.
			 */
			const Math::Vector3df* findNearest(const Math::Vector3df& query) const;

			/**
			 * @brief Finds the index of the nearest point to the given query position.
			 *
			 * The index corresponds to the insertion order in addPoint().
			 *
			 * @param query The query position in 3D space.
			 * @return Index of the closest point, or -1 if the tree is empty.
			 */
			int findNearestIndex(const Math::Vector3df& query) const;

			/**
			 * @brief Returns the indices of all points within a given radius.
			 *
			 * Requires build() to have been called beforehand.
			 *
			 * @param query  The center of the search sphere.
			 * @param radius The search radius. Negative radii return no points;
			 *               zero includes points exactly at the query position.
			 * @return Vector of indices (insertion order) of all points within the radius. The order is the
			 *         tree traversal order -- deterministic for a given tree, but not sorted.
			 */
			std::vector<int> findWithinRadius(const Math::Vector3df& query, const float radius) const;

			/**
			 * @brief findWithinRadius() into a caller-owned buffer.
			 *
			 * `out` is cleared and then filled with exactly the indices (and in exactly the order) the
			 * returning overload would give; its capacity is reused, so calling this in a loop with one
			 * buffer allocates only while the buffer is still growing.
			 */
			void findWithinRadius(const Math::Vector3df& query, const float radius, std::vector<int>& out) const;

			/**
			 * @brief Calls `visit(int index)` for every point within `radius` (inclusive), without
			 * building a result container. Same points and same order as findWithinRadius().
			 *
			 * `visit` must not modify the tree (build()/addPoint()/clear()). Negative or NaN radius, or an
			 * empty tree, visit nothing.
			 */
			template <typename Visitor>
			void forEachWithinRadius(const Math::Vector3df& query, const float radius, Visitor&& visit) const
			{
				if (rootIndex < 0 || !(radius >= 0.0f) || !Validation::isFinite(query)) return;
				radiusVisit(rootIndex, query, radius * radius, visit);
			}

			/**
			 * @brief Finds the indices of the k nearest points to the query position.
			 *
			 * Requires build() to have been called beforehand. If the tree holds fewer than k
			 * points, all of them are returned. Results are sorted by ascending distance.
			 *
			 * @param query The query position in 3D space.
			 * @param k     The maximum number of neighbors to return.
			 * @return Indices (insertion order) of the k nearest points, nearest first.
			 */
			std::vector<int> findKNearestIndices(const Math::Vector3df& query, const size_t k) const;

			/**
			 * @brief findKNearestIndices() into a caller-owned buffer, with caller-owned heap storage.
			 *
			 * `out` is cleared and filled with the same indices in the same order as the returning
			 * overload. Reusing `out` and `scratch` across calls removes the per-query allocations.
			 */
			void findKNearestIndices(const Math::Vector3df& query, const size_t k, std::vector<int>& out, KNearestScratch& scratch) const;

		private:
			// Flat, index-based node storage (contiguous std::vector<Node>) instead of a tree of
			// individually heap-allocated Node objects: avoids pointer-chasing across scattered
			// allocations during traversal, which profiling showed was the dominant cost of
			// neighbor search (see internal design notes). -1 means "no
			// child". `positions` doubles as the staging buffer (appended to by addPoint()) and the
			// build()-time snapshot indexed by nodes: no virtual dispatch and no separate item
			// storage, just a flat value copy.
			struct Node
			{
				int index = -1;
				int axis = 0;
				int left = -1;
				int right = -1;
			};

			int buildRecursive(std::vector<int>& indices, int begin, int end, int depth);
			void nearestRecursive(int nodeIndex, const Math::Vector3df& query, int& bestIndex, float& bestDist2) const;

			template <typename Visitor>
			void radiusVisit(int nodeIndex, const Math::Vector3df& query, float radius2, Visitor& visit) const
			{
				if (nodeIndex < 0) return;
				const Node& node = nodes[nodeIndex];

				const Math::Vector3df& p = positions[node.index];
				if (Math::getDistanceSquared(p, query) <= radius2) visit(node.index);

				const int axis = node.axis;
				const float diff = query[axis] - p[axis];
				const float diff2 = diff * diff;

				// Near side first; the far side only if the splitting plane is within the radius.
				if (diff <= 0.0f) {
					if (node.left >= 0) radiusVisit(node.left, query, radius2, visit);
					if (node.right >= 0 && diff2 <= radius2) radiusVisit(node.right, query, radius2, visit);
				}
				else {
					if (node.right >= 0) radiusVisit(node.right, query, radius2, visit);
					if (node.left >= 0 && diff2 <= radius2) radiusVisit(node.left, query, radius2, visit);
				}
			}
			void kNearestRecursive(int nodeIndex, const Math::Vector3df& query, size_t k, std::vector<Candidate>& heap) const;

			Math::Vector3dfVector positions;
			std::vector<Node> nodes;
			int rootIndex = -1;
			size_t builtPointCount = 0;
			size_t rejectedPointCount = 0;
		};
	}
}
