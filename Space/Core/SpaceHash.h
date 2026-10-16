#pragma once

#include "CGLib/Util/UnCopyable.h"
#include "CGLib/Math/Core/Vector3d.h"
#include "SpaceValidation.h"
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
			 * @param divideLength  Side length of each cubic spatial cell. Must be finite and positive;
			 *                      otherwise every add() is rejected and every query is empty.
			 * @param tableSize     Number of hash buckets in the table (values < 1 are raised to 1).
			 */
			SpaceHash(const double divideLength, const int tableSize);

			/**
			 * @brief Inserts a position into the hash table.
			 *
			 * The position is stored and its index (insertion order, 0-based) is recorded
			 * in the appropriate hash bucket.
			 *
			 * An unusable position (NaN/Inf, or a cell coordinate beyond +-2^30) is rejected: it still
			 * consumes its index, so later indices are unchanged, but it is never returned by a query.
			 *
			 * @param position The 3D position to insert.
			 * @return false if the position (or the cell size) was rejected.
			 */
			bool add(const Math::Vector3df& position);

			/**
			 * @brief Returns the indices of all positions strictly closer than `divideLength` to `position`.
			 *
			 * Scans the 27 cells (3x3x3 cube) surrounding the cell that contains `position`, then keeps
			 * only positions with `distance^2 < divideLength^2` -- an exact radius result, not a candidate
			 * set. A stored position equal to `position` is included (no self-exclusion). The result is
			 * sorted ascending and has no duplicates.
			 *
			 * @param position The query position. A non-finite or out-of-range position yields an empty list.
			 * @return List of insertion-order indices.
			 */
			std::list<int> findNeighborIndices(const Math::Vector3df& position);

		private:
			std::vector<std::list<int>> table;

			std::vector<Math::Vector3df> positions;

			int toHash(const std::array<int, 3>& index) const;

			bool tryToIndex(const Math::Vector3df& pos, std::array<int, 3>& index) const;

			const double divideLength;
		};

	}
}
