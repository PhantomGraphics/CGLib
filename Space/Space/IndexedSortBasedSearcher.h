#pragma once

#include "CGLib/Util/UnCopyable.h"

#include "IndexedParticle.h"
#include <array>
#include <cstddef>
#include <utility>
#include <vector>

namespace Phantom {
	namespace Space {

using ParticlePair = std::pair<int, int>;

/**
 * @brief Sort-based fixed-radius all-pairs neighbor search.
 *
 * Every added position is bucketed into a cubic cell of side `effectLength` and
 * given a scalar grid ID (IndexedParticle::toGridId(), linear in the 3D cell
 * index). Sorting by that ID puts every cell's occupants in one contiguous run
 * and, more importantly, puts the runs themselves in ID order -- so each of the
 * 13 "forward" cells of a particle's 27-cell neighborhood is a *contiguous
 * window of IDs* that can be found by advancing a cursor rather than by hashing.
 *
 * The search emits each pair once (i < j in sorted order), not twice:
 *  - search over the same (dy = dz = 0) row covers cell offsets dx in {0, +1},
 *    scanning forward from the particle itself, and
 *  - search over the four forward rows covers (dy, dz) in
 *    {(+1, 0), (-1, +1), (0, +1), (+1, +1)}, each with dx in {-1, 0, +1}.
 * Together with the mirrored pairs implied by emitting both directions
 * downstream (see CSRNeighborList), that is the full 27-cell neighborhood.
 *
 * Pairs are distance-filtered: only positions strictly closer than
 * `effectLength` are emitted, so a caller does not need to re-check the radius.
 *
 * Typical use is via CSRNeighborList::build(positions, effectLength), which
 * owns a searcher internally; use this class directly only when the raw pair
 * list itself is what you want.
 *
 * @note Non-copyable. Reusable: search() replaces (does not append to) the
 *       previous pair list, and clear() drops the accumulated positions.
 */
class IndexedSortBasedSearcher : private UnCopyable
{
public:
	explicit IndexedSortBasedSearcher(const float effectLength) :
		effectLength(effectLength)
	{}

	/// Reserves storage for `count` positions, avoiding repeated reallocation in add().
	void reserve(const size_t count) { iparticles.reserve(count); }

	/// Adds one position; its index in the emitted pairs is the insertion order.
	void add(const Math::Vector3df& position);

	/// Adds every position in order -- equivalent to reserve() + add() in a loop.
	void add(const std::vector<Math::Vector3df>& positions);

	/// Drops all added positions and previously found pairs.
	void clear();

	/// Finds every pair closer than effectLength, replacing the previous result.
	void search();

	const std::vector<ParticlePair>& getPairs() const { return pairs; }

private:
	using ConstIter = std::vector<IndexedParticle>::const_iterator;

	/// Number of forward (dy, dz) rows scanned by searchForwardRows().
	static constexpr size_t rowCount = 4;

	/**
	 * @brief Emits pairs (xIter, y) for every y in [scanBegin, end) whose grid
	 *        ID is <= maxGridId and which lies within effectLength of xIter.
	 *
	 * The sorted order guarantees the scan can stop at the first ID past
	 * maxGridId instead of running to the end.
	 */
	void collectWindow(ConstIter xIter, ConstIter scanBegin, int maxGridId, std::vector<ParticlePair>& out) const;

	/// Pairs within the particle's own (dy = dz = 0) row: cell offsets dx in {0, +1}.
	void searchSameRow(ConstIter begin, ConstIter end, std::vector<ParticlePair>& out) const;

	/// Pairs in the four forward (dy, dz) rows, each covering dx in {-1, 0, +1}.
	void searchForwardRows(ConstIter begin, ConstIter end, std::vector<ParticlePair>& out) const;

	std::vector<ParticlePair> pairs;
	std::vector<IndexedParticle> iparticles;
	const float effectLength;
};

	}
}
