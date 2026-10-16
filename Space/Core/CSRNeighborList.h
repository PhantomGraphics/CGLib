#pragma once

#include "IndexedSortBasedSearcher.h"
#include "NeighborIndexView.h"

#include "CGLib/Math/Core/Vector3d.h"
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
		 *
		 * Ownership: the list owns its rows (`offsets` + `indices`) and hands out borrowed
		 * NeighborIndexView rows. Rebuilding replaces the rows wholesale and reuses both the row
		 * storage and the internal search scratch, so calling build() every step is cheap; the row
		 * order does not depend on that reuse (it is the pair-generation order, which the SPH solvers'
		 * summation order -- and therefore their bit-exact determinism -- relies on).
		 */
		class CSRNeighborList
		{
		public:
			/// Kept as a member alias for call sites written against the old nested name.
			using View = NeighborIndexView;

			/**
			 * @brief Runs the neighbor search over `positions` and flattens the result.
			 * @param positions    Particle positions; a row index is an index into this.
			 * @param effectLength Search radius. Non-positive, NaN or infinite yields rows that are all
			 *                     empty (rather than a division by zero in cell binning). A position that
			 *                     is NaN/Inf or beyond the cell range gets an empty row and appears in
			 *                     nobody else's row. Use tryBuild() to be told about such input.
			 */
			void build(const std::vector<Math::Vector3df>& positions, const float effectLength);

			/**
			 * @brief build() that reports invalid input instead of silently producing empty rows.
			 * @return false -- leaving this list *empty* (size() == 0) -- if `effectLength` is not finite
			 *         and positive, any position is rejected (NaN/Inf or beyond +-2^30 cells), or the
			 *         particle/edge count does not fit the int offsets. true otherwise; the result is
			 *         then identical to build().
			 */
			bool tryBuild(const std::vector<Math::Vector3df>& positions, const float effectLength);

			/**
			 * @brief Flattens an already-computed, unordered pair list.
			 *
			 * Both directions of every pair are stored, so pass each unordered
			 * pair exactly once (which is what IndexedSortBasedSearcher emits).
			 *
			 * Preconditions, *unchecked* here for speed: every index is in [0, particleCount), no pair is
			 * (i, i), no unordered pair appears twice, and 2 * pairs.size() fits an int. A violation is
			 * out-of-range memory access (bad index) or a doubled / self entry in a row (the others). Use
			 * tryBuild(particleCount, pairs) for pairs that did not come from IndexedSortBasedSearcher.
			 */
			void build(const size_t particleCount, const std::vector<ParticlePair>& pairs);

			/**
			 * @brief build(particleCount, pairs) with the preconditions checked.
			 *
			 * Rejects out-of-range indices, self pairs, duplicate pairs (in either orientation) and a
			 * particle/edge count that would overflow the int offsets. Costs an extra O(E log E) pass, so
			 * keep build() for the hot path.
			 * @return false -- leaving this list empty (size() == 0) -- if any check fails.
			 */
			bool tryBuild(const size_t particleCount, const std::vector<ParticlePair>& pairs);

			/// Number of rows, i.e. the particle count the list was built for.
			size_t size() const { return offsets.empty() ? 0 : offsets.size() - 1; }

			/// Drops all rows (size() == 0). Invalidates every NeighborIndexView obtained so far. The
			/// scratch storage is kept for the next build; see releaseScratch().
			void clear();

			/// Frees the scratch storage that build() keeps between calls (sorted particles, pair
			/// lists). The rows themselves are untouched. Call it to give memory back after a one-off
			/// large build; the next build() simply allocates again.
			void releaseScratch();

			/**
			 * @brief Row `particleIndex`, with the index checked.
			 * @return The row, or an empty view if `particleIndex >= size()` (including an unbuilt or
			 *         cleared list).
			 */
			NeighborIndexView at(const size_t particleIndex) const
			{
				return particleIndex < size() ? (*this)[particleIndex] : NeighborIndexView{};
			}

			/**
			 * @brief Row `particleIndex`. Requires `particleIndex < size()` (unchecked; see at()).
			 *
			 * The view borrows this list's storage and is invalidated by the next build(), tryBuild() or
			 * clear() -- and by destroying the list. Rebuilding is wholesale, never in place, so a view is
			 * only meant to live within one pass over the list. An empty row is a valid view with
			 * begin() == end().
			 */
			NeighborIndexView operator[](const size_t particleIndex) const
			{
				return NeighborIndexView{
					indices.data() + offsets[particleIndex],
					indices.data() + offsets[particleIndex + 1]
				};
			}

		private:
			// Scratch for build(): kept between calls so a per-step rebuild reuses its allocations instead
			// of making a few hundred per call. Copying or assigning a list gives the copy *empty*
			// scratch -- it is capacity only, never part of the list's value. build() writes to it, so one
			// list must not be built from two threads at once (reading rows concurrently is fine);
			// separate lists never share scratch.
			struct Scratch : NeighborSearchWorkspace
			{
				Scratch() = default;
				Scratch(const Scratch&) {}
				Scratch& operator=(const Scratch&) { return *this; }
				Scratch(Scratch&&) = default;
				Scratch& operator=(Scratch&&) = default;
			};

			std::vector<int> offsets;
			std::vector<int> indices;
			Scratch scratch;
		};

	}
}
