#include "ZIndexedSearcher.h"

#include "ZOrderCurve3d.h"
#include "SpaceValidation.h"

#include <algorithm>

using namespace Phantom::Math;
using namespace Phantom::Space;

ZIndexedSearcher::ZIndexedSearcher(const float searchRadius, const Vector3df& minPosition) :
	searchRadius(searchRadius),
	minPosition(minPosition)
{}

bool ZIndexedSearcher::tryToIndex(const Vector3df& position, std::array<unsigned int, 3>& index) const
{
	if (!Validation::isValidCellSize(searchRadius) || !Validation::isFinite(minPosition) || !Validation::isFinite(position)) {
		return false;
	}
	// toIndex() is `floor(p / r) + 1` and findNeighbors() also visits index + 1, so index + 1 must stay
	// within the 10-bit Morton range: floor(p / r) <= kMaxAxisIndex - 2.
	const float limit = static_cast<float>(ZOrderCurve3d::kMaxAxisIndex - 2);
	const auto p = position - minPosition;
	for (int axis = 0; axis < 3; ++axis) {
		const float q = p[axis] / searchRadius;
		if (!(q >= 0.0f) || !(q < limit)) {
			return false;
		}
	}
	index = toIndex(position);
	return true;
}

bool ZIndexedSearcher::add(const Vector3df& position)
{
	const int insertionIndex = insertionCount++;
	std::array<unsigned int, 3> ix;
	if (!tryToIndex(position, ix)) {
		return false;
	}
	ZIndexedParticle zip(curve.encode(ix), position);
	zip.index = insertionIndex;
	points.push_back(zip);
	return true;
}

void ZIndexedSearcher::sort()
{
	std::sort(points.begin(), points.end());
}

std::list<int> ZIndexedSearcher::findNeighbors(const Math::Vector3df& position)
{
	std::list<int> results;
	std::array<unsigned int, 3> index;
	if (!tryToIndex(position, index)) {
		return results;
	}

	// Morton codes are monotonic in each axis, so every cell of the 3x3x3 block lies between the codes of
	// its lowest and highest corners (both inclusive -- the upper corner cell is itself a neighbor).
	const ZIndexedParticle rangeStart(curve.encode({ index[0] - 1, index[1] - 1, index[2] - 1 }), Vector3df(0, 0, 0));
	const ZIndexedParticle rangeEnd(curve.encode({ index[0] + 1, index[1] + 1, index[2] + 1 }), Vector3df(0, 0, 0));
	const auto rangeFirst = std::lower_bound(points.begin(), points.end(), rangeStart);
	const auto rangeLast = std::upper_bound(rangeFirst, points.end(), rangeEnd);

	const float radius2 = searchRadius * searchRadius;
	for (unsigned int i = index[0] - 1; i <= index[0] + 1; ++i) {
		for (unsigned int j = index[1] - 1; j <= index[1] + 1; ++j) {
			for (unsigned int k = index[2] - 1; k <= index[2] + 1; ++k) {
				const ZIndexedParticle cell(curve.encode({ i, j, k }), Vector3df(0, 0, 0));
				const auto range = std::equal_range(rangeFirst, rangeLast, cell);
				for (auto iter = range.first; iter != range.second; ++iter) {
					if (::getDistanceSquared(position, iter->getPosition()) < radius2) {
						results.push_back(iter->index);
					}
				}
			}
		}
	}
	return results;
}

std::array<unsigned int, 3> ZIndexedSearcher::toIndex(const Vector3df& position) const
{
	const auto p = position - minPosition;
	const auto ix = static_cast<unsigned int>(p.x / searchRadius) + 1; // avoid negative index.
	const auto iy = static_cast<unsigned int>(p.y / searchRadius) + 1;
	const auto iz = static_cast<unsigned int>(p.z / searchRadius) + 1;

	return { ix, iy, iz };
}