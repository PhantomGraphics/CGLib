#include "CSRNeighborList.h"
#include "SpaceValidation.h"

#include <algorithm>
#include <limits>

using namespace Phantom::Math;
using namespace Phantom::Space;

void CSRNeighborList::build(const std::vector<Vector3df>& positions, const float effectLength)
{
	if (!Validation::isValidCellSize(effectLength)) {
		// IndexedParticle::toIndex() divides by the search radius, so a
		// zero/negative one would bin every position into an undefined cell.
		// Callers that have not set an effect length yet (see
		// internal design notes Phase 5) get an empty
		// neighborhood -- an inert solver rather than undefined behavior.
		this->build(positions.size(), std::vector<ParticlePair>{});
		return;
	}

	IndexedSortBasedSearcher searcher(effectLength, scratch);
	searcher.add(positions);
	searcher.search();
	this->build(positions.size(), searcher.getPairs());
}

bool CSRNeighborList::tryBuild(const std::vector<Vector3df>& positions, const float effectLength)
{
	constexpr size_t kMaxCount = static_cast<size_t>(std::numeric_limits<int>::max());
	if (!Validation::isValidCellSize(effectLength) || positions.size() >= kMaxCount) {
		clear();
		return false;
	}

	IndexedSortBasedSearcher searcher(effectLength, scratch);
	if (!searcher.add(positions)) {
		clear();
		return false;
	}
	searcher.search();
	// Pairs come from the searcher, so they are valid by construction; only the int offsets can overflow.
	if (searcher.getPairs().size() > kMaxCount / 2) {
		clear();
		return false;
	}
	this->build(positions.size(), searcher.getPairs());
	return true;
}

bool CSRNeighborList::tryBuild(const size_t particleCount, const std::vector<ParticlePair>& pairs)
{
	constexpr size_t kMaxCount = static_cast<size_t>(std::numeric_limits<int>::max());
	if (particleCount >= kMaxCount || pairs.size() > kMaxCount / 2) {
		clear();
		return false;
	}

	auto& normalized = scratch.checkedPairs;
	normalized.clear();
	normalized.reserve(pairs.size());
	for (const auto& pair : pairs) {
		if (pair.first < 0 || pair.second < 0 ||
			static_cast<size_t>(pair.first) >= particleCount || static_cast<size_t>(pair.second) >= particleCount ||
			pair.first == pair.second) {
			clear();
			return false;
		}
		normalized.emplace_back(std::min(pair.first, pair.second), std::max(pair.first, pair.second));
	}
	std::sort(normalized.begin(), normalized.end());
	if (std::adjacent_find(normalized.begin(), normalized.end()) != normalized.end()) {
		clear();
		return false;
	}

	this->build(particleCount, pairs);
	return true;
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
	auto& cursor = scratch.cursor;
	cursor.assign(offsets.begin(), offsets.end() - 1);
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

void CSRNeighborList::releaseScratch()
{
	scratch = Scratch{};
	// Move-assignment of a fresh object frees the old capacity (copy-assignment would not).
}
