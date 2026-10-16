#include "SpaceHash.h"

#include <algorithm>
#include <cstdint>

using namespace Phantom::Math;
using namespace Phantom::Space;

namespace {
	// Unsigned 32-bit wraparound: the same bits the old `bitset<32>(index * p)` produced on MSVC (where
	// `long` is 32-bit), but well defined and identical on platforms where `long` is 64-bit.
	constexpr std::uint32_t p1 = 73856093u;
	constexpr std::uint32_t p2 = 19349663u;
	constexpr std::uint32_t p3 = 83492791u;
}

SpaceHash::SpaceHash(const double divideLength, const int tableSize) :
	table(static_cast<size_t>(tableSize < 1 ? 1 : tableSize)),
	divideLength(divideLength)
{
}

bool SpaceHash::tryToIndex(const Vector3df& pos, std::array<int, 3>& index) const
{
	return Validation::isValidCellSize(divideLength) && Validation::tryToCellIndex(pos, divideLength, index);
}

bool SpaceHash::add(const Vector3df& position)
{
	const auto ix = static_cast<int>(positions.size());
	positions.push_back(position);

	std::array<int, 3> index;
	if (!tryToIndex(position, index)) {
		return false;
	}
	table[toHash(index)].push_back(ix);
	return true;
}

std::list<int> SpaceHash::findNeighborIndices(const Vector3df& position)
{
	std::list<int> results;

	std::array<int, 3> index;
	if (!tryToIndex(position, index)) {
		return results;
	}
	for (int i = index[0] - 1; i <= index[0] + 1; ++i) {
		for (int j = index[1] - 1; j <= index[1] + 1; ++j) {
			for (int k = index[2] - 1; k <= index[2] + 1; ++k) {
				std::array<int, 3> index{ i,j,k };
				const auto& hash = toHash(index);
				const auto& indices = table[hash];
				for (const auto i : indices) {
					/*
					const auto ix = toIndex(position);
					if (ix != index) {
						continue;
					}
					*/
					const auto& p = positions[i];
					const double d2 = getDistanceSquared<float>(p, position);
					if (d2 < divideLength * divideLength) {
						results.push_back(i);
					}
				}
			}
		}
	}

	results.sort();
	results.unique();

	return results;
}

/*
std::vector<IParticle*> SpaceHash3d::getParticles(const Vector3df& position)
{
	const auto& index = toIndex(position);
	const auto& hash = toHash(index);
	const auto& points = table[hash];
	std::vector<IParticle*> results;
	for (auto p : points) {
		const auto ix = toIndex(p->getPosition());
		if (ix == index) {
			results.push_back(p);
		}
	}
	return results;
}
*/

/*
bool SpaceHash::isEmpty(const Vector3df& position) const
{
	const auto& index = toIndex(position);
	const auto& hash = toHash(index);
	const auto& points = table[hash];
	for (auto p : points) {
		const auto ix = toIndex(p->getPosition());
		if (ix == index) {
			return false;
		}
	}
	return true;
}
*/

int SpaceHash::toHash(const std::array<int, 3>& index) const
{
	const std::uint32_t value = (static_cast<std::uint32_t>(index[0]) * p1) ^
		(static_cast<std::uint32_t>(index[1]) * p2) ^
		(static_cast<std::uint32_t>(index[2]) * p3);
	return static_cast<int>(value % table.size());
}
