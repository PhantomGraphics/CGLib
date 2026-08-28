#pragma once

#include "CGLib/Util/UnCopyable.h"
#include "CGLib/Math/Vector3d.h"

#include "ITreeItem.h"

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
		 * @note This class is non-copyable.
		 */
		class KDTree : private UnCopyable
		{
		public:
			KDTree() = default;
			~KDTree() = default;

			/**
			 * @brief Registers a point position to be included in the next build() call.
			 * @param position The point position to add.
			 */
			void addPoint(const Math::Vector3df& position);

			/**
			 * @brief Constructs the KD-Tree from all points added via addPoint().
			 *
			 * Must be called after all points have been added and before any queries.
			 * Rebuilds the tree from scratch each time it is invoked.
			 */
			void build();

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
			 * @param radius The search radius.
			 * @return Vector of indices (insertion order) of all points within the radius.
			 */
			std::vector<int> findWithinRadius(const Math::Vector3df& query, const float radius) const;

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

		private:
			// Flat, index-based node storage (contiguous std::vector<Node>) instead of a tree of
			// individually heap-allocated Node objects: avoids pointer-chasing across scattered
			// allocations during traversal, which profiling showed was the dominant cost of
			// neighbor search (see docs/todo/PLAN_pointcloud_feature_gap_analysis.md). -1 means "no
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

			struct Candidate
			{
				float dist2 = 0.0f;
				int index = -1;
			};

			int buildRecursive(std::vector<int>& indices, int begin, int end, int depth);
			void nearestRecursive(int nodeIndex, const Math::Vector3df& query, int& bestIndex, float& bestDist2) const;
			void radiusRecursive(int nodeIndex, const Math::Vector3df& query, float radius2, std::vector<int>& result) const;
			void kNearestRecursive(int nodeIndex, const Math::Vector3df& query, size_t k, std::vector<Candidate>& heap) const;

			Math::Vector3dfVector positions;
			std::vector<Node> nodes;
			int rootIndex = -1;
		};
	}
}
