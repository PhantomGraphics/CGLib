#pragma once

#include "CGLib/Math/Core/Vector3d.h"
#include <array>
#include <cstdint>

namespace Phantom {
	namespace Space {

		/**
		 * @brief A particle with a precomputed integer grid ID for sorting-based spatial search.
		 *
		 * Each particle stores a 3D position and a scalar `gridId` derived from its position
		 * and an effect (search) length. Particles can be sorted by gridId to enable efficient
		 * neighborhood searches using sorted arrays instead of hash tables.
		 *
		 * Typical workflow:
		 * 1. Create particles from positions.
		 * 2. Call setGridId(effectLength) on each.
		 * 3. Sort the container (operator< is provided).
		 * 4. Use IndexedSortBasedSearcher to find all pairs within effectLength.
		 */
		class IndexedParticle
		{
		public:
			/**
			 * @brief Scalar grid cell identifier.
			 *
			 * 64-bit so that `iz * gridStrideZ + iy * gridStrideY + ix` cannot overflow for any cell
			 * coordinate accepted by Validation::tryToCellIndex() (|coordinate| < 2^30). With the previous
			 * 32-bit id, iz >= 2048 overflowed (signed overflow, undefined behavior).
			 */
			using GridId = std::int64_t;

			IndexedParticle() :
				gridId(0),
				position(0,0,0),
				id(0)
			{}

			/**
			 * @brief Constructs a particle at the given 3D position.
			 * @param position The world-space position of the particle.
			 */
			explicit IndexedParticle(const Math::Vector3df& position) :
				gridId(0),
				position(position),
				id(0)
			{
			}

		public:
			/**
			 * @brief Bit shift applied to the Y cell index when packing a grid ID.
			 *
			 * toGridId() packs the 3D cell index as
			 * `iz * gridStrideZ + iy * gridStrideY + ix`, i.e. the packed ID is a
			 * *linear* function of the cell index. Neighbor searches rely on that
			 * linearity: a cell offset (dx, dy, dz) is simply
			 * `dz * gridStrideZ + dy * gridStrideY + dx` in ID space, which is how
			 * IndexedSortBasedSearcher derives the ID windows it scans (rather
			 * than hardcoding the resulting magic numbers).
			 */
			static constexpr int gridStrideY = 1 << 10;

			/// Bit shift applied to the Z cell index when packing a grid ID -- see gridStrideY.
			static constexpr int gridStrideZ = 1 << 20;

			/**
			 * @brief Computes and stores the scalar grid ID for this particle.
			 *
			 * The grid ID encodes the 3D grid cell (ix, iy, iz) that contains this particle
			 * into a single integer, suitable for sorting.
			 *
			 * @param effectLength The side length of one spatial cell (search radius). Must be finite and
			 *                     positive and the position finite and within range -- use trySetGridId()
			 *                     for input that is not already known to be valid.
			 */
			void setGridId(const float effectLength);

			/**
			 * @brief Validating version of setGridId().
			 * @return false (gridId unchanged) if `effectLength` is not finite and positive, or the position
			 *         is non-finite or maps to a cell coordinate outside +-2^30.
			 */
			bool trySetGridId(const float effectLength);

			/**
			 * @brief Validating version of toGridId().
			 * @return false (`out` unchanged) on the same conditions as trySetGridId().
			 */
			static bool tryToGridId(const Math::Vector3df& pos, const float effectLength, GridId& out);

			/**
			 * @brief Returns the 3D world position of this particle.
			 * @return Position as Vector3df.
			 */
			const Math::Vector3df& getPosition() const { return position; }

			/**
			 * @brief Converts a 3D position to a scalar grid ID.
			 * @param pos          World position.
			 * @param effectLength Cell side length.
			 * @return Scalar grid ID for sorting.
			 */
			static GridId toGridId(const Math::Vector3df& pos, const float effectLength);

			/**
			 * @brief Converts a 3D position to a 3D integer grid index.
			 * @param pos          World position.
			 * @param effectLength Cell side length.
			 * @return 3D grid index [ix, iy, iz] (truncation toward zero). Undefined for non-finite or
			 *         out-of-int-range input; see tryToGridId() / Validation::tryToCellIndex().
			 */
			static std::array<int, 3> toIndex(const Math::Vector3df& pos, const float effectLength);

			/**
			 * @brief Returns the precomputed scalar grid ID.
			 * @return Grid ID value (set by setGridId()).
			 */
			GridId getGridId() const { return gridId; }

			/**
			 * @brief Less-than comparison by grid ID, enabling std::sort on containers.
			 */
			bool operator<(const IndexedParticle& rhs) const {
				return this->getGridId() < rhs.getGridId();
			}

			/**
			 * @brief Sets the application-defined particle ID.
			 * @param id Integer identifier.
			 */
			void setId(const int id) { this->id = id; }

			/**
			 * @brief Returns the application-defined particle ID.
			 * @return Integer identifier.
			 */
			int getId() const { return this->id; }

		private:
			static GridId toIdX(const std::array<int, 3>& index);

		private:
			// Largest member first: GridId (8 bytes) + Vector3df (12) + id (4) packs into 24 bytes.
			GridId gridId;
			Math::Vector3df position;
			int id;
		};

	}
}
