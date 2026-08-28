#pragma once

#include "CGLib/Util/UnCopyable.h"
#include "CGLib/Math/Box3d.h"
#include "ITreeItem.h"

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
		 * Child nodes are created lazily 窶・subdivision occurs only when items are inserted.
		 * Once children are created, existing items in the node are redistributed
		 * into the appropriate child octants.
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
			 */
			void add(ITreeItem* item);

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
			 * @param searchRadius  Radius of the search sphere.
			 * @return List of pointers to items whose bounding boxes intersect the sphere.
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

		private:
			std::array<std::unique_ptr<Octree>, 8> children;
			std::list<ITreeItem*> items;
			Math::Box3df box;
		};
	}
}
