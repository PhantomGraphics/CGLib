#pragma once

#include "CGLib/Math/Box3d.h"

namespace Phantom
{
	namespace Space {

		/**
		 * @brief Interface for items stored in tree-based spatial structures (e.g., Octree).
		 *
		 * Any object that needs to be inserted into a spatial tree must implement this interface
		 * by providing an axis-aligned bounding box (AABB) that encloses the object.
		 */
		class ITreeItem
		{
		public:
			virtual ~ITreeItem() = default;

			/**
			 * @brief Returns the axis-aligned bounding box (AABB) of this item.
			 * @return A Box3d<float> that tightly encloses the item in 3D space.
			 */
			virtual Math::Box3d<float> getBox() = 0;
		};
	}
}
