#pragma once

#include <utility>

namespace Phantom {
	namespace Space {

		/**
		 * @brief Compact representation of a node position in a linear (implicit) octree.
		 *
		 * A linear octree encodes tree nodes as single integers by combining the node's
		 * depth level and its position number at that level:
		 *
		 *   index1d = (8^0 + 8^1 + ... + 8^(level-1)) + number
		 *
		 * This encoding allows octree nodes to be stored in flat arrays, sets, or maps
		 * without explicit pointers, while still supporting parent/child traversal.
		 *
		 * Supports equality and less-than comparison for use in ordered containers.
		 */
		class LinearOctreeIndex
		{
		public:
			/**
			 * @brief Constructs a LinearOctreeIndex directly from a 1D encoded value.
			 * @param index1d The pre-encoded 1D index.
			 */
			explicit LinearOctreeIndex(const unsigned int index1d) :
				index1d(index1d)
			{}

			/**
			 * @brief Constructs a LinearOctreeIndex from a depth level and node number.
			 * @param level  Depth level in the octree (root = level 0).
			 * @param number The node's sequential number at the given level.
			 */
			LinearOctreeIndex(const unsigned int level, const unsigned int number);

			/**
			 * @brief Decodes the 1D index back into (level, number).
			 * @return Pair of (level, number) for this node.
			 */
			std::pair<unsigned int, unsigned int> getLevelAndNumber() const;

			/**
			 * @brief Returns the LinearOctreeIndex of this node's parent.
			 * @return Index of the parent node.
			 */
			LinearOctreeIndex getParentIndex() const;

			/**
			 * @brief Returns the raw 1D encoded index.
			 * @return Unsigned integer 1D index.
			 */
			unsigned int getIndex1d() const;

			/**
			 * @brief Equality comparison by 1D index value.
			 */
			bool operator==(const LinearOctreeIndex& rhs) const { return this->index1d == rhs.index1d; }

			/**
			 * @brief Less-than comparison by 1D index value (enables use in std::set/std::map).
			 */
			bool operator<(const LinearOctreeIndex& rhs) const { return this->index1d < rhs.index1d; }

		private:
			unsigned int index1d;
		};

	}
}
