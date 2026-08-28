#include "SpaceHash.h"

#include <bitset>
#include <algorithm>

using namespace Phantom::Math;
using namespace Phantom::Space;

namespace {
	constexpr long int p1 = 73856093;
	constexpr long int p2 = 19349663;
	constexpr long int p3 = 83492791;
}

SpaceHash::SpaceHash(const double divideLength, const int tableSize) :
	divideLength(divideLength),
	table(tableSize)
{
}

void SpaceHash::add(const Vector3df& position)
{
	const auto& index = toIndex(position);
	const auto hashIndex = toHash(index);
	const auto ix = static_cast<int>(positions.size());
	table[hashIndex].push_back(ix);
	positions.push_back(position);
}

std::list<int> SpaceHash::findNeighborIndices(const Vector3df& position)
{
	std::list<int> results;

	const auto& index = toIndex(position);
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

std::array<int, 3> SpaceHash::toIndex(const Vector3df& pos) const
{
	const int ix = static_cast<int>((pos[0]) / divideLength);
	const int iy = static_cast<int>((pos[1]) / divideLength);
	const int iz = static_cast<int>((pos[2]) / divideLength);
	return { ix, iy, iz };
}

int SpaceHash::toHash(const Vector3df& pos) const
{
	return toHash(toIndex(pos));
}

int SpaceHash::toHash(const std::array<int, 3>& index) const
{
	std::bitset<32> x = index[0] * p1;
	std::bitset<32> y = index[1] * p2;
	std::bitset<32> z = index[2] * p3;
	//assert(x >= 0);
	//assert(y >= 0);
	//assert(z >= 0);
	const auto value = (x ^ y ^ z).to_ulong();
	return value % table.size();
}