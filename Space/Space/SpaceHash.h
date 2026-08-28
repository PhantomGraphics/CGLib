#pragma once

#include "CGLib/Util/UnCopyable.h"
#include "CGLib/Math/Vector3d.h"
#include <list>
#include <array>
#include <vector>

namespace Phantom {
	namespace Space {

		/**
		 * @brief Spatial hash table for fast 3D neighborhood lookups.
		 *
		 * Discretizes 3D space into uniform cubic cells of size `divideLength`.
		 * Each position is mapped to a hash bucket via three spatial prime multipliers,
		 * enabling O(1) average-case insertion and neighbor queries.
		 *
		 * Best suited for point clouds with roughly uniform density.
		 * For non-uniform data or memory-efficient storage, prefer CompactSpaceHash.
		 *
		 * Hash function: `(ix*p1) XOR (iy*p2) XOR (iz*p3)` modulo tableSize,
		 * where p1=73856093, p2=19349663, p3=83492791.
		 *
		 * @note This class is non-copyable.
		 */
		class SpaceHash : private UnCopyable
		{
		public:
			/**
			 * @brief Constructs a SpaceHash with the given cell size and table capacity.
			 * @param divideLength  Side length of each cubic spatial cell.
			 * @param tableSize     Number of hash buckets in the table.
			 */
			SpaceHash(const double divideLength, const int tableSize);

			/**
			 * @brief Inserts a position into the hash table.
			 *
			 * The position is stored and its index (insertion order, 0-based) is recorded
			 * in the appropriate hash bucket.
			 *
			 * @param position The 3D position to insert.
			 */
			void add(const Math::Vector3df& position);

			/**
			 * @brief Returns the indices of all positions in cells neighboring the query position.
			 *
			 * Checks the 27 cells (3x3x3 cube) surrounding the cell that contains `position`.
			 *
			 * @param position The query position.
			 * @return List of insertion-order indices of positions in the neighboring cells.
			 */
			std::list<int> findNeighborIndices(const Math::Vector3df& position);

		private:
			std::vector<std::list<int>> table;

			std::vector<Math::Vector3df> positions;

			int toHash(const Math::Vector3df& pos) const;

			int toHash(const std::array<int, 3>& index) const;

			std::array<int, 3> toIndex(const Math::Vector3df& pos) const;

			const double divideLength;
		};

	}
}
