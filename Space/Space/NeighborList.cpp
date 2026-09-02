#include "NeighborList.h"

using namespace Phantom::Math;
using namespace Phantom::Space;

void CSRNeighborList::build(const std::vector<Vector3df>& positions, const float effectLength)
{
	if (effectLength <= 0.0f) {
		// IndexedParticle::toIndex() divides by the search radius, so a
		// zero/negative one would bin every position into an undefined cell.
		// Callers that have not set an effect length yet (see
		// internal design notes Phase 5) get an empty
		// neighborhood -- an inert solver rather than undefined behavior.
		this->build(positions.size(), std::vector<ParticlePair>{});
		return;
	}

	IndexedSortBasedSearcher searcher(effectLength);
	searcher.add(positions);
	searcher.search();
	this->build(positions.size(), searcher.getPairs());
}

void CSRNeighborList::build(const size_t particleCount, const std::vector<ParticlePair>& pairs)
{
	offsets.assign(particleCount + 1, 0);
	for (const auto& pair : pairs) {
		++offsets[static_cast<size_t>(pair.first) + 1];
		++offsets[static_cast<size_t>(pair.second) + 1];
	}
	for (size_t i = 0; i < particleCount; ++i) {
		offsets[i + 1] += offsets[i];
	}

	indices.resize(static_cast<size_t>(offsets.back()));

	// Running write cursor per particle, seeded from offsets (not offsets
	// itself -- offsets must stay untouched as the final CSR row boundaries).
	std::vector<int> cursor(offsets.begin(), offsets.end() - 1);
	for (const auto& pair : pairs) {
		indices[static_cast<size_t>(cursor[static_cast<size_t>(pair.first)]++)] = pair.second;
		indices[static_cast<size_t>(cursor[static_cast<size_t>(pair.second)]++)] = pair.first;
	}
}

void CSRNeighborList::clear()
{
	offsets.clear();
	indices.clear();
}
