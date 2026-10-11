#include "CompactSpaceHash.h"

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

	size_t bucketCount(const int tableSize) { return static_cast<size_t>(tableSize < 1 ? 1 : tableSize); }
}

CompactSpaceHash::CompactSpaceHash() :
	table(1)
{
}

CompactSpaceHash::CompactSpaceHash(const double divideLength, const int tableSize) :
	table(bucketCount(tableSize)),
	divideLength(divideLength),
	zCurve()
{
}

bool CompactSpaceHash::tryToIndex(const Vector3df& pos, std::array<int, 3>& index) const
{
	return Validation::isValidCellSize(divideLength) && Validation::tryToCellIndex(pos, divideLength, index);
}

CompactSpaceCell* CompactSpaceHash::findCell(const std::array<int, 3>& ix) const
{
	const auto& bucket = table[toHash(ix)];
	const auto iter = std::find_if(bucket.begin(), bucket.end(), [&ix](const CompactSpaceCell* cell) {
		return cell->index == ix;
		});
	return iter == bucket.end() ? nullptr : *iter;
}

CompactSpaceHash::~CompactSpaceHash()
{
	clear();
}

void CompactSpaceHash::setup(const double divideLength, const int tableSize)
{
	clear();
	this->divideLength = divideLength;
	this->table.resize(bucketCount(tableSize));
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
	// The buckets stay allocated (only emptied): table.size() is the modulus of toHash(), so a cleared
	// hash must keep at least one bucket to remain usable.

	positions.clear();
}

bool CompactSpaceHash::add(const Vector3df& position)
{
	const auto ix = static_cast<int>( this->positions.size() );
	this->positions.push_back(position);

	std::array<int, 3> index;
	if (!tryToIndex(position, index)) {
		return false;
	}
	if (auto* cell = findCell(index)) {
		cell->particleIndices.push_back(ix);
		return true;
	}

	auto cell = new CompactSpaceCell();
	cell->index = index;
	cell->cellId = toZIndex(index);
	cell->particleIndices.reserve(64);
	cell->particleIndices.push_back(ix);
	this->cells.push_back(cell);
	table[toHash(index)].push_back(cell);
	return true;
}

bool CompactSpaceHash::remove(const int i)
{
	if (i < 0 || static_cast<size_t>(i) >= positions.size()) {
		return false;
	}
	std::array<int, 3> index;
	if (!tryToIndex(positions[i], index)) {
		return false;
	}
	auto* cell = findCell(index);
	if (cell == nullptr) {
		return false;
	}
	auto& ps = cell->particleIndices;
	const auto last = std::remove(ps.begin(), ps.end(), i);
	const bool found = last != ps.end();
	ps.erase(last, ps.end());
	return found;
}


std::vector<int> CompactSpaceHash::findNeighborIndices(const int positionIndex) const
{
	std::vector<int> neighbors;
	findNeighborIndices(positionIndex, neighbors);
	return neighbors;
}

std::vector<int> CompactSpaceHash::findNeighborIndices(const Vector3df& position) const
{
	std::vector<int> neighbors;
	findNeighborIndices(position, neighbors);
	return neighbors;
}

void CompactSpaceHash::findNeighborIndices(const int positionIndex, std::vector<int>& out) const
{
	out.clear();
	if (positionIndex < 0 || static_cast<size_t>(positionIndex) >= positions.size()) {
		return;
	}
	out.reserve(64);
	forEachNeighbor(positionIndex, [&out](const int neighbor) { out.push_back(neighbor); });
}

void CompactSpaceHash::findNeighborIndices(const Vector3df& position, std::vector<int>& out) const
{
	out.clear();
	out.reserve(64);
	forEachNeighbor(position, [&out](const int neighbor) { out.push_back(neighbor); });
}

std::vector<int> CompactSpaceHash::find(const int index) const
{
	if (index < 0 || static_cast<size_t>(index) >= positions.size()) {
		return {};
	}
	std::array<int, 3> cellIndex;
	if (!tryToIndex(positions[index], cellIndex)) {
		return {};
	}
	return find(cellIndex);
}

std::vector<int> CompactSpaceHash::find(const std::array<int, 3>& index) const
{
	const auto* cell = findCell(index);
	if (cell == nullptr) {
		return {};
	}
	return cell->particleIndices;
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
	const std::uint32_t value = (static_cast<std::uint32_t>(index[0]) * p1) ^
		(static_cast<std::uint32_t>(index[1]) * p2) ^
		(static_cast<std::uint32_t>(index[2]) * p3);
	return static_cast<unsigned int>(value % table.size());
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
	std::array<int, 3> index;
	if (!tryToIndex(pos, index)) {
		return true;
	}
	const auto* cell = findCell(index);
	return cell == nullptr || cell->particleIndices.empty();
}