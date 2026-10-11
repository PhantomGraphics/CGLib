#pragma once

#include "BoxQueries.h"

#include <algorithm>
#include <array>
#include <utility>
#include <vector>
#include "CGLib/Math/Box3d.h"


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
		 * Ownership, ids and updates:
		 * - The BVH does not take ownership of BVHObject pointers and does not copy the objects. They
		 *   must stay alive, and at the same address, for as long as the BVH is queried. A null pointer
		 *   in the input is skipped (counted by nullObjectCount()).
		 * - Object ids should be unique (hasUniqueIds()). findAllPairs() reports a pair only when
		 *   `idA < idB`, so two objects sharing an id never form a pair. queryOverlaps/queryRay do not
		 *   look at ids.
		 * - Moving objects: change `BVHObject::box`, then call refit() (cheap, keeps the topology, loose
		 *   boxes if objects moved far) or rebuild() (re-splits from the current boxes). Queries made
		 *   between moving a box and refit()/rebuild() can miss objects. The set of objects cannot change
		 *   after construction: construct a new BVH to add or remove objects.
		 * - refitDegradation() measures how much looser the boxes got since the last (re)build; use it to
		 *   decide between the two.
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
			 * @brief Re-splits the tree from the objects' *current* boxes (same object set, same leafMax).
			 *
			 * Use it instead of refit() when objects moved a lot (see refitDegradation()). O(n log n).
			 */
			void rebuild();

			/// Number of objects in the tree (null input pointers excluded).
			size_t objectCount() const { return m_objects.size(); }

			/// Null pointers skipped by the constructor.
			size_t nullObjectCount() const { return m_nullObjects; }

			/// True if no two objects share an id (checked at construction and by rebuild()).
			bool hasUniqueIds() const { return m_uniqueIds; }

			/**
			 * @brief How much the tree's total box surface area has grown since the last (re)build.
			 *
			 * 1.0 right after construction/rebuild(). refit() keeps the topology, so when objects drift
			 * far from where they were split, boxes overlap more, queries visit more nodes and this value
			 * rises. A value well above ~2 means rebuild() is probably worthwhile (a rule of thumb, not a
			 * guarantee -- measure your own workload). O(nodes).
			 */
			float refitDegradation() const;

			/**
			 * @brief Returns all objects whose AABB overlaps the query box.
			 * @param query The axis-aligned bounding box to test against.
			 * @return Vector of pointers to overlapping BVHObject instances.
			 */
            std::vector<BVHObject*> queryOverlaps(const Math::Box3df& query) const;

			/**
			 * @brief queryOverlaps() into a caller-owned buffer (cleared first, capacity reused).
			 *
			 * Same objects in the same order as the returning overload. The traversal uses a fixed-size
			 * stack, so a loop of queries over one buffer allocates only while the buffer still grows.
			 */
			void queryOverlaps(const Math::Box3df& query, std::vector<BVHObject*>& out) const;

			/**
			 * @brief Calls `visit(BVHObject*)` for every object whose box overlaps `query`, in the same
			 * order as queryOverlaps(), without building a container. `visit` must not modify the BVH
			 * structure; moving an object's box is fine as long as refit() is called before the next query.
			 */
			template <typename Visitor>
			void forEachOverlap(const Math::Box3df& query, Visitor&& visit) const
			{
				if (m_nodes.empty()) return;
				std::array<int, kMaxTraversalDepth> stack;
				int top = 0;
				stack[top++] = 0;
				while (top > 0) {
					const BVHNode& node = m_nodes[stack[--top]];
					if (!node.box.intersects(query)) continue;
					if (node.isLeaf()) {
						for (int i = 0; i < node.count; ++i) {
							BVHObject* obj = m_objects[m_indices[node.first + i]];
							if (obj->box.intersects(query)) {
								visit(obj);
							}
						}
					}
					else {
						stack[top++] = node.left;
						stack[top++] = node.right;
					}
				}
			}

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

			/// queryRay() into a caller-owned buffer (cleared first, capacity reused); same objects, same order.
			void queryRay(
				const Math::Vector3df& origin,
				const Math::Vector3df& direction,
				std::vector<BVHObject*>& out,
				float tMin = 1e-4f,
				float tMax = 1e38f) const;

			/// Calls `visit(BVHObject*)` for every ray candidate, in the order queryRay() returns them.
			template <typename Visitor>
			void forEachRayCandidate(
				const Math::Vector3df& origin,
				const Math::Vector3df& direction,
				float tMin,
				float tMax,
				Visitor&& visit) const
			{
				if (m_nodes.empty()) return;
				std::array<int, kMaxTraversalDepth> stack;
				int top = 0;
				stack[top++] = 0;
				while (top > 0) {
					const BVHNode& node = m_nodes[stack[--top]];
					if (!rayHitsBox(node.box, origin, direction, tMin, tMax)) continue;
					if (node.isLeaf()) {
						for (int i = 0; i < node.count; ++i) {
							BVHObject* obj = m_objects[m_indices[node.first + i]];
							if (rayHitsBox(obj->box, origin, direction, tMin, tMax)) {
								visit(obj);
							}
						}
					}
					else {
						stack[top++] = node.left;
						stack[top++] = node.right;
					}
				}
			}

			/**
			 * @brief Enumerates all pairs of objects whose AABBs overlap.
			 *
			 * Uses the BVH hierarchy for O(n log n) average-case performance.
			 *
			 * @return Vector of (idA, idB) pairs where idA < idB.
			 */
            std::vector<std::pair<int, int>> findAllPairs() const;

			/**
			 * @brief findAllPairs() into caller-owned buffers: `out` is cleared and receives the pairs
			 * (same pairs, same order); `stack` is the traversal work list, cleared on entry. Keep both
			 * across calls (e.g. one pair per simulation step) and the steady state allocates nothing.
			 */
			void findAllPairs(std::vector<std::pair<int, int>>& out, std::vector<std::pair<int, int>>& stack) const;

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
			// The tree splits at the median, so its depth is at most ~log2(n) + 1 (< 33 for any int-sized
			// n); a DFS that pushes both children holds at most depth + 1 entries. 64 leaves ample headroom.
			static constexpr int kMaxTraversalDepth = 64;

			static bool rayHitsBox(const Math::Box3df& box, const Math::Vector3df& origin, const Math::Vector3df& dir, float tMin, float tMax)
			{
				return BoxQueries::rayHitsBox(box, origin, dir, tMin, tMax);
			}

			float totalSurfaceArea() const;
			void buildFromObjects();

            std::vector<BVHObject*> m_objects;
            std::vector<int> m_indices;
            std::vector<BVHNode> m_nodes;
            int m_leafMax;
			size_t m_nullObjects = 0;
			bool m_uniqueIds = true;
			float m_buildSurfaceArea = 0.0f;

            int buildRecursive(int begin, int end);

            Math::Box3df refitRecursive(int nodeIndex);
        };

	}
}
