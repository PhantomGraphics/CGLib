#pragma once

#include <vector>
#include "../../../CGLib/Math/Box3d.h"


namespace Phantom {
	namespace Space {

		/**
		 * @brief An object stored in a Bounding Volume Hierarchy (BVH).
		 *
		 * Wraps a user-supplied integer ID and an axis-aligned bounding box (AABB).
		 * Derive from this class or use it directly when constructing a BVH.
		 */
        class BVHObject {
        public:
			/**
			 * @brief Constructs a BVHObject with the given ID and bounding box.
			 * @param id  Application-defined integer identifier for this object.
			 * @param box Axis-aligned bounding box enclosing the object.
			 */
			BVHObject(const int id, const Math::Box3df& box) : id(id), box(box) {}

            virtual ~BVHObject() = default;

			/// Application-defined unique identifier.
			int id = -1;
			/// Axis-aligned bounding box (AABB) of this object.
            Math::Box3df box;
        };


		/**
		 * @brief Internal node of a Bounding Volume Hierarchy tree.
		 *
		 * - **Internal node**: `left` and `right` hold child node indices; `count == 0`.
		 * - **Leaf node**: `first` and `count` define the range `[first, first+count)` of
		 *   object indices stored in this leaf; `left` and `right` are unused.
		 */
        struct BVHNode {
			/// Axis-aligned bounding box enclosing all objects under this node.
            Math::Box3df box;
			/// Index of the left child node (-1 if leaf).
            int left = -1;
			/// Index of the right child node (-1 if leaf).
            int right = -1;
			/// Index into the object-index array for the first object in this leaf.
            int first = -1;
			/// Number of objects in this leaf (0 for internal nodes).
            int count = 0;

			/**
			 * @brief Returns true if this node is a leaf (holds objects directly).
			 */
            bool isLeaf() const { return count > 0; }
        };

		/**
		 * @brief Bounding Volume Hierarchy (BVH) for fast overlap and collision queries.
		 *
		 * Builds a binary tree over a set of BVHObject instances, where each node stores
		 * a tight AABB around its subtree. Supports:
		 * - **Overlap queries**: find all objects overlapping a query AABB.
		 * - **All-pairs overlap**: enumerate all overlapping object pairs (O(n log n)).
		 * - **Refit**: update bounding boxes after objects have moved (topology unchanged).
		 *
		 * The BVH does not take ownership of BVHObject pointers.
		 * Objects must remain valid for the lifetime of the BVH.
		 */
        class BVH {
        public:
			/**
			 * @brief Constructs and builds a BVH over the given set of objects.
			 * @param objects  Pointers to the objects to include in the hierarchy.
			 * @param leafMax  Maximum number of objects per leaf node (default: 4).
			 */
            explicit BVH(const std::vector<BVHObject*>& objects, int leafMax = 4);

			/**
			 * @brief Returns all objects whose AABB overlaps the query box.
			 * @param query The axis-aligned bounding box to test against.
			 * @return Vector of pointers to overlapping BVHObject instances.
			 */
            std::vector<BVHObject*> queryOverlaps(const Math::Box3df& query) const;

			/**
			 * @brief Returns all objects potentially intersected by a ray (slab test).
			 * @param origin    Ray origin.
			 * @param direction Ray direction (need not be normalized).
			 * @param tMin      Minimum ray parameter (default 1e-4).
			 * @param tMax      Maximum ray parameter (default 1e38).
			 * @return Vector of pointers to candidate BVHObject instances.
			 */
            std::vector<BVHObject*> queryRay(
                const Math::Vector3df& origin,
                const Math::Vector3df& direction,
                float tMin = 1e-4f,
                float tMax = 1e38f) const;

			/**
			 * @brief Enumerates all pairs of objects whose AABBs overlap.
			 *
			 * Uses the BVH hierarchy for O(n log n) average-case performance.
			 *
			 * @return Vector of (idA, idB) pairs where idA < idB.
			 */
            std::vector<std::pair<int, int>> findAllPairs() const;

			/**
			 * @brief Refits all node bounding boxes after objects have moved.
			 *
			 * Recomputes AABBs bottom-up without changing the tree topology.
			 * Call this after modifying BVHObject::box values.
			 */
            void refit() {
                if (m_nodes.empty()) return;
                refitRecursive(0);
            }

			/**
			 * @brief Returns the total number of nodes in the BVH tree.
			 * @return Node count (useful for diagnostics and debugging).
			 */
            int nodeCount() const { return (int)m_nodes.size(); }

        private:
            std::vector<BVHObject*> m_objects;
            std::vector<int> m_indices;
            std::vector<BVHNode> m_nodes;
            int m_leafMax;

            int buildRecursive(int begin, int end);

            Math::Box3df refitRecursive(int nodeIndex);
        };

	}
}
