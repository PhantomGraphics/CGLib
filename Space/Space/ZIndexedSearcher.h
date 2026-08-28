#pragma once

#include "CGLib/Math/Vector3d.h"

#include "CGLib/Util/UnCopyable.h"

#include "ZOrderCurve3d.h"

#include <vector>
#include <list>

namespace Phantom {
	namespace Space {

		/**
		 * @brief A particle tagged with a Z-order (Morton) index for sorted spatial queries.
		 *
		 * Wraps a position and its corresponding Z-order curve value.
		 * Sorting a collection of ZIndexedParticle by zIndex groups spatially nearby
		 * particles together in memory, improving cache performance during neighborhood searches.
		 */
		class ZIndexedParticle
		{
		public:
			/**
			 * @brief Constructs a ZIndexedParticle with the given Z-order index and position.
			 * @param zIndex   Z-order (Morton) encoded spatial index.
			 * @param position World-space position of the particle.
			 */
			ZIndexedParticle(const unsigned int zIndex, const Math::Vector3df& position) :
				zIndex(zIndex),
				position(position)
			{}

			/**
			 * @brief Less-than comparison by Z-order index, enabling std::sort.
			 */
			bool operator<(const ZIndexedParticle& rhs) const {
				return this->zIndex < rhs.zIndex;
			}

			/**
			 * @brief Returns the world-space position of this particle.
			 * @return Position as Vector3df.
			 */
			Math::Vector3df getPosition() { return position; }

			/// Application-defined index (set externally after insertion).
			int index;

		private:
			unsigned int zIndex;
			Math::Vector3df position;
		};

		/**
		 * @brief Spatial neighbor search using Z-order (Morton) sorted particles.
		 *
		 * Adds particles, sorts them by Z-order index, and queries neighbors
		 * within the configured search radius by scanning the sorted array.
		 * Spatial locality of the Z-order curve reduces unnecessary comparisons.
		 *
		 * Typical workflow:
		 * 1. Construct with searchRadius and a minimum reference position.
		 * 2. Call add() for each particle.
		 * 3. Call sort() once.
		 * 4. Call findNeighbors() for each query position.
		 *
		 * @note This class is non-copyable.
		 */
		class ZIndexedSearcher : private UnCopyable
		{
		public:
			/**
			 * @brief Constructs a ZIndexedSearcher.
			 * @param searchRadius  Maximum distance for a particle to be considered a neighbor.
			 * @param minPosition   The minimum world-space position used as the grid origin.
			 */
			explicit ZIndexedSearcher(const float searchRadius, const Math::Vector3df& minPosition);

			/**
			 * @brief Adds a position to the searcher.
			 *
			 * Computes the Z-order index from the position and stores the particle internally.
			 *
			 * @param position World-space position to add.
			 */
			void add(const Math::Vector3df& position);

			/**
			 * @brief Sorts all added particles by their Z-order index.
			 *
			 * Must be called after all add() calls and before any findNeighbors() call.
			 */
			void sort();

			/**
			 * @brief Returns the insertion-order indices of particles near the given position.
			 * @param position The query position in world space.
			 * @return List of insertion-order indices of neighboring particles.
			 */
			std::list<int> findNeighbors(const Math::Vector3df& position);

			/**
			 * @brief Converts a world position to a 3D unsigned integer grid index.
			 * @param position World-space position.
			 * @return 3D grid index [ix, iy, iz] relative to minPosition.
			 */
			std::array<unsigned int, 3> toIndex(const Math::Vector3df& position) const;

		private:
			float searchRadius;
			Math::Vector3df minPosition;
			std::vector<ZIndexedParticle> points;
			ZOrderCurve3d curve;
		};

	}
}
