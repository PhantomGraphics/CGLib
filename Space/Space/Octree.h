#pragma once

#include "CGLib/Util/UnCopyable.h"
#include "CGLib/Math/Math/Box3d.h"
#include "ITreeItem.h"
#include "SpaceValidation.h"

#include <array>
#include <list>
#include <vector>
#include <memory>
#include <cstddef>

namespace Phantom {
	namespace Space {

		/**
		 * @brief Octree for 3D spatial partitioning.
		 *
		 * Recursively subdivides 3D space into up to 8 child nodes (octants).
		 * Supports insertion of ITreeItem objects and spatial range queries
		 * by sphere radius or axis-aligned bounding box (AABB).
		 *
		 * Subdivision happens only when an item is inserted that intersects this node's box but is not
		 * fully inside it: the node then gets 8 children, and items (existing and new) that a child
		 * fully contains move into that child. An item that fits a node completely is simply stored
		 * there. As a consequence the tree is at most **one level deep** (root plus 8 octants); it does
		 * not refine further however many items are added. It is a coarse partition, not an adaptive
		 * octree, and the query cost grows with the number of items kept at a node.
		 *
		 * Ownership and lifetime of items:
		 * - Items are *not owned* and *not copied*: the tree keeps ITreeItem pointers. Every item must
		 *   outlive the tree (or at least every query that could reach it).
		 * - The same pointer added twice is stored twice (no de-duplication) and is returned twice.
		 * - An item whose box does not intersect the root box is ignored (add() returns false).
		 * - The tree caches no boxes: queries call item->getBox() again, but the node an item sits in
		 *   was chosen at add() time. If an item's box changes after insertion, results may miss it or
		 *   report it from a node that no longer contains it -- build a new Octree after moving items.
		 * - There is no remove(); rebuild instead.
		 *
		 * @note This class is non-copyable.
		 */
		class Octree : private UnCopyable
		{
		public:
			Octree() = default;

			/**
			 * @brief Constructs an Octree node with the given bounding box.
			 * @param box The axis-aligned bounding box defining this node's spatial region.
			 */
			explicit Octree(const Math::Box3df& box) :
				box(box)
			{}

			/**
			 * @brief Inserts an item into the octree.
			 *
			 * If this node has children, the item is forwarded to the appropriate child.
			 * Otherwise, it is stored at this node. When the node becomes too full,
			 * children are created and items are redistributed.
			 *
			 * @param item Pointer to the item to insert. The caller retains ownership.
			 * @return false if nothing was stored: `item` is null, its box is not finite, or the box does
			 *         not intersect this node's box.
			 */
			bool add(ITreeItem* item);

			/**
			 * @brief Subdivides this node into 8 child octants.
			 *
			 * Creates 8 child Octree nodes whose bounding boxes partition this node's space.
			 * Items already in this node are redistributed into the appropriate children.
			 */
			void createChildren();

			/**
			 * @brief Returns the array of child octants.
			 * @return Const reference to the array of 8 unique_ptr<Octree> children.
			 *         Entries may be null if the node has not been subdivided.
			 */
			const std::array<std::unique_ptr<Octree>, 8>& getChildren() const { return children; }

			/**
			 * @brief Returns the bounding box of this node.
			 * @return The axis-aligned bounding box (AABB) of this octree node.
			 */
			Math::Box3df getBox() const { return box; }

			/**
			 * @brief Finds all items within a spherical region.
			 * @param position  Center of the search sphere in 3D space.
			 * @param searchRadius  Radius of the search sphere. Negative or NaN yields an empty list.
			 * @return List of pointers to items whose bounding boxes intersect the sphere, in tree order
			 *         (this node's items, then each child's) -- deterministic, not sorted.
			 */
			std::list<ITreeItem*> findItems(const Math::Vector3df& position, const float searchRadius) const;

			/**
			 * @brief Finds all items that intersect the given axis-aligned bounding box.
			 * @param box The query AABB.
			 * @return List of pointers to items whose bounding boxes overlap the query box.
			 */
			std::list<ITreeItem*> findItems(const Math::Box3df& box) const;

			/**
			 * @brief Returns the number of items stored directly in this node (not in children).
			 * @return Item count at this node.
			 */
			std::size_t getItemsCount() const { return items.size(); }

			/// Items stored in this node and all of its descendants (duplicates counted).
			std::size_t countItems() const;

		private:
			std::array<std::unique_ptr<Octree>, 8> children;
			std::list<ITreeItem*> items;
			Math::Box3df box;
		};
	}
}
