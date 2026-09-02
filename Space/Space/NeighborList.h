#pragma once

#include "IndexedSortBasedSearcher.h"
#include "NeighborIndexView.h"

#include "CGLib/Math/Vector3d.h"
#include <cstddef>
#include <vector>

namespace Phantom {
	namespace Space {

		/**
		 * @brief Flattened (CSR) adjacency list of fixed-radius neighbors.
		 *
		 * Equivalent to a std::vector<std::vector<int>> holding, for each
		 * particle, the indices of every other particle within the search
		 * radius -- but as two flat arrays (offsets + indices) instead of one
		 * separately heap-allocated std::vector<int> per particle. The
		 * per-particle allocations dominate cost when the list is rebuilt every
		 * simulation step (see internal design notes
		 * section 4).
		 *
		 * This is the single entry point every SPH solver in Phantom::Physics
		 * uses for neighbor search: build(positions, effectLength) runs the
		 * search and flattens the result in one call, so no solver has to
		 * repeat the searcher setup boilerplate (or pick its own searcher).
		 *
		 * Consuming a row yields *symmetric* neighbors: if j appears in row i
		 * then i appears in row j. That is what lets the solvers parallelize
		 * their density/force passes over particles ("gather") rather than over
		 * pairs -- with each thread writing only to its own particle's
		 * accumulators, instead of two threads racing on a shared one.
		 *
		 * Rows exclude the particle itself, and are distance-filtered: only
		 * particles strictly closer than the search radius appear.
		 */
		class CSRNeighborList
		{
		public:
			/// Kept as a member alias for call sites written against the old nested name.
			using View = NeighborIndexView;

			/**
			 * @brief Runs the neighbor search over `positions` and flattens the result.
			 * @param positions    Particle positions; a row index is an index into this.
			 * @param effectLength Search radius. Non-positive yields an empty list
			 *                     (rather than a division by zero in cell binning).
			 */
			void build(const std::vector<Math::Vector3df>& positions, const float effectLength);

			/**
			 * @brief Flattens an already-computed, unordered pair list.
			 *
			 * Both directions of every pair are stored, so pass each unordered
			 * pair exactly once (which is what IndexedSortBasedSearcher emits).
			 */
			void build(const size_t particleCount, const std::vector<ParticlePair>& pairs);

			/// Number of rows, i.e. the particle count the list was built for.
			size_t size() const { return offsets.empty() ? 0 : offsets.size() - 1; }

			void clear();

			NeighborIndexView operator[](const size_t particleIndex) const
			{
				return NeighborIndexView{
					indices.data() + offsets[particleIndex],
					indices.data() + offsets[particleIndex + 1]
				};
			}

		private:
			std::vector<int> offsets;
			std::vector<int> indices;
		};

	}
}
