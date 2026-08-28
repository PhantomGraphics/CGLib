#include "CompactSpaceHash.h"

#include <algorithm>
#include <bitset>

using namespace Phantom::Math;
using namespace Phantom::Space;

namespace {
	constexpr long int p1 = 73856093;
	constexpr long int p2 = 19349663;
	constexpr long int p3 = 83492791;
}

CompactSpaceHash::CompactSpaceHash()
{
}

CompactSpaceHash::CompactSpaceHash(const double divideLength, const int tableSize) :
	divideLength(divideLength),
	table(tableSize),
	zCurve()
{
}

CompactSpaceHash::~CompactSpaceHash()
{
	clear();
}

void CompactSpaceHash::setup(const double divideLength, const int tableSize)
{
	clear();
	this->divideLength = divideLength;
	this->table.resize(tableSize);
}

void CompactSpaceHash::clear()
{
	for (auto cell : cells) {
		cell->particleIndices.clear();
		delete cell;
	}
	cells.clear();

	for (auto& t : table) {
		/*
		for (auto& tt : t) {
			tt->particles.clear();
		}
		*/
		t.clear();
	}
	table.clear();

	positions.clear();
}

void CompactSpaceHash::add(const Vector3df& position)
{
	const auto ix = static_cast<int>( this->positions.size() );
	this->positions.push_back(position);

	const auto& index = toIndex(position);
	const auto hashIndex = toHash(index);
	const auto cellId = toZIndex(index);

	auto& cells = table[hashIndex];
	auto iter = std::find_if(cells.begin(), cells.end(), [cellId](CompactSpaceCell* cell) {
		return cell->cellId == cellId;
		});
	if (iter == cells.end()) {
		auto cell = new CompactSpaceCell();
		cell->index = index;
		cell->cellId = cellId;
		cell->particleIndices.reserve(64);
		cell->particleIndices.push_back(ix);
		this->cells.push_back(cell);
		cells.push_back(cell);
	}
	else {
		(*iter)->particleIndices.push_back(ix);
	}
}

void CompactSpaceHash::remove(const int i)
{
	const auto& index = toIndex( positions[i] );
	const auto hashIndex = toHash(index);
	const auto cellId = toZIndex(index);

	auto& cells = table[hashIndex];
	auto iter = std::find_if(cells.begin(), cells.end(), [cellId](CompactSpaceCell* cell) { return cell->cellId == cellId; });
	assert(iter != cells.end());
	auto& ps = (*iter)->particleIndices;
	ps.erase(std::remove(ps.begin(), ps.end(), i), ps.end());
}


std::vector<int> CompactSpaceHash::findNeighborIndices(const int positionIndex)
{
	const auto position = positions[positionIndex]; //particle->getPosition();

	const auto index = toIndex(position);

	std::vector<int> neighbors;
	neighbors.reserve(64);

	for (int i = -1; i <= 1; ++i) {
		for (int j = -1; j <= 1; ++j) {
			for (int k = -1; k <= 1; ++k) {
				std::array<int, 3> ix{ i + index[0],j + index[1],k + index[2] };
				const auto hash = toHash(ix);
				const auto& cells = table[hash];
				const auto cellId = toZIndex(ix);
				auto iter = std::find_if(cells.begin(), cells.end(), [cellId](CompactSpaceCell* cell) {
					return cell->cellId == cellId;
					});
				if (iter == cells.end()) {
					continue;
				}
				const auto& indices = (*iter)->particleIndices;
				for (const auto ix2 : indices) {
					if (positionIndex == ix2) {
						continue;
					}
					const auto position2 = positions[ix2];
					const double d2 = Math::getDistanceSquared(position, position2);
					if (d2 < divideLength * divideLength) {
						neighbors.push_back(ix2);
					}
				}
			}
		}
	}
	return neighbors;
}

std::vector<int> CompactSpaceHash::findNeighborIndices(const Vector3df& position)
{
	const auto index = toIndex(position);

	std::vector<int> neighbors;
	neighbors.reserve(64);

	for (int i = -1; i <= 1; ++i) {
		for (int j = -1; j <= 1; ++j) {
			for (int k = -1; k <= 1; ++k) {
				std::array<int, 3> ix{ i + index[0],j + index[1],k + index[2] };
				const auto hash = toHash(ix);
				const auto& cells = table[hash];
				const auto cellId = toZIndex(ix);
				auto iter = std::find_if(cells.begin(), cells.end(), [cellId](CompactSpaceCell* cell) {
					return cell->cellId == cellId;
					});
				if (iter == cells.end()) {
					continue;
				}
				const auto& particleIndices = (*iter)->particleIndices;
				for (const auto ix2 : particleIndices) {
					const auto p2 = positions[ix2];
					const double d2 = Math::getDistanceSquared(position, p2);
					if (d2 < divideLength * divideLength) {
						neighbors.push_back(ix2);
					}
				}
			}
		}
	}
	return neighbors;
}

std::vector<int> CompactSpaceHash::find(const int index) const
{
	const auto& p = positions[index];
	return find(toIndex(p));
}

std::vector<int> CompactSpaceHash::find(const std::array<int, 3>& index) const
{
	const auto hash = toHash(index);
	const auto& cells = table[hash];
	const auto cellId = toZIndex(index);
	auto iter = std::find_if(cells.begin(), cells.end(), [cellId](CompactSpaceCell* cell) { return cell->cellId == cellId; });
	if (iter == cells.end()) {
		return {};
	}
	return (*iter)->particleIndices;
}

std::array<int, 3> CompactSpaceHash::toIndex(const Vector3df& pos) const
{
	const auto ix = static_cast<int>((pos[0]) / divideLength);
	const auto iy = static_cast<int>((pos[1]) / divideLength);
	const auto iz = static_cast<int>((pos[2]) / divideLength);
	return { ix, iy, iz };
}

Vector3df CompactSpaceHash::toPosition(const std::array<int, 3>& index) const
{
	const auto x = divideLength * index[0];
	const auto y = divideLength * index[1];
	const auto z = divideLength * index[2];
	return Vector3df(x, y, z);
}

unsigned int CompactSpaceHash::toHash(const Vector3df& pos) const
{
	return toHash(toIndex(pos));
}

unsigned int CompactSpaceHash::toHash(const std::array<int, 3>& index) const
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

unsigned int CompactSpaceHash::toZIndex(const std::array<int, 3>& index) const
{
	return zCurve.encode({ (unsigned int)index[0], (unsigned int)index[1], (unsigned int)index[2] });
}

std::array<unsigned int, 3> CompactSpaceHash::fromZIndex(unsigned int index) const
{
	return zCurve.decode(index);
}

bool CompactSpaceHash::isEmpty(const Vector3df& pos) const
{
	const auto pts = find(toIndex(pos));
	return pts.empty();
}