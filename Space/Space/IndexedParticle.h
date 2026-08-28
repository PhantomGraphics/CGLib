#pragma once

#include "CGLib/Math/Vector3d.h"
#include <array>

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
			IndexedParticle() :
				position(0,0,0),
				gridId(0),
				id(0)
			{}

			/**
			 * @brief Constructs a particle at the given 3D position.
			 * @param position The world-space position of the particle.
			 */
			explicit IndexedParticle(const Math::Vector3df& position) :
				position(position),
				gridId(0),
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
			 * @param effectLength The side length of one spatial cell (search radius).
			 */
			void setGridId(const float effectLength);

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
			static int toGridId(const Math::Vector3df& pos, const float effectLength);

			/**
			 * @brief Converts a 3D position to a 3D integer grid index.
			 * @param pos          World position.
			 * @param effectLength Cell side length.
			 * @return 3D grid index [ix, iy, iz].
			 */
			static std::array<int, 3> toIndex(const Math::Vector3df& pos, const float effectLength);

			/**
			 * @brief Returns the precomputed scalar grid ID.
			 * @return Grid ID value (set by setGridId()).
			 */
			int getGridId() const { return gridId; }

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
			static int toIdX(std::array<int, 3> index);

		private:
			Math::Vector3df position;
			int gridId;
			int id;
		};

	}
}
