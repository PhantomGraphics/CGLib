#pragma once

#include "../../../CGLib/Util/UnCopyable.h"
#include "../../../CGLib/Math/Vector3d.h"
#include "ZOrderCurve3d.h"
#include <vector>
#include <list>
#include <functional>

namespace Phantom {
	namespace Space {

		/**
		 * @brief A single cell in a CompactSpaceHash grid.
		 *
		 * Stores the Z-order (Morton) encoded cell ID, the 3D grid index of the cell,
		 * and the list of particle indices whose positions map to this cell.
		 */
		class CompactSpaceCell
		{
		public:
			/// Z-order (Morton) encoded cell identifier.
			unsigned int cellId;
			/// 3D integer grid index [ix, iy, iz] of this cell.
			std::array<int, 3> index;
			/// Indices (insertion order) of particles located in this cell.
			std::vector<int> particleIndices;
		};

		/**
		 * @brief Memory-efficient spatial hash using Z-order (Morton) curve encoding.
		 *
		 * Similar to SpaceHash, but cells are identified via Z-order curve values
		 * for improved spatial locality and compact storage.
		 *
		 * Supports:
		 * - Insertion and removal of positions by index.
		 * - Neighbor queries by position or by particle index.
		 * - Coordinate conversion between world positions, 3D grid indices, and Z-order values.
		 *
		 * The hash function maps Z-order cell IDs to hash buckets, reducing cache misses
		 * for spatially adjacent cells.
		 *
		 * @note This class is non-copyable.
		 */
		class CompactSpaceHash : private UnCopyable
		{
		public:
			CompactSpaceHash();

			/**
			 * @brief Constructs a CompactSpaceHash with the given cell size and table capacity.
			 * @param divideLength  Side length of each cubic spatial cell.
			 * @param tableSize     Number of hash buckets.
			 */
			CompactSpaceHash(const double divideLength, const int tableSize);

			~CompactSpaceHash();

			/**
			 * @brief Re-initializes the hash with new parameters, clearing all stored data.
			 * @param divideLength  New cell side length.
			 * @param tableSize     New number of hash buckets.
			 */
			void setup(const double divideLength, const int tableSize);

			/**
			 * @brief Removes all stored positions and resets internal state.
			 */
			void clear();

			/**
			 * @brief Inserts a position into the hash.
			 *
			 * The position is assigned an index equal to the current insertion count (0-based).
			 *
			 * @param position The 3D world position to insert.
			 */
			void add(const Math::Vector3df& position);

			/**
			 * @brief Removes the position at the given index from its cell.
			 * @param index Insertion-order index of the position to remove.
			 */
			void remove(const int index);

			/**
			 * @brief Returns the indices of positions in cells neighboring the given particle.
			 * @param index Insertion-order index of the reference particle.
			 * @return Vector of indices of particles in the 27 neighboring cells.
			 */
			std::vector<int> findNeighborIndices(const int index);

			/**
			 * @brief Returns the indices of positions in cells neighboring the given position.
			 * @param position The query world position.
			 * @return Vector of indices of particles in the 27 neighboring cells.
			 */
			std::vector<int> findNeighborIndices(const Math::Vector3df& position);

			/**
			 * @brief Returns the particle indices stored in the cell containing the given particle.
			 * @param index Insertion-order index of the reference particle.
			 * @return Vector of particle indices in the same cell.
			 */
			std::vector<int> find(const int index) const;

			/**
			 * @brief Returns the particle indices stored in the cell at the given 3D grid index.
			 * @param position 3D integer grid index [ix, iy, iz].
			 * @return Vector of particle indices in that cell.
			 */
			std::vector<int> find(const std::array<int, 3>& position) const;

			/**
			 * @brief Converts a world position to a 3D integer grid index.
			 * @param pos World position.
			 * @return 3D grid index [ix, iy, iz].
			 */
			std::array<int, 3> toIndex(const Math::Vector3df& pos) const;

			/**
			 * @brief Encodes a 3D grid index to a Z-order (Morton) curve value.
			 * @param index 3D integer grid index [ix, iy, iz].
			 * @return Morton-encoded unsigned integer.
			 */
			unsigned int toZIndex(const std::array<int, 3>& index) const;

			/**
			 * @brief Decodes a Z-order (Morton) value back to a 3D unsigned integer index.
			 * @param index Morton-encoded value.
			 * @return Decoded 3D unsigned integer grid index [ix, iy, iz].
			 */
			std::array<unsigned int, 3> fromZIndex(unsigned int index) const;

			/**
			 * @brief Converts a 3D integer grid index to a world-space position (cell center).
			 * @param index 3D integer grid index [ix, iy, iz].
			 * @return Corresponding world position.
			 */
			Math::Vector3df toPosition(const std::array<int, 3>& index) const;

			/**
			 * @brief Checks whether the cell containing the given position has no particles.
			 * @param pos World position to check.
			 * @return true if the cell is empty, false otherwise.
			 */
			bool isEmpty(const Math::Vector3df& pos) const;

		private:
			std::vector<std::vector<CompactSpaceCell*>> table;
			std::vector<CompactSpaceCell*> cells;
			std::vector<Math::Vector3df> positions;

			unsigned int toHash(const Math::Vector3df& pos) const;

			unsigned int toHash(const std::array<int, 3>& index) const;

			double divideLength;

			ZOrderCurve3d zCurve;
		};

	}
}
