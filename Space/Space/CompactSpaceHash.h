#pragma once

#include "CGLib/Util/UnCopyable.h"
#include "CGLib/Math/Math/Vector3d.h"
#include "ZOrderCurve3d.h"
#include "SpaceValidation.h"
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
			/// Z-order (Morton) encoded cell identifier. Morton codes keep 10 bits per axis, so cells whose
			/// coordinates are outside 0..1023 (e.g. cells 0 and 2048, or negative cells) can share a cellId; the cell's identity
			/// is `index`, never `cellId`.
			unsigned int cellId;
			/// 3D integer grid index [ix, iy, iz] of this cell (its identity).
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
			/// Default state: cell size 1, one hash bucket -- usable, but call setup() to size it.
			CompactSpaceHash();

			/**
			 * @brief Constructs a CompactSpaceHash with the given cell size and table capacity.
			 * @param divideLength  Side length of each cubic spatial cell. Must be finite and positive;
			 *                      otherwise every add() is rejected and every query is empty.
			 * @param tableSize     Number of hash buckets (values < 1 are raised to 1).
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
			 * @brief Removes all stored positions and cells. The cell size and the number of buckets are
			 * kept, so the hash can be refilled right away.
			 */
			void clear();

			/**
			 * @brief Inserts a position into the hash.
			 *
			 * The position is assigned an index equal to the current insertion count (0-based).
			 *
			 * An unusable position (NaN/Inf, or a cell coordinate beyond +-2^30) is rejected: it still
			 * consumes its index, so later indices are unchanged, but it belongs to no cell and is never
			 * returned by a query.
			 *
			 * @param position The 3D world position to insert.
			 * @return false if the position (or the cell size) was rejected.
			 */
			bool add(const Math::Vector3df& position);

			/**
			 * @brief Removes the position at the given index from its cell.
			 *
			 * The index is not reused and the stored position stays (only the cell membership goes).
			 * @param index Insertion-order index of the position to remove.
			 * @return false if `index` is out of range, was rejected by add(), or was already removed.
			 */
			bool remove(const int index);

			/**
			 * @brief Returns the particles strictly closer than `divideLength` to the given particle.
			 *
			 * Scans the 27 surrounding cells and keeps `distance^2 < divideLength^2`; the particle itself
			 * is excluded. Order: cells in (dx, dy, dz) loop order, insertion order within a cell; no
			 * duplicates.
			 * @param index Insertion-order index of the reference particle. Out of range or rejected
			 *              input yields an empty vector.
			 */
			std::vector<int> findNeighborIndices(const int index) const;

			/**
			 * @brief Returns the particles strictly closer than `divideLength` to the given position.
			 *
			 * Same scan and order as the by-index overload, but a stored particle at `position` is
			 * included. A non-finite or out-of-range position yields an empty vector.
			 * @param position The query world position.
			 */
			std::vector<int> findNeighborIndices(const Math::Vector3df& position) const;

			/**
			 * @brief findNeighborIndices() into a caller-owned buffer.
			 *
			 * `out` is cleared and filled with exactly the indices, in exactly the order, the returning
			 * overloads give; its capacity is reused, so a loop over all particles with one buffer
			 * allocates only while the buffer is still growing. Invalid input leaves `out` empty.
			 */
			void findNeighborIndices(const int index, std::vector<int>& out) const;
			void findNeighborIndices(const Math::Vector3df& position, std::vector<int>& out) const;

			/**
			 * @brief Calls `visit(int index)` for every neighbor, without building a container.
			 *
			 * Same particles and same order as findNeighborIndices() (the by-index overload skips the
			 * particle itself). `visit` must not modify the hash (add/remove/clear/setup). Invalid input
			 * visits nothing.
			 */
			template <typename Visitor>
			void forEachNeighbor(const int index, Visitor&& visit) const
			{
				std::array<int, 3> cell;
				if (index < 0 || static_cast<size_t>(index) >= positions.size() || !tryToIndex(positions[index], cell)) return;
				visitNeighbors(cell, positions[index], index, visit);
			}

			template <typename Visitor>
			void forEachNeighbor(const Math::Vector3df& position, Visitor&& visit) const
			{
				std::array<int, 3> cell;
				if (!tryToIndex(position, cell)) return;
				visitNeighbors(cell, position, -1, visit);
			}

			/**
			 * @brief Returns the particle indices stored in the cell containing the given particle.
			 * @param index Insertion-order index of the reference particle (out of range -> empty).
			 * @return Every particle in that cell (no distance filter), in insertion order.
			 */
			std::vector<int> find(const int index) const;

			/**
			 * @brief Returns the particle indices stored in the cell at the given 3D grid index.
			 * @param position 3D integer grid index [ix, iy, iz].
			 * @return Vector of particle indices in that cell.
			 */
			std::vector<int> find(const std::array<int, 3>& position) const;

			/**
			 * @brief Converts a world position to a 3D integer grid index (truncation toward zero).
			 *
			 * Undefined for non-finite or out-of-int-range input; add() and the queries validate first.
			 * @param pos World position.
			 * @return 3D grid index [ix, iy, iz].
			 */
			std::array<int, 3> toIndex(const Math::Vector3df& pos) const;

			/**
			 * @brief Encodes a 3D grid index to a Z-order (Morton) curve value (10 bits per axis; larger
			 * and negative coordinates may alias -- not an identity, see CompactSpaceCell::cellId).
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
			// Cell lookup helper: the cell with grid index `ix` in bucket toHash(ix), or nullptr.
			CompactSpaceCell* findCell(const std::array<int, 3>& ix) const;

			bool tryToIndex(const Math::Vector3df& pos, std::array<int, 3>& index) const;

			// The 27-cell scan shared by every neighbor query: cells in (dx, dy, dz) loop order, insertion
			// order within a cell, `d^2 < divideLength^2`, `skipIndex` (-1: none) left out.
			template <typename Visitor>
			void visitNeighbors(const std::array<int, 3>& center, const Math::Vector3df& position, const int skipIndex, Visitor& visit) const
			{
				const double radius2 = divideLength * divideLength;
				for (int i = -1; i <= 1; ++i) {
					for (int j = -1; j <= 1; ++j) {
						for (int k = -1; k <= 1; ++k) {
							const std::array<int, 3> ix{ i + center[0], j + center[1], k + center[2] };
							const auto* cell = findCell(ix);
							if (cell == nullptr) {
								continue;
							}
							for (const int other : cell->particleIndices) {
								if (other == skipIndex) {
									continue;
								}
								const double d2 = Math::getDistanceSquared(position, positions[other]);
								if (d2 < radius2) {
									visit(other);
								}
							}
						}
					}
				}
			}

			std::vector<std::vector<CompactSpaceCell*>> table;
			std::vector<CompactSpaceCell*> cells;
			std::vector<Math::Vector3df> positions;

			unsigned int toHash(const Math::Vector3df& pos) const;

			unsigned int toHash(const std::array<int, 3>& index) const;

			double divideLength = 1.0;

			ZOrderCurve3d zCurve;
		};

	}
}
