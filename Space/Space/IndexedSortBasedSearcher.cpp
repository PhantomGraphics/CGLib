#include "IndexedSortBasedSearcher.h"
#include "IndexedParticle.h"

#include <algorithm>
#include <iterator>

#ifdef _OPENMP
#include <omp.h>
#endif

using namespace Phantom::Math;
using namespace Phantom::Space;

namespace
{
	// A cell offset (dx, dy, dz) expressed in packed grid-ID space. Valid
	// because IndexedParticle::toGridId() is linear in the cell index --
	// see IndexedParticle::gridStrideY's doc comment.
	constexpr int gridIdOffset(const int dx, const int dy, const int dz)
	{
		return dz * IndexedParticle::gridStrideZ + dy * IndexedParticle::gridStrideY + dx;
	}

	// Start of each forward row's ID window, relative to the reference
	// particle's own grid ID. Each window spans three cells (dx = -1, 0, +1),
	// hence the `+ 2` when scanning. Previously these were four hardcoded
	// literals (1023 / 1047551 / 1048575 / 1049599) with no derivation.
	constexpr int rowStartOffsets[4] = {
		gridIdOffset(-1, +1,  0),
		gridIdOffset(-1, -1, +1),
		gridIdOffset(-1,  0, +1),
		gridIdOffset(-1, +1, +1),
	};

	// Width of a row window in cells, minus one: the scan runs over
	// [rowStart, rowStart + rowSpan] inclusive.
	constexpr int rowSpan = 2;

	// Appends `eachPairs` (one vector per thread, in thread order) onto `dst`
	// with a single resize + parallel per-thread copy, instead of the
	// previous serial `pairs.insert()` per thread (which was O(total pairs)
	// and entirely unparallelized -- a fixed cost dragging down search()'s
	// scaling regardless of thread count, see
	// internal design notes section 11.4/12.1). Each
	// thread's slice lands at the same [start, start+size) offset every run
	// (computed purely from sizes), so the resulting order in `dst` is
	// identical to what the old serial insert loop produced -- required
	// because CSRNeighborList::build() (consumer of getPairs()) walks
	// `pairs` in order when building each particle's neighbor row, and that
	// row order feeds the density/force passes' floating-point summation
	// order. A different order would still be correct but would break
	// FluidDeterminism's bit-exact repeat-run checks
	// (Physics/PhysicsTest/FluidDeterministicTest.cpp).
	void appendMerged(std::vector<ParticlePair>& dst, const std::vector<std::vector<ParticlePair>>& eachPairs, const int threads)
	{
		std::vector<size_t> start(static_cast<size_t>(threads) + 1, 0);
		for (int i = 0; i < threads; ++i) {
			start[static_cast<size_t>(i) + 1] = start[static_cast<size_t>(i)] + eachPairs[static_cast<size_t>(i)].size();
		}

		const size_t oldSize = dst.size();
		dst.resize(oldSize + start[static_cast<size_t>(threads)]);

#pragma omp parallel for
		for (int i = 0; i < threads; ++i) {
			const auto& src = eachPairs[static_cast<size_t>(i)];
			std::copy(src.begin(), src.end(), dst.begin() + static_cast<std::ptrdiff_t>(oldSize + start[static_cast<size_t>(i)]));
		}
	}
}

void IndexedSortBasedSearcher::add(const Vector3df& position)
{
	IndexedParticle ip(position);
	ip.setGridId(effectLength);
	ip.setId(static_cast<int>(iparticles.size()));
	iparticles.push_back(ip);
}

void IndexedSortBasedSearcher::add(const std::vector<Vector3df>& positions)
{
	iparticles.reserve(iparticles.size() + positions.size());
	for (const auto& position : positions) {
		this->add(position);
	}
}

void IndexedSortBasedSearcher::clear()
{
	iparticles.clear();
	pairs.clear();
}

void IndexedSortBasedSearcher::search()
{
	// Replace rather than append: search() used to accumulate onto whatever a
	// previous call had left in `pairs`, so calling it twice silently doubled
	// every pair.
	pairs.clear();

	std::sort(iparticles.begin(), iparticles.end());

	// Chunk count tracks the actual OpenMP thread count instead of a
	// hardcoded "optimization for quad core" value -- the previous constant
	// (8) silently capped this search's parallelism at 8 threads regardless
	// of OMP_NUM_THREADS/hardware (internal design notes
	// section 6.1).
#ifdef _OPENMP
	const int threads = std::max(1, omp_get_max_threads());
#else
	const int threads = 1;
#endif

	std::vector<std::vector<ParticlePair>> eachPairs(threads);

	std::vector<ConstIter> iters;
	iters.reserve(static_cast<size_t>(threads) + 1);
	for (int i = 0; i < threads; ++i) {
		iters.push_back(iparticles.cbegin() + i * iparticles.size() / threads);
	}
	iters.push_back(iparticles.cend());

#pragma omp parallel for
	for (int i = 0; i < threads; ++i) {
		searchSameRow(iters[i], iters[i + 1], eachPairs[i]);
	}

	appendMerged(pairs, eachPairs, threads);

#pragma omp parallel for
	for (int i = 0; i < threads; ++i) {
		searchForwardRows(iters[i], iters[i + 1], eachPairs[i]);
	}

	appendMerged(pairs, eachPairs, threads);
}

void IndexedSortBasedSearcher::collectWindow(const ConstIter xIter, const ConstIter scanBegin, const int maxGridId, std::vector<ParticlePair>& out) const
{
	const auto effectLengthSquared = effectLength * effectLength;
	const auto& centerX = xIter->getPosition();
	const auto scanEnd = iparticles.cend();
	for (auto yIter = scanBegin; yIter != scanEnd && yIter->getGridId() <= maxGridId; ++yIter) {
		if (getDistanceSquared(centerX, yIter->getPosition()) < effectLengthSquared) {
			out.emplace_back(xIter->getId(), yIter->getId());
		}
	}
}

void IndexedSortBasedSearcher::searchSameRow(const ConstIter begin, const ConstIter end, std::vector<ParticlePair>& out) const
{
	// Reuses the vector's capacity across search() calls instead of handing
	// back a freshly allocated one each time.
	out.clear();
	for (auto xIter = begin; xIter != end; ++xIter) {
		// std::next(xIter) skips the particle itself and, because the scan only
		// ever runs forward, keeps each pair from being emitted twice.
		collectWindow(xIter, std::next(xIter), xIter->getGridId() + 1, out);
	}
}

void IndexedSortBasedSearcher::searchForwardRows(const ConstIter begin, const ConstIter end, std::vector<ParticlePair>& out) const
{
	out.clear();

	// One cursor per row, kept across xIter iterations. xIter walks the sorted
	// range in non-decreasing grid ID, so every row's window start can only
	// move forward -- keeping the cursors makes the whole scan linear instead
	// of re-seeking from `begin` for each particle.
	std::array<ConstIter, rowCount> rowCursor;
	rowCursor.fill(begin);

	const auto scanEnd = iparticles.cend();

	for (auto xIter = begin; xIter != end; ++xIter) {
		for (size_t row = 0; row < rowCount; ++row) {
			const auto rowStartId = xIter->getGridId() + rowStartOffsets[row];
			while (rowCursor[row] != scanEnd && rowCursor[row]->getGridId() < rowStartId) {
				++rowCursor[row];
			}
			collectWindow(xIter, rowCursor[row], rowStartId + rowSpan, out);
		}
	}
}
